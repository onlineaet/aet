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
 * base on expr.cc
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
#include "internal-fn.h"

#include "mtcsreg.h"
#include "mtcsexpr.h"
#include "mtcstarget.h"
#include "mtcstool.h"
#include "mtcsoptabs.h"
#include "mtcssimplifyrtx.h"
#include "mtcsasm.h"
#include "mtcsvectorbuilder.h"
#include "aet/aetprinttree.h"
#include "aet/aetprintgimple.h"
#include "mtcsprintrtl.h"
#include "../mtcsinfo.h"


static rtx_insn *emit_move_via_integer (MtcsExpr *self,machine_mode mode, rtx x, rtx y, bool force);
//原型 compress_float_constant expr.cc
static rtx_insn *compress_float_constant (MtcsExpr *self,rtx x, rtx y);
//原型 emit_move_multi_word expr.cc
static rtx_insn *emit_move_multi_word (MtcsExpr *self,machine_mode mode, rtx x, rtx y);
//原型 emit_move_ccmode expr.cc
static rtx_insn *emit_move_ccmode (MtcsExpr *self,machine_mode mode, rtx x, rtx y);
//原型 emit_move_change_mode expr.cc
static rtx emit_move_change_mode (MtcsExpr *self,machine_mode new_mode, machine_mode old_mode, rtx x, bool force);
//原型 emit_group_load_1 expr.cc
static void emit_group_load_1 (MtcsExpr *self,rtx *tmps, rtx dst, rtx orig_src, tree type,poly_int64 ssize);
//原型 expand_expr_constant expr.cc
static rtx expand_expr_constant (MtcsExpr *self,tree exp, int defer, enum expand_modifier modifier);
//原型 widest_fixed_size_mode_for_size
static fixed_size_mode widest_fixed_size_mode_for_size (MtcsExpr *self,unsigned int size, by_pieces_operation op);
//原型 alignment_for_piecewise_move expr.cc
static unsigned int alignment_for_piecewise_move (MtcsExpr *self,unsigned int max_pieces, unsigned int align);
//原型 emit_block_move_via_loop expr.cc
static void emit_block_move_via_loop (MtcsExpr *self,rtx x, rtx y, rtx size,unsigned int align, int incr);

/* Return the promoted (inner) mode of SUBREG_PROMOTED_VAR_P subreg X.  */
//原型 subreg_promoted_mode rtl.h
static scalar_int_mode mtcs_subreg_promoted_mode (MtcsMode *mtcsMode,rtx x)
{
  gcc_checking_assert (SUBREG_PROMOTED_VAR_P (x));
  return mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (SUBREG_REG (x)));
}

/* Return the unpromoted (outer) mode of SUBREG_PROMOTED_VAR_P subreg X.  */
//原型 subreg_unpromoted_mode rtl.h
static scalar_int_mode mtcs_subreg_unpromoted_mode (MtcsMode *mtcsMode,rtx x)
{
  gcc_checking_assert (SUBREG_PROMOTED_VAR_P (x));
  return mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (x));
}


static void mtcsExprInit(MtcsExpr *self)
{
    //原型 POINTERS_EXTEND_UNSIGNED emit_block_move_via_oriented_loop 中定义 或 i386.h中定义 POINTERS_EXTEND_UNSIGNED=1
    self->pointersExtendUnsigned=1;
}

/* Used when performing piecewise block operations, holds information
   about one of the memory objects involved.  The member functions
   can be used to generate code for loading from the object and
   updating the address when iterating.  */

class mtcs_pieces_addr
{
  /* The object being referenced, a MEM.  Can be NULL_RTX to indicate
     stack pushes.  */
  rtx m_obj;
  /* The address of the object.  Can differ from that seen in the
     MEM rtx if we copied the address to a register.  */
  rtx m_addr;
  /* Nonzero if the address on the object has an autoincrement already,
     signifies whether that was an increment or decrement.  */
  signed char m_addr_inc;
  /* Nonzero if we intend to use autoinc without the address already
     having autoinc form.  We will insert add insns around each memory
     reference, expecting later passes to form autoinc addressing modes.
     The only supported options are predecrement and postincrement.  */
  signed char m_explicit_inc;
  /* True if we have either of the two possible cases of using
     autoincrement.  */
  bool m_auto;
  /* True if this is an address to be used for load operations rather
     than stores.  */
  bool m_is_load;

  /* Optionally, a function to obtain constants for any given offset into
     the objects, and data associated with it.  */
  by_pieces_constfn m_constfn;
  void *m_cfndata;
  MtcsExpr *self;
public:
  mtcs_pieces_addr (rtx, bool, by_pieces_constfn, void *,MtcsExpr *);
  rtx adjust (fixed_size_mode, HOST_WIDE_INT, by_pieces_prev * = nullptr);
  void increment_address (HOST_WIDE_INT);
  void maybe_predec (HOST_WIDE_INT);
  void maybe_postinc (HOST_WIDE_INT);
  void decide_autoinc (machine_mode, bool, HOST_WIDE_INT);
  int get_addr_inc ()
  {
    return m_addr_inc;
  }
};

/* Initialize a mtcs_pieces_addr structure from an object OBJ.  IS_LOAD is
   true if the operation to be performed on this object is a load
   rather than a store.  For stores, OBJ can be NULL, in which case we
   assume the operation is a stack push.  For loads, the optional
   CONSTFN and its associated CFNDATA can be used in place of the
   memory load.  */

mtcs_pieces_addr::mtcs_pieces_addr (rtx obj, bool is_load, by_pieces_constfn constfn,
              void *cfndata,MtcsExpr *mtcsExpr)
  : m_obj (obj), m_is_load (is_load), m_constfn (constfn), m_cfndata (cfndata)
{
  self=mtcsExpr;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

  m_addr_inc = 0;
  m_auto = false;
  if (obj){
      rtx addr = XEXP (obj, 0);
      rtx_code code = GET_CODE (addr);
      m_addr = addr;
      bool dec = code == PRE_DEC || code == POST_DEC;
      bool inc = code == PRE_INC || code == POST_INC;
      m_auto = inc || dec;
      if (m_auto)
          m_addr_inc = dec ? -1 : 1;

      /* While we have always looked for these codes here, the code
     implementing the memory operation has never handled them.
     Support could be added later if necessary or beneficial.  */
      gcc_assert (code != PRE_INC && code != POST_DEC);
  }else{
      m_addr = NULL_RTX;
      if (!is_load){
          m_auto = true;
          if (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
            m_addr_inc = -1;
          else
            m_addr_inc = 1;
      }else
          gcc_assert (constfn != NULL);
  }
  m_explicit_inc = 0;
  if (constfn)
    gcc_assert (is_load);
}

/* Decide whether to use autoinc for an address involved in a memory op.
   MODE is the mode of the accesses, REVERSE is true if we've decided to
   perform the operation starting from the end, and LEN is the length of
   the operation.  Don't override an earlier decision to set m_auto.  */

void mtcs_pieces_addr::decide_autoinc (machine_mode ARG_UNUSED (mode), bool reverse,
                 HOST_WIDE_INT len)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  if (m_auto || m_obj == NULL_RTX)
    return;

  bool use_predec = (m_is_load
             ? USE_LOAD_PRE_DECREMENT (mode)
             : USE_STORE_PRE_DECREMENT (mode));
  bool use_postinc = (m_is_load
              ? USE_LOAD_POST_INCREMENT (mode)
              : USE_STORE_POST_INCREMENT (mode));
  machine_mode addr_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,m_obj);

  if (use_predec && reverse){
      m_addr = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,addr_mode,
              mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,addr_mode, m_addr, len));
      m_auto = true;
      m_explicit_inc = -1;
  }else if (use_postinc && !reverse){
      m_addr = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,addr_mode, m_addr);
      m_auto = true;
      m_explicit_inc = 1;
  }else if (CONSTANT_P (m_addr))
    m_addr = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,addr_mode, m_addr);
}

/* Adjust the address to refer to the data at OFFSET in MODE.  If we
   are using autoincrement for this address, we don't add the offset,
   but we still modify the MEM's properties.  */

rtx mtcs_pieces_addr::adjust (fixed_size_mode mode, HOST_WIDE_INT offset,by_pieces_prev *prev)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  if (m_constfn)
    /* Pass the previous data to m_constfn.  */
    return m_constfn (m_cfndata, prev, offset, mode);
  if (m_obj == NULL_RTX)
    return NULL_RTX;
  if (m_auto)
    return mtcs_rtl_adjust_automodify_address/*!adjust_automodify_address*/(mtcsRTL,m_obj, mode, m_addr, offset);
  else
    return mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,m_obj, mode, offset);
}

/* Emit an add instruction to increment the address by SIZE.  */

void mtcs_pieces_addr::increment_address (HOST_WIDE_INT size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx amount = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size, GET_MODE (m_addr));
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,mtcs_optabs_gen_add2_insn/*!gen_add2_insn*/(mtcsOptabs,m_addr, amount));
}

/* If we are supposed to decrement the address after each access, emit code
   to do so now.  Increment by SIZE (which has should have the correct sign
   already).  */

void mtcs_pieces_addr::maybe_predec (HOST_WIDE_INT size)
{
  if (m_explicit_inc >= 0)
    return;
  gcc_assert (HAVE_PRE_DECREMENT);
  increment_address (size);
}

/* If we are supposed to decrement the address after each access, emit code
   to do so now.  Increment by SIZE.  */

void mtcs_pieces_addr::maybe_postinc (HOST_WIDE_INT size)
{
  if (m_explicit_inc <= 0)
    return;
  gcc_assert (HAVE_POST_INCREMENT);
  increment_address (size);
}

/* This structure is used by do_op_by_pieces to describe the operation
   to be performed.  */

class op_by_pieces_d
{
 private:
  fixed_size_mode get_usable_mode (fixed_size_mode, unsigned int);
  fixed_size_mode smallest_fixed_size_mode_for_size (unsigned int);

 protected:
  mtcs_pieces_addr m_to, m_from;
  /* Make m_len read-only so that smallest_fixed_size_mode_for_size can
     use it to check the valid mode size.  */
  const unsigned HOST_WIDE_INT m_len;
  HOST_WIDE_INT m_offset;
  unsigned int m_align;
  unsigned int m_max_size;
  bool m_reverse;
  /* True if this is a stack push.  */
  bool m_push;
  /* True if targetm.overlap_op_by_pieces_p () returns true.  */
  bool m_overlap_op_by_pieces;
  /* The type of operation that we're performing.  */
  by_pieces_operation m_op;
  MtcsExpr *self;

  /* Virtual functions, overriden by derived classes for the specific
     operation.  */
  virtual void generate (rtx, rtx, machine_mode) = 0;
  virtual bool prepare_mode (machine_mode, unsigned int) = 0;
  virtual void finish_mode (machine_mode)
  {
  }

 public:
  op_by_pieces_d (unsigned int, rtx, bool, rtx, bool, by_pieces_constfn,
          void *, unsigned HOST_WIDE_INT, unsigned int, bool,
          by_pieces_operation,  MtcsExpr *);
  void run ();
};


/* The constructor for an op_by_pieces_d structure.  We require two
   objects named TO and FROM, which are identified as loads or stores
   by TO_LOAD and FROM_LOAD.  If FROM is a load, the optional FROM_CFN
   and its associated FROM_CFN_DATA can be used to replace loads with
   constant values.  MAX_PIECES describes the maximum number of bytes
   at a time which can be moved efficiently.  LEN describes the length
   of the operation.  */

op_by_pieces_d::op_by_pieces_d (unsigned int max_pieces, rtx to,
                bool to_load, rtx from, bool from_load,
                by_pieces_constfn from_cfn,
                void *from_cfn_data,
                unsigned HOST_WIDE_INT len,
                unsigned int align, bool push,
                by_pieces_operation op,  MtcsExpr *mtcsExpr)
  : m_to (to, to_load, NULL, NULL,mtcsExpr),
    m_from (from, from_load, from_cfn, from_cfn_data,mtcsExpr),
    m_len (len), m_max_size (max_pieces + 1),
    m_push (push), m_op (op)
{
  self=mtcsExpr;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  int toi = m_to.get_addr_inc ();
  int fromi = m_from.get_addr_inc ();
  if (toi >= 0 && fromi >= 0)
    m_reverse = false;
  else if (toi <= 0 && fromi <= 0)
    m_reverse = true;
  else
    gcc_unreachable ();

  m_offset = m_reverse ? len : 0;
  align = MIN (to ? mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,to) : align,
           from ? mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,from) : align);

  /* If copying requires more than two move insns,
     copy addresses to registers (to make displacements shorter)
     and use post-increment if available.  */
  if (mtcs_expr_by_pieces_ninsns/*!by_pieces_ninsns*/(self,len, align, m_max_size, MOVE_BY_PIECES) > 2){
      /* Find the mode of the largest comparison.  */
      fixed_size_mode mode= widest_fixed_size_mode_for_size(self,m_max_size, m_op);
      m_from.decide_autoinc (mode, m_reverse, len);
      m_to.decide_autoinc (mode, m_reverse, len);
  }

  align = alignment_for_piecewise_move(self,mtcs_reg_get_move_max_pieces/*!MOVE_MAX_PIECES*/(mtcsReg), align);
  m_align = align;
  m_overlap_op_by_pieces =mtcsTarget/*!targetm.overlap_op_by_pieces_p*/->overlap_op_by_pieces_p(mtcsTarget);
}
/*PUSHG_P 重新声明在mtcsargs.h */
//#ifdef PUSH_ROUNDING
//#define PUSHG_P(to)  ((to) == nullptr)
//#else
//#define PUSHG_P(to)  false
//#endif

class mtcs_move_by_pieces_d : public op_by_pieces_d
{
  insn_gen_fn m_gen_fun;
  void generate (rtx, rtx, machine_mode) final override;
  bool prepare_mode (machine_mode, unsigned int) final override;

 public:
  mtcs_move_by_pieces_d (rtx to, rtx from, unsigned HOST_WIDE_INT len,
            unsigned int align,MtcsExpr *expr)
    : op_by_pieces_d (mtcs_reg_get_move_max_pieces/*!MOVE_MAX_PIECES*/(
          (mtcs_target_get_reg(((MtcsTarget*)((MtcsMode *) MTCS_GET_MODE_OBJECT(expr))->target)))),
              to, false, from, true, NULL,
              NULL, len, align,
              mtcs_args_is_push_p/*! PUSHG_P (to)*/(
                    (mtcs_target_get_args(((MtcsTarget*)((MtcsMode *) MTCS_GET_MODE_OBJECT(expr))->target))),to),
              MOVE_BY_PIECES,expr)
  {
  }
  rtx finish_retmode (memop_ret);
};

/* Return true if MODE can be used for a set of copies, given an
   alignment ALIGN.  Prepare whatever data is necessary for later
   calls to generate.  */

bool mtcs_move_by_pieces_d::prepare_mode (machine_mode mode, unsigned int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode);
  m_gen_fun = MTCS_GEN_FCN/*!GEN_FCN*/(icode);
  return icode != CODE_FOR_nothing && align >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);
}

/* A callback used when iterating for a compare_by_pieces_operation.
   OP0 and OP1 are the values that have been loaded and should be
   compared in MODE.  If OP0 is NULL, this means we should generate a
   push; otherwise EXTRA_DATA holds a pointer to a pointer to the insn
   gen function that should be used to generate the mode.  */

void mtcs_move_by_pieces_d::generate (rtx op0, rtx op1,machine_mode mode ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
//#ifdef PUSH_ROUNDING //host=1 nvptx=0
//  if (op0 == NULL_RTX)
//    {
//      emit_single_push_insn (mode, op1, NULL);
//      return;
//    }
//#endif
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,m_gen_fun(op0, op1));
}

/* Perform the final adjustment at the end of a string to obtain the
   correct return value for the block operation.
   Return value is based on RETMODE argument.  */

rtx mtcs_move_by_pieces_d::finish_retmode (memop_ret retmode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  gcc_assert (!m_reverse);
  if (retmode == RETURN_END_MINUS_ONE){
      m_to.maybe_postinc (-1);
      --m_offset;
  }
  return m_to.adjust (mtcsMode->modes.M_QImode, m_offset);
}



/* Derived class from op_by_pieces_d, providing support for block move
   operations.  */

class mtcs_store_by_pieces_d : public op_by_pieces_d
{
  insn_gen_fn m_gen_fun;
  void generate (rtx, rtx, machine_mode) final override;
  bool prepare_mode (machine_mode, unsigned int) final override;

 public:
  mtcs_store_by_pieces_d (rtx to, by_pieces_constfn cfn, void *cfn_data,
             unsigned HOST_WIDE_INT len, unsigned int align, by_pieces_operation op,MtcsExpr *expr)
    : op_by_pieces_d (STORE_MAX_PIECES, to, false, NULL_RTX, true, cfn, cfn_data, len, align, false, op,expr)
  {
  }
  rtx finish_retmode (memop_ret);
};

/* Return true if MODE can be used for a set of stores, given an
   alignment ALIGN.  Prepare whatever data is necessary for later
   calls to generate.  */

bool mtcs_store_by_pieces_d::prepare_mode (machine_mode mode, unsigned int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode);
  m_gen_fun = MTCS_GEN_FCN/*!GEN_FCN*/(icode);
  return icode != CODE_FOR_nothing && align >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);
}

/* A callback used when iterating for a store_by_pieces_operation.
   OP0 and OP1 are the values that have been loaded and should be
   compared in MODE.  If OP0 is NULL, this means we should generate a
   push; otherwise EXTRA_DATA holds a pointer to a pointer to the insn
   gen function that should be used to generate the mode.  */

void mtcs_store_by_pieces_d::generate (rtx op0, rtx op1, machine_mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit, m_gen_fun (op0, op1));
}

/* Perform the final adjustment at the end of a string to obtain the
   correct return value for the block operation.
   Return value is based on RETMODE argument.  */

rtx mtcs_store_by_pieces_d::finish_retmode (memop_ret retmode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  gcc_assert (!m_reverse);
  if (retmode == RETURN_END_MINUS_ONE){
      m_to.maybe_postinc (-1);
      --m_offset;
  }
  return m_to.adjust (mtcsMode->modes.M_QImode, m_offset);
}


/* Return TRUE if expression STMT is suitable for replacement.
   Never consider memory loads as replaceable, because those don't ever lead
   into constant expressions.  */
//原型 stmt_is_replaceable_p expr.cc
static bool stmt_is_replaceable_p (gimple *stmt)
{
  if (ssa_is_replaceable_p (stmt)){
      /* Don't move around loads.  */
      if (!gimple_assign_single_p (stmt) || is_gimple_val (gimple_assign_rhs1 (stmt)))
          return true;
  }
  return false;
}


/* Return the defining gimple statement for SSA_NAME NAME if it is an
   assigment and the code of the expresion on the RHS is CODE.  Return
   NULL otherwise.  */
//原型 get_def_for_expr expr.cc
static gimple *get_def_for_expr (tree name, enum tree_code code)
{
  gimple *def_stmt;
  if (TREE_CODE (name) != SSA_NAME)
    return NULL;
  def_stmt = get_gimple_for_ssa_name (name);
  if (!def_stmt || gimple_assign_rhs_code (def_stmt) != code)
    return NULL;
  return def_stmt;
}

/* Returns the number of FIELD_DECLs in TYPE.  */
//原型 fields_length
static int fields_length (const_tree type)
{
  tree t = TYPE_FIELDS (type);
  int count = 0;
  for (; t; t = DECL_CHAIN (t))
    if (TREE_CODE (t) == FIELD_DECL)
      ++count;
  return count;
}

/* Alignment in bits the TARGET of an assignment may be assumed to have.  */
//原型 target_align expr.cc
static unsigned HOST_WIDE_INT target_align (const_tree target)
{
  /* We might have a chain of nested references with intermediate misaligning
     bitfields components, so need to recurse to find out.  */

  unsigned HOST_WIDE_INT this_align, outer_align;

  switch (TREE_CODE (target)){
    case BIT_FIELD_REF:
      return 1;

    case COMPONENT_REF:
      this_align = DECL_ALIGN (TREE_OPERAND (target, 1));
      outer_align = target_align (TREE_OPERAND (target, 0));
      return MIN (this_align, outer_align);

    case ARRAY_REF:
    case ARRAY_RANGE_REF:
      this_align = TYPE_ALIGN (TREE_TYPE (target));
      outer_align = target_align (TREE_OPERAND (target, 0));
      return MIN (this_align, outer_align);

    CASE_CONVERT:
    case NON_LVALUE_EXPR:
    case VIEW_CONVERT_EXPR:
      this_align = TYPE_ALIGN (TREE_TYPE (target));
      outer_align = target_align (TREE_OPERAND (target, 0));
      return MAX (this_align, outer_align);

    default:
      return TYPE_ALIGN (TREE_TYPE (target));
  }
}


/* Similar, except that the alignment requirements of TARGET are
   taken into account.  Assume it is at least as aligned as its
   type, unless it is a COMPONENT_REF in which case the layout of
   the structure gives the alignment.  */
//原型 highest_pow2_factor_for_target expr.cc
static unsigned HOST_WIDE_INT highest_pow2_factor_for_target (const_tree target, const_tree exp)
{
  unsigned HOST_WIDE_INT talign = target_align (target) / BITS_PER_UNIT;
  unsigned HOST_WIDE_INT factor = highest_pow2_factor (exp);

  return MAX (factor, talign);
}

/* Subroutine of above: returns true if OFFSET corresponds to an offset that
   when applied to the address of EXP produces an address known to be
   aligned more than BIGGEST_ALIGNMENT.  */
//原型 is_aligning_offset expr.cc
static bool is_aligning_offset (MtcsExpr *self,const_tree offset, const_tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);

  /* Strip off any conversions.  */
  while (CONVERT_EXPR_P (offset))
    offset = TREE_OPERAND (offset, 0);

  /* We must now have a BIT_AND_EXPR with a constant that is one less than
     power of 2 and which is larger than BIGGEST_ALIGNMENT.  */
  if (TREE_CODE (offset) != BIT_AND_EXPR
      || !tree_fits_uhwi_p (TREE_OPERAND (offset, 1))
      || compare_tree_int (TREE_OPERAND (offset, 1),
            mtcs_align_get_biggest_alignment/*BIGGEST_ALIGNMENT*/(mtcsAlign)/ BITS_PER_UNIT) <= 0
      || !pow2p_hwi (tree_to_uhwi (TREE_OPERAND (offset, 1)) + 1))
    return false;

  /* Look at the first operand of BIT_AND_EXPR and strip any conversion.
     It must be NEGATE_EXPR.  Then strip any more conversions.  */
  offset = TREE_OPERAND (offset, 0);
  while (CONVERT_EXPR_P (offset))
    offset = TREE_OPERAND (offset, 0);

  if (TREE_CODE (offset) != NEGATE_EXPR)
    return false;

  offset = TREE_OPERAND (offset, 0);
  while (CONVERT_EXPR_P (offset))
    offset = TREE_OPERAND (offset, 0);

  /* This must now be the address of EXP.  */
  return TREE_CODE (offset) == ADDR_EXPR && TREE_OPERAND (offset, 0) == exp;
}


/* A utility routine that returns the base of an auto-inc memory, or NULL.  */
//原型 mem_autoinc_base expr.cc
static rtx mem_autoinc_base (rtx mem)
{
  if (MEM_P (mem)){
      rtx addr = XEXP (mem, 0);
      if (GET_RTX_CLASS (GET_CODE (addr)) == RTX_AUTOINC)
          return XEXP (addr, 0);
  }
  return NULL;
}

/* Return a CONST_VECTOR rtx representing vector mask for
   a VECTOR_CST of booleans.  */
//原型 const_vector_mask_from_tree expr.cc
static rtx const_vector_mask_from_tree (MtcsExpr *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  machine_mode mode = TYPE_MODE (TREE_TYPE (exp));
  machine_mode inner = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
  MtcsVectorBuilder/*!rtx_vector_builder*/ builder (mtcsMode,mode, VECTOR_CST_NPATTERNS (exp),
                  VECTOR_CST_NELTS_PER_PATTERN (exp));
  unsigned int count = builder.encoded_nelts ();
  for (unsigned int i = 0; i < count; ++i){
      tree elt = VECTOR_CST_ELT (exp, i);
      gcc_assert (TREE_CODE (elt) == INTEGER_CST);
      if (integer_zerop (elt))
          builder.quick_push (CONST0_RTX (inner));
      else if (integer_onep (elt) || integer_minus_onep (elt))
          builder.quick_push (CONSTM1_RTX (inner));
      else
          gcc_unreachable ();
  }
  return builder.build ();
}

/* Return a CONST_VECTOR rtx for a VECTOR_CST tree.  */
//原型 const_vector_from_tree expr.cc
static rtx const_vector_from_tree(MtcsExpr *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  machine_mode mode = TYPE_MODE (TREE_TYPE (exp));
  if (initializer_zerop (exp))
    return CONST0_RTX (mode);
  if (VECTOR_BOOLEAN_TYPE_P (TREE_TYPE (exp)))
    return const_vector_mask_from_tree(self,exp);
  machine_mode inner = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
  MtcsVectorBuilder/*!rtx_vector_builder*/ builder (mtcsMode,mode, VECTOR_CST_NPATTERNS (exp),
                  VECTOR_CST_NELTS_PER_PATTERN (exp));
  unsigned int count = builder.encoded_nelts ();
  for (unsigned int i = 0; i < count; ++i){
      tree elt = VECTOR_CST_ELT (exp, i);
      if (TREE_CODE (elt) == REAL_CST)
          builder.quick_push (mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,TREE_REAL_CST (elt),inner));
      else if (TREE_CODE (elt) == FIXED_CST)
          builder.quick_push (mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(mtcsRTL,TREE_FIXED_CST (elt),inner));
      else
          builder.quick_push (mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::to_poly_wide (elt),inner));
  }
  return builder.build();
}

/* Return true if EXP contains all zeros.  */
//原型 all_zeros_p expr.cc
static bool all_zeros_p (MtcsExpr *self,const_tree exp)
{
  if (TREE_CODE (exp) == CONSTRUCTOR){
      HOST_WIDE_INT nz_elts, unz_elts, init_elts;
      int complete_p;
      mtcs_expr_categorize_ctor_elements/*!categorize_ctor_elements*/(self,exp, &nz_elts, &unz_elts, &init_elts,&complete_p);
      return nz_elts == 0;
  }
  return initializer_zerop (exp);
}

/* Returns a tree for the size of EXP in bytes.  */
//原型 tree_expr_size expr.cc
static tree tree_expr_size (const_tree exp)
{
  if (DECL_P (exp) && DECL_SIZE_UNIT (exp) != 0)
    return DECL_SIZE_UNIT (exp);
  else
    return size_in_bytes (TREE_TYPE (exp));
}

/* Return true if field F of structure TYPE is a flexible array.  */
//原型 flexible_array_member_p expr.cc
static bool flexible_array_member_p (const_tree f, const_tree type)
{
  const_tree tf;

  tf = TREE_TYPE (f);
  return (DECL_CHAIN (f) == NULL
      && TREE_CODE (tf) == ARRAY_TYPE
      && TYPE_DOMAIN (tf)
      && TYPE_MIN_VALUE (TYPE_DOMAIN (tf))
      && integer_zerop (TYPE_MIN_VALUE (TYPE_DOMAIN (tf)))
      && !TYPE_MAX_VALUE (TYPE_DOMAIN (tf))
      && int_size_in_bytes (type) >= 0);
}

/* If FOR_CTOR_P, return the number of top-level elements that a constructor
   must have in order for it to completely initialize a value of type TYPE.
   Return -1 if the number isn't known.

   If !FOR_CTOR_P, return an estimate of the number of scalars in TYPE.  */
//原型 count_type_elements expr.cc
static HOST_WIDE_INT count_type_elements (const_tree type, bool for_ctor_p)
{
  switch (TREE_CODE (type)){
    case ARRAY_TYPE:
    {
         tree nelts_minus_one;

         nelts_minus_one = array_type_nelts_minus_one (type);
         if (nelts_minus_one && tree_fits_uhwi_p (nelts_minus_one)){
            unsigned HOST_WIDE_INT n;
            n = tree_to_uhwi (nelts_minus_one) + 1;
            if (n == 0 || for_ctor_p)
               return n;
            else
               return n * count_type_elements (TREE_TYPE (type), false);
         }
         return for_ctor_p ? -1 : 1;
      }
    case RECORD_TYPE:
      {
        unsigned HOST_WIDE_INT n;
        tree f;
        n = 0;
        for (f = TYPE_FIELDS (type); f ; f = DECL_CHAIN (f))
          if (TREE_CODE (f) == FIELD_DECL){
              if (!for_ctor_p)
                  n += count_type_elements (TREE_TYPE (f), false);
              else if (!flexible_array_member_p (f, type))
                /* Don't count flexible arrays, which are not supposed
                   to be initialized.  */
                n += 1;
          }
        return n;
      }

    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      {
        tree f;
        HOST_WIDE_INT n, m;
        gcc_assert (!for_ctor_p);
        /* Estimate the number of scalars in each field and pick the
           maximum.  Other estimates would do instead; the idea is simply
           to make sure that the estimate is not sensitive to the ordering
           of the fields.  */
        n = 1;
        for (f = TYPE_FIELDS (type); f ; f = DECL_CHAIN (f))
          if (TREE_CODE (f) == FIELD_DECL){
              m = count_type_elements (TREE_TYPE (f), false);
              /* If the field doesn't span the whole union, add an extra
             scalar for the rest.  */
              if (simple_cst_equal (TYPE_SIZE (TREE_TYPE (f)),TYPE_SIZE (type)) != 1)
                  m++;
              if (n < m)
                  n = m;
          }
        return n;
      }

    case COMPLEX_TYPE:
      return 2;

    case VECTOR_TYPE:
      {
        unsigned HOST_WIDE_INT nelts;
        if (TYPE_VECTOR_SUBPARTS (type).is_constant (&nelts))
          return nelts;
        else
          return -1;
      }

    case INTEGER_TYPE:
    case REAL_TYPE:
    case FIXED_POINT_TYPE:
    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
    case POINTER_TYPE:
    case OFFSET_TYPE:
    case REFERENCE_TYPE:
    case NULLPTR_TYPE:
    case OPAQUE_TYPE:
    case BITINT_TYPE:
      return 1;

    case ERROR_MARK:
      return 0;

    case VOID_TYPE:
    case METHOD_TYPE:
    case FUNCTION_TYPE:
    case LANG_TYPE:
    default:
      gcc_unreachable ();
  }
}


/* Generate several move instructions to clear LEN bytes of block TO.  (A MEM
   rtx with BLKmode).  ALIGN is maximum alignment we can assume.  */
//原型  clear_by_pieces expr.cc
static void clear_by_pieces(MtcsExpr *self,rtx to, unsigned HOST_WIDE_INT len, unsigned int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

   if (len == 0)
      return;
   /* Use builtin_memset_read_str to support vector mode broadcast.  */
   char c = 0;
   BuiltinReadStrData userData={mtcsBuiltins,&c};
   mtcs_store_by_pieces_d data (to,mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/,
                    &userData/*!&c*/, len, align,CLEAR_BY_PIECES,self);
   data.run ();
}

typedef struct _StringCstReadStr
{
  MtcsExpr *mtcsExpr;
  void *data;
}StringCstReadStr;

/* Helper function for store_expr storing of STRING_CST.  */
//原型 string_cst_read_str expr.cc
static rtx string_cst_read_str(void *data, void *, HOST_WIDE_INT offset,
             fixed_size_mode mode)
{
  StringCstReadStr *userData=(StringCstReadStr *)data;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(userData->mtcsExpr);
  tree str = (tree) data;
  gcc_assert (offset >= 0);
  if (offset >= TREE_STRING_LENGTH (str))
    return const0_rtx;

  if ((unsigned HOST_WIDE_INT) offset + mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)
      > (unsigned HOST_WIDE_INT) TREE_STRING_LENGTH (str)){
      char *p = XALLOCAVEC (char, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
      size_t l = TREE_STRING_LENGTH (str) - offset;
      memcpy (p, TREE_STRING_POINTER (str) + offset, l);
      memset (p + l, '\0', mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) - l);
      return c_readstr (p, mode, false);
  }
  return c_readstr (TREE_STRING_POINTER (str) + offset, mode, false);
}

/* Like emit_block_move_via_loop, but choose a suitable INCR based on
   ALIGN and CTZ_SIZE.  */
//原型 emit_block_move_via_sized_loop expr.cc
static void emit_block_move_via_sized_loop (MtcsExpr *self,rtx x, rtx y, rtx size,
                unsigned int align, unsigned int ctz_size)
{
  int incr = align / BITS_PER_UNIT;
  if (CONST_INT_P (size))
    ctz_size = MAX (ctz_size, (unsigned) wi::ctz (UINTVAL (size)));
  if (HOST_WIDE_INT_1U << ctz_size < (unsigned HOST_WIDE_INT) incr)
    incr = HOST_WIDE_INT_1U << ctz_size;
  while (incr > 1 && !mtcs_expr_can_move_by_pieces/*!can_move_by_pieces*/(self,incr, align))
    incr >>= 1;
  gcc_checking_assert (incr);
  return emit_block_move_via_loop(self,x, y, size, align, incr);
}



/* A subroutine of expand_assignment.  Optimize FIELD op= VAL, where
   FIELD is a bitfield.  Returns true if the optimization was successful,
   and there's nothing else to do.  */
//原型 optimize_bitfield_assignment_op expr.cc
static bool optimize_bitfield_assignment_op(MtcsExpr *self,poly_uint64 pbitsize,
                 poly_uint64 pbitpos,   poly_uint64 pbitregion_start,
                 poly_uint64 pbitregion_end, machine_mode mode1, rtx str_rtx, tree to, tree src, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  /* str_mode is not guaranteed to be a scalar type.  */
  machine_mode str_mode = GET_MODE (str_rtx);
  unsigned int str_bitsize;
  tree op0, op1;
  rtx value, result;
  optab binop;
  gimple *srcstmt;
  enum tree_code code;

  unsigned HOST_WIDE_INT bitsize, bitpos, bitregion_start, bitregion_end;
  if (mode1 != VOIDmode
      || !pbitsize.is_constant (&bitsize)
      || !pbitpos.is_constant (&bitpos)
      || !pbitregion_start.is_constant (&bitregion_start)
      || !pbitregion_end.is_constant (&bitregion_end)
      || bitsize >= BITS_PER_WORD
      || !mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,str_mode).is_constant(&str_bitsize)
      || str_bitsize > BITS_PER_WORD
      || TREE_SIDE_EFFECTS (to)
      || TREE_THIS_VOLATILE (to))
    return false;

  STRIP_NOPS (src);
  if (TREE_CODE (src) != SSA_NAME)
    return false;
  if (TREE_CODE (TREE_TYPE (src)) != INTEGER_TYPE)
    return false;

  srcstmt = get_gimple_for_ssa_name (src);
  if (!srcstmt || TREE_CODE_CLASS (gimple_assign_rhs_code (srcstmt)) != tcc_binary)
    return false;

  code = gimple_assign_rhs_code (srcstmt);
  op0 = gimple_assign_rhs1 (srcstmt);

  /* If OP0 is an SSA_NAME, then we want to walk the use-def chain
     to find its initialization.  Hopefully the initialization will
     be from a bitfield load.  */
  if (TREE_CODE (op0) == SSA_NAME){
      gimple *op0stmt = get_gimple_for_ssa_name (op0);
      /* We want to eventually have OP0 be the same as TO, which
     should be a bitfield.  */
      if (!op0stmt || !is_gimple_assign (op0stmt) || gimple_assign_rhs_code (op0stmt) != TREE_CODE (to))
          return false;
      op0 = gimple_assign_rhs1 (op0stmt);
  }

  op1 = gimple_assign_rhs2 (srcstmt);
  if (!operand_equal_p (to, op0, 0))
    return false;

  if (MEM_P (str_rtx)){
      unsigned HOST_WIDE_INT offset1;
      if (str_bitsize == 0 || str_bitsize > BITS_PER_WORD)
          str_bitsize = BITS_PER_WORD;

      scalar_int_mode best_mode;
      if (!mtcs_mode_get_best_mode/*!get_best_mode*/(mtcsMode,bitsize, bitpos, bitregion_start, bitregion_end,
              mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,str_rtx), str_bitsize, false, &best_mode))
          return false;
      str_mode = best_mode;
      str_bitsize =mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,best_mode);

      offset1 = bitpos;
      bitpos %= str_bitsize;
      offset1 = (offset1 - bitpos) / BITS_PER_UNIT;
      str_rtx = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,str_rtx, str_mode, offset1);
  }else if (!REG_P (str_rtx) && GET_CODE (str_rtx) != SUBREG)
    return false;

  /* If the bit field covers the whole REG/MEM, store_field
     will likely generate better code.  */
  if (bitsize >= str_bitsize)
    return false;

  /* We can't handle fields split across multiple entities.  */
  if (bitpos + bitsize > str_bitsize)
    return false;

  if (reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN)
    bitpos = str_bitsize - bitpos - bitsize;

  switch (code){
    case PLUS_EXPR:
    case MINUS_EXPR:
      /* For now, just optimize the case of the topmost bitfield
     where we don't need to do any masking and also
     1 bit bitfields where xor can be used.
     We might win by one instruction for the other bitfields
     too if insv/extv instructions aren't used, so that
     can be added later.  */
      if ((reverse || bitpos + bitsize != str_bitsize) && (bitsize != 1 || TREE_CODE (op1) != INTEGER_CST))
          break;

      value = mtcs_expr_expand_expr/*!expand_expr*/(self,op1, NULL_RTX, str_mode, EXPAND_NORMAL);
      value = mtcs_expr_convert_modes/*!convert_modes*/(self,str_mode,TYPE_MODE (TREE_TYPE (op1)), value,TYPE_UNSIGNED (TREE_TYPE (op1)));

      /* We may be accessing data outside the field, which means
     we can alias adjacent data.  */
      if (MEM_P (str_rtx)){
          str_rtx = shallow_copy_rtx (str_rtx);
          mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,str_rtx, 0);
          mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,str_rtx, 0);
      }

      if (bitsize == 1 && (reverse || bitpos + bitsize != str_bitsize)){
          value = mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,str_mode, value, const1_rtx, NULL);
          binop = xor_optab;
      }else
          binop = code == PLUS_EXPR ? add_optab : sub_optab;

      value = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, str_mode, value, bitpos, NULL_RTX, 1);
      if (reverse)
          value = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,str_mode, value);
      result = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,str_mode, binop, str_rtx,value, str_rtx, 1, OPTAB_WIDEN);
      if (result != str_rtx)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,str_rtx, result);
      return true;

    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
      if (TREE_CODE (op1) != INTEGER_CST)
          break;
      value = mtcs_expr_expand_expr/*!expand_expr*/(self,op1, NULL_RTX, str_mode, EXPAND_NORMAL);
      value = mtcs_expr_convert_modes/*!convert_modes*/(self,str_mode,TYPE_MODE (TREE_TYPE (op1)), value, TYPE_UNSIGNED (TREE_TYPE (op1)));

      /* We may be accessing data outside the field, which means
     we can alias adjacent data.  */
      if (MEM_P (str_rtx)){
          str_rtx = shallow_copy_rtx (str_rtx);
          mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,str_rtx, 0);
          mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,str_rtx, 0);
      }

      binop = code == BIT_IOR_EXPR ? ior_optab : xor_optab;
      if (bitpos + bitsize != str_bitsize){
          rtx mask = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,(HOST_WIDE_INT_1U << bitsize) - 1,str_mode);
          value = mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,str_mode, value, mask, NULL_RTX);
      }
      value = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, str_mode, value, bitpos, NULL_RTX, 1);
      if (reverse)
          value =  mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,str_mode, value);
      result = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,str_mode, binop, str_rtx,value, str_rtx, 1, OPTAB_WIDEN);
      if (result != str_rtx)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,str_rtx, result);
      return true;
    default:
      break;
  }

  return false;
}

/* Like emit_block_move_via_sized_loop, but besides choosing INCR so
   as to ensure safe moves even in case of overlap, output dynamic
   tests to choose between two loops, one moving downwards, another
   moving upwards.  */
//原型 emit_block_move_via_oriented_loop expr.cc
static void emit_block_move_via_oriented_loop (MtcsExpr *self, rtx x, rtx y, rtx size,
                   unsigned int align, unsigned int ctz_size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  int incr = align / BITS_PER_UNIT;
  if (CONST_INT_P (size))
    ctz_size = MAX (ctz_size, (unsigned) wi::ctz (UINTVAL (size)));
  if (HOST_WIDE_INT_1U << ctz_size < (unsigned HOST_WIDE_INT) incr)
    incr = HOST_WIDE_INT_1U << ctz_size;
  while (incr > 1 && !mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,incr, 0).exists ())
    incr >>= 1;

  gcc_checking_assert (incr);

  rtx_code_label *upw_label, *end_label;
  upw_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  end_label =mtcs_rtl_gen_label_rtx(mtcsRTL);

  rtx x_addr = mtcs_expr_force_operand/*!force_operand*/(self,XEXP (x, 0), NULL_RTX);
  rtx y_addr = mtcs_expr_force_operand/*!force_operand*/(self,XEXP (y, 0), NULL_RTX);
  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  machine_mode mode = GET_MODE (x_addr);
  if (mode != GET_MODE (y_addr)){
      scalar_int_mode xmode= mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,
              mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode));
      scalar_int_mode ymode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,
              mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (y_addr)));
      if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,xmode) < mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,ymode))
          mode = ymode;
      else
          mode = xmode;

//#ifndef POINTERS_EXTEND_UNSIGNED //host=1 nvptx=0
 //     const int POINTERS_EXTEND_UNSIGNED = 1;
//#endif
      x_addr = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, GET_MODE (x_addr), x_addr,self->pointersExtendUnsigned/*!POINTERS_EXTEND_UNSIGNED*/);
      y_addr = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, GET_MODE (y_addr), y_addr, self->pointersExtendUnsigned/*!POINTERS_EXTEND_UNSIGNED*/);
  }

  /* Test for overlap: if (x >= y || x + size <= y) goto upw_label.  */
  mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,x_addr, y_addr, GEU, NULL_RTX, mode,
               true, upw_label,profile_probability::guessed_always ().apply_scale (5, 10));
  rtx tmp = mtcs_expr_convert_modes/*!convert_modes*/(self,GET_MODE (x_addr), GET_MODE (size), size, true);
  tmp = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, GET_MODE (x_addr), x_addr, tmp);

  mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,tmp, y_addr, LEU, NULL_RTX, mode,
               true, upw_label, profile_probability::guessed_always ().apply_scale (8, 10));

  emit_block_move_via_loop(self,x, y, size, align, -incr);
  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,end_label);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,upw_label);
  emit_block_move_via_loop(self,x, y, size, align, incr);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,end_label);
}

/* A subroutine of emit_block_move.  Copy the data via an explicit
   loop.  This is used only when libcalls are forbidden, or when
   inlining is required.  INCR is the block size to be copied in each
   loop iteration.  If it is negative, the absolute value is used, and
   the block is copied backwards.  INCR must be a power of two, an
   exact divisor for SIZE and ALIGN, and imply a mode that can be
   safely copied per iteration assuming no overlap.  */
//原型 emit_block_move_via_loop expr.cc
static void emit_block_move_via_loop (MtcsExpr *self,rtx x, rtx y, rtx size,
              unsigned int align, int incr)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  rtx_code_label *cmp_label, *top_label;
  rtx iter, x_addr, y_addr, tmp;
  machine_mode x_addr_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,x);
  machine_mode y_addr_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,y);
  machine_mode iter_mode;

  iter_mode = GET_MODE (size);
  if (iter_mode == VOIDmode)
    iter_mode = word_mode;

  top_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  cmp_label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  iter = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,iter_mode);

  bool downwards = incr < 0;
  rtx iter_init;
  rtx_code iter_cond;
  rtx iter_limit;
  rtx iter_incr;
  machine_mode move_mode;
  if (downwards){
      incr = -incr;
      iter_init = size;
      iter_cond = GEU;
      iter_limit = const0_rtx;
      iter_incr = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,incr);
  }else{
      iter_init = const0_rtx;
      iter_cond = LTU;
      iter_limit = size;
      iter_incr = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,incr);
  }
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,iter, iter_init);

  opt_scalar_int_mode int_move_mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,incr * BITS_PER_UNIT, 1);
  if (!int_move_mode.exists (&move_mode)
      || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_move_mode.require ()) != incr * BITS_PER_UNIT){
      move_mode = mtcsMode->modes.M_BLKmode;
      gcc_checking_assert (mtcs_expr_can_move_by_pieces/*!can_move_by_pieces*/(self,incr, align));
  }

  x_addr = mtcs_expr_force_operand/*!force_operand*/(self,XEXP (x, 0), NULL_RTX);
  y_addr = mtcs_expr_force_operand/*!force_operand*/(self,XEXP (y, 0), NULL_RTX);
  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,cmp_label);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,top_label);

  tmp = mtcs_expr_convert_modes/*!convert_modes*/(self,x_addr_mode, iter_mode, iter, true);
  x_addr = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, x_addr_mode, x_addr, tmp);

  if (x_addr_mode != y_addr_mode)
    tmp = mtcs_expr_convert_modes/*!convert_modes*/(self,y_addr_mode, iter_mode, iter, true);
  y_addr = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, y_addr_mode, y_addr, tmp);

  x = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,x, move_mode, x_addr);
  y = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,y, move_mode, y_addr);

  if (move_mode == mtcsMode->modes.M_BLKmode){
      bool done;
      mtcs_expr_emit_block_move_hints/*!emit_block_move_hints*/(self,x, y, iter_incr, BLOCK_OP_NO_LIBCALL,
                 align, incr, incr, incr, incr,false, &done, false);
      gcc_checking_assert (done);
  }else
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,x, y);

  if (downwards)
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,cmp_label);

  tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,iter_mode, PLUS, iter, iter_incr, iter,
                 true, OPTAB_LIB_WIDEN);
  if (tmp != iter)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,iter, tmp);

  if (!downwards)
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,cmp_label);

  mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,iter, iter_limit, iter_cond, NULL_RTX, iter_mode,
               true, top_label,profile_probability::guessed_always () .apply_scale (9, 10));
}

/* Copy a BLKmode object of TYPE out of a register SRCREG into TARGET.

   This is used on targets that return BLKmode values in registers.  */
//原型 copy_blkmode_from_reg expr.cc
static void copy_blkmode_from_reg (MtcsExpr *self,rtx target, rtx srcreg, tree type)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  unsigned HOST_WIDE_INT bytes = int_size_in_bytes (type);
  rtx src = NULL, dst = NULL;
  unsigned HOST_WIDE_INT bitsize = MIN (TYPE_ALIGN (type), BITS_PER_WORD);
  unsigned HOST_WIDE_INT bitpos, xbitpos, padding_correction = 0;
  /* No current ABI uses variable-sized modes to pass a BLKmnode type.  */
  fixed_size_mode mode = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,GET_MODE (srcreg));
  fixed_size_mode tmode = mtcs_mode_as_a <fixed_size_mode> (mtcsMode,GET_MODE (target));
  fixed_size_mode copy_mode;

  /* BLKmode registers created in the back-end shouldn't have survived.  */
  gcc_assert (mode != mtcsMode->modes.M_BLKmode);

  /* If the structure doesn't take up a whole number of words, see whether
     SRCREG is padded on the left or on the right.  If it's on the left,
     set PADDING_CORRECTION to the number of bits to skip.

     In most ABIs, the structure will be returned at the least end of
     the register, which translates to right padding on little-endian
     targets and left padding on big-endian targets.  The opposite
     holds if the structure is returned at the most significant
     end of the register.  */
  if (bytes % UNITS_PER_WORD != 0
      && (target_calls_return_in_msb/*targetm.calls.return_in_msb*/(mtcsMachine->calls,type)
      ? !BYTES_BIG_ENDIAN: BYTES_BIG_ENDIAN))
    padding_correction= (BITS_PER_WORD - ((bytes % UNITS_PER_WORD) * BITS_PER_UNIT));

  /* We can use a single move if we have an exact mode for the size.  */
  else if (MEM_P (target)
       && (!mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode,
               mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target))
           || mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
       && bytes == mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)){
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,target, mode, 0), srcreg);
      return;
  }

  /* And if we additionally have the same mode for a register.  */
  else if (REG_P (target) && GET_MODE (target) == mode
       && bytes == mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)){
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, srcreg);
     return;
  }

  /* This code assumes srcreg is at least a full word.  If it isn't, copy it
     into a new pseudo which is a full word.  */
  if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) < UNITS_PER_WORD){
      srcreg = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,word_mode, srcreg, TYPE_UNSIGNED (type));
      mode = word_mode;
  }

  /* Copy the structure BITSIZE bits at a time.  If the target lives in
     memory, take care of not reading/writing past its end by selecting
     a copy mode suited to BITSIZE.  This should always be possible given
     how it is computed.

     If the target lives in register, make sure not to select a copy mode
     larger than the mode of the register.

     We could probably emit more efficient code for machines which do not use
     strict alignment, but it doesn't seem worth the effort at the current
     time.  */

  copy_mode = word_mode;
  if (MEM_P (target)){
      opt_scalar_int_mode mem_mode =mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,bitsize, 1);
      if (mem_mode.exists ())
          copy_mode = mem_mode.require ();
  }else if (REG_P (target) && mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,tmode) < BITS_PER_WORD)
    copy_mode = tmode;

  for (bitpos = 0, xbitpos = padding_correction;
       bitpos < bytes * BITS_PER_UNIT;   bitpos += bitsize, xbitpos += bitsize){
      /* We need a new source operand each time xbitpos is on a
     word boundary and when xbitpos == padding_correction
     (the first time through).  */
      if (xbitpos % BITS_PER_WORD == 0 || xbitpos == padding_correction)
          src =mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,srcreg, xbitpos / BITS_PER_WORD, mode);

      /* We need a new destination operand each time bitpos is on
     a word boundary.  */
      if (REG_P (target) && mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,tmode) < BITS_PER_WORD)
          dst = target;
      else if (bitpos % BITS_PER_WORD == 0)
          dst = mtcs_rtl_operand_subword/*!operand_subword*/(mtcsRTL,target, bitpos / BITS_PER_WORD, 1, tmode);

      /* Use xbitpos for the source extraction (right justified) and
     bitpos for the destination store (left justified).  */
      mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,dst, bitsize, bitpos % BITS_PER_WORD, 0, 0, copy_mode,
              mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,src, bitsize,
                      xbitpos % BITS_PER_WORD, 1, NULL_RTX, copy_mode, copy_mode,false, NULL), false, false);
  }
}
/* Return true if we know how to implement OP using vectors of bytes.  */
//原型 can_use_qi_vectors expr.cc
static bool can_use_qi_vectors (by_pieces_operation op)
{
  return (op == COMPARE_BY_PIECES || op == SET_BY_PIECES  || op == CLEAR_BY_PIECES);
}


/* Return true if optabs exists for the mode and certain by pieces
   operations.  */
//原型 by_pieces_mode_supported_p
static bool by_pieces_mode_supported_p(MtcsExpr *self,fixed_size_mode mode, by_pieces_operation op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  if (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode) == CODE_FOR_nothing)
    return false;

  if ((op == SET_BY_PIECES || op == CLEAR_BY_PIECES) && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)
      && mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,vec_duplicate_optab, mode) == CODE_FOR_nothing)
    return false;

  if (op == COMPARE_BY_PIECES  && !mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,EQ, mode, ccp_jump))
    return false;

  return true;
}


/* Return the widest mode that can be used to perform part of an
   operation OP on SIZE bytes.  Try to use QI vector modes where
   possible.  */
//原型 widest_fixed_size_mode_for_size
static fixed_size_mode widest_fixed_size_mode_for_size (MtcsExpr *self,unsigned int size, by_pieces_operation op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  fixed_size_mode result =mtcs_mode_get_narrowest_int_mode/*!NARROWEST_INT_MODE*/(mtcsMode);
  gcc_checking_assert (size > 1);
  /* Use QI vector only if size is wider than a WORD.  */
  if (can_use_qi_vectors (op) && size > UNITS_PER_WORD){
      machine_mode mode;
      fixed_size_mode candidate;
      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_INT)
          if (mtcs_mode_is_a<fixed_size_mode>(mtcsMode,mode, &candidate)
                && mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,candidate) ==mtcsMode->modes.M_QImode){
                if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,candidate) >= size)
                  break;
                if (by_pieces_mode_supported_p(self,candidate, op))
                  result = candidate;
          }

      if (result != mtcs_mode_get_narrowest_int_mode/*!NARROWEST_INT_MODE*/(mtcsMode))
          return result;
  }

  opt_scalar_int_mode tmode;
  scalar_int_mode mode;
  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,tmode, MODE_INT){
      mode = tmode.require ();
      if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode) < size
          && by_pieces_mode_supported_p(self,mode, op))
          result = mode;
  }
  return result;
}


/* If reading SIZE bytes from X will end up reading from
   Y return the number of bytes that overlap.  Return -1
   if there is no overlap or -2 if we can't determine
   (for example when X and Y have different base registers).  */
//原型 memory_load_overlap expr.cc
static int memory_load_overlap (MtcsExpr *self,rtx x, rtx y, HOST_WIDE_INT size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx tmp = mtcs_rtl_plus_constant (mtcsRTL,pMode, x, size);
  rtx sub = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, pMode, tmp, y);
  if (!CONST_INT_P (sub))
    return -2;

  HOST_WIDE_INT val = INTVAL (sub);
  return IN_RANGE (val, 1, size) ? val : -1;
}

/* Return true if EXP contains mostly (3/4) zeros.  */
//原型 mostly_zeros_p expr.cc
static bool mostly_zeros_p (MtcsExpr *self,const_tree exp)
{
  if (TREE_CODE (exp) == CONSTRUCTOR){
      HOST_WIDE_INT nz_elts, unz_elts, init_elts;
      int complete_p;
      mtcs_expr_categorize_ctor_elements/*!categorize_ctor_elements*/(self,exp, &nz_elts, &unz_elts, &init_elts,
                &complete_p);
      return !complete_p || nz_elts < init_elts / 4;
  }
  return initializer_zerop (exp);
}

/* Generate code for computing CONSTRUCTOR EXP.
   An rtx for the computed value is returned.  If AVOID_TEMP_MEM
   is TRUE, instead of creating a temporary variable in memory
   NULL is returned and the caller needs to handle it differently.  */
//原型 expand_constructor expr.cc
static rtx expand_constructor (MtcsExpr *self,tree exp, rtx target, enum expand_modifier modifier,bool avoid_temp_mem)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  tree type = TREE_TYPE (exp);
  machine_mode mode = TYPE_MODE (type);
  mode=mtcs_mode_host2device_by_tree(mtcsMode,type,mode);
  /* Try to avoid creating a temporary at all.  This is possible
     if all of the initializer is zero.
     FIXME: try to handle all [0..255] initializers we can handle
     with memset.  */
  if (TREE_STATIC (exp)
      && !TREE_ADDRESSABLE (exp)
      && target != 0 && mode ==mtcsMode->modes.M_BLKmode
      && all_zeros_p(self,exp)){
      mtcs_expr_clear_storage/*!clear_storage*/(self,target, mtcs_expr_expr_size/*!expr_size*/(self,exp), BLOCK_OP_NORMAL);
      return target;
  }

  /* All elts simple constants => refer to a constant in memory.  But
     if this is a non-BLKmode mode, let it store a field at a time
     since that should make a CONST_INT, CONST_WIDE_INT or
     CONST_DOUBLE when we fold.  Likewise, if we have a target we can
     use, it is best to store directly into the target unless the type
     is large enough that memcpy will be used.  If we are making an
     initializer and all operands are constant, put it in memory as
     well.

     FIXME: Avoid trying to fill vector constructors piece-meal.
     Output them with output_constant_def below unless we're sure
     they're zeros.  This should go away when vector initializers
     are treated like VECTOR_CST instead of arrays.  */
  if ((TREE_STATIC (exp)  && ((mode == mtcsMode->modes.M_BLKmode  && ! (target != 0
          && mtcs_expr_safe_from_p/*!safe_from_p*/(self,target, exp, 1)))
       || TREE_ADDRESSABLE (exp) || (tree_fits_uhwi_p (TYPE_SIZE_UNIT (type))
           && (! mtcs_expr_can_move_by_pieces/*!can_move_by_pieces*/(self,tree_to_uhwi (TYPE_SIZE_UNIT (type)),
            TYPE_ALIGN (type)))  && ! mostly_zeros_p (self,exp))))
      || ((modifier == EXPAND_INITIALIZER || modifier == EXPAND_CONST_ADDRESS)  && TREE_CONSTANT (exp))){

      rtx constructor;
      if (avoid_temp_mem)
          return NULL_RTX;
      constructor = expand_expr_constant(self,exp, 1, modifier);
      if (modifier != EXPAND_CONST_ADDRESS  && modifier != EXPAND_INITIALIZER  && modifier != EXPAND_SUM)
          constructor = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,constructor);
      return constructor;
  }
  /* If the CTOR is available in static storage and not mostly
     zeros and we can move it by pieces prefer to do so since
     that's usually more efficient than performing a series of
     stores from immediates.  */
  if (avoid_temp_mem  && TREE_STATIC (exp)  && TREE_CONSTANT (exp) && tree_fits_uhwi_p (TYPE_SIZE_UNIT (type))
      && mtcs_expr_can_move_by_pieces/*!can_move_by_pieces*/(self,tree_to_uhwi (TYPE_SIZE_UNIT (type)),
                 TYPE_ALIGN (type)) && ! mostly_zeros_p(self,exp))
    return NULL_RTX;

  /* Handle calls that pass values in multiple non-contiguous
     locations.  The Irix 6 ABI has examples of this.  */
  if (target == 0 || ! mtcs_expr_safe_from_p/*!safe_from_p*/(self,target, exp, 1)
      || GET_CODE (target) == PARALLEL || modifier == EXPAND_STACK_PARM
      /* Also make a temporary if the store is to volatile memory, to
     avoid individual accesses to aggregate members.  */
      || (GET_CODE (target) == MEM  && MEM_VOLATILE_P (target) && !TREE_ADDRESSABLE (TREE_TYPE (exp)))){
      if (avoid_temp_mem)
          return NULL_RTX;
      target = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, TREE_ADDRESSABLE (exp), 1);
  }
  mtcs_expr_store_constructor/*!store_constructor*/(self,exp, target, 0, int_expr_size (exp), false);
  return target;
}



/* A helper function for expand_expr_real_2 to be used with a
   misaligned mem_ref TEMP.  Assume an unsigned type if UNSIGNEDP
   is nonzero, with alignment ALIGN in bits.
   Store the value at TARGET if possible (if TARGET is nonzero).
   Regardless of TARGET, we return the rtx for where the value is placed.
   If the result can be stored at TARGET, and ALT_RTL is non-NULL,
   then *ALT_RTL is set to TARGET (before legitimziation).  */
//原型 expand_misaligned_mem_ref expr.cc
static rtx expand_misaligned_mem_ref(MtcsExpr *self,rtx temp, machine_mode mode, int unsignedp,
               unsigned int align, rtx target, rtx *alt_rtl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  enum insn_code icode;
  if ((icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab, mode))
      != CODE_FOR_nothing){
      class expand_operand ops[2];
     n_debug("mtcsexpr.c expand_misaligned_mem_ref 00 mode:%d unsignedp:%d align:%d\n",mode,unsignedp,align);
      /* We've already validated the memory, and we're creating a
     new pseudo destination.  The predicates really can't fail,
     nor can the generator.  */
      create_output_operand (&ops[0], NULL_RTX, mode);
      create_fixed_operand (&ops[1], temp);
      mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 2, ops);
      temp = ops[0].value;
  }else if (mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode, align)){
     n_debug("mtcsexpr.c expand_misaligned_mem_ref 11 mode:%d unsignedp:%d align:%d\n",mode,unsignedp,align);

    temp = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,temp, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode),
                  0, unsignedp, target, mode, mode, false, alt_rtl);
  }
  return temp;
}

/* A subroutine of emit_group_load.  Arguments as for emit_group_load,
   except that values are placed in TMPS[i], and must later be moved
   into corresponding XEXP (XVECEXP (DST, 0, i), 0) element.  */
//原型 emit_group_load_1 expr.cc
static void emit_group_load_1 (MtcsExpr *self,rtx *tmps, rtx dst, rtx orig_src, tree type,poly_int64 ssize)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx src;
  int start, i;
  machine_mode m = GET_MODE (orig_src);
  gcc_assert (GET_CODE (dst) == PARALLEL);
  if (m != VOIDmode  && !mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,m)
      && !MEM_P (orig_src)  && GET_CODE (orig_src) != CONCAT){
      scalar_int_mode imode;
      if (mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,GET_MODE (orig_src)).exists (&imode)){
          src = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,imode);
          mtcs_expr_emit_move_insn(self,gen_lowpart (GET_MODE (orig_src), src), orig_src);
      }else{
          src = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (orig_src), ssize);
          mtcs_expr_emit_move_insn(self,src, orig_src);
      }
      emit_group_load_1 (self,tmps, dst, src, type, ssize);
      return;
  }
  /* Check for a NULL entry, used to indicate that the parameter goes
     both on the stack and in registers.  */
  if (XEXP (XVECEXP (dst, 0, 0), 0))
    start = 0;
  else
    start = 1;
  /* Process the pieces.  */
  for (i = start; i < XVECLEN (dst, 0); i++){
      machine_mode mode = GET_MODE (XEXP (XVECEXP (dst, 0, i), 0));
      poly_int64 bytepos = rtx_to_poly_int64 (XEXP (XVECEXP (dst, 0, i), 1));
      poly_int64 bytelen = mtcs_mode_get_size(mtcsMode,mode);
      poly_int64 shift = 0;

      /* Handle trailing fragments that run over the size of the struct.
     It's the target's responsibility to make sure that the fragment
     cannot be strictly smaller in some cases and strictly larger
     in others.  */
      gcc_checking_assert (ordered_p (bytepos + bytelen, ssize));
      if (known_size_p (ssize) && maybe_gt (bytepos + bytelen, ssize)){
          /* Arrange to shift the fragment to where it belongs.
             extract_bit_field loads to the lsb of the reg.  */
          if (
    #ifdef BLOCK_REG_PADDING
              BLOCK_REG_PADDING (GET_MODE (orig_src), type, i == start)
              == (BYTES_BIG_ENDIAN ? PAD_UPWARD : PAD_DOWNWARD)
    #else
              BYTES_BIG_ENDIAN
    #endif
              )
             shift = (bytelen - (ssize - bytepos)) * BITS_PER_UNIT;
          bytelen = ssize - bytepos;
          gcc_assert (maybe_gt (bytelen, 0));
      }

      /* If we won't be loading directly from memory, protect the real source
     from strange tricks we might play; but make sure that the source can
     be loaded directly into the destination.  */
      src = orig_src;
      if (!MEM_P (orig_src)  && (!REG_P (orig_src) || mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,orig_src))
          && !CONSTANT_P (orig_src)){
          gcc_assert (GET_MODE (orig_src) != VOIDmode);
          src = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (orig_src), orig_src);
      }

      /* Optimize the access just a bit.  */
      if (MEM_P (src)
        && (! mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode,
              mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,src))
          || mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,src) >=mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
        && multiple_p (bytepos * BITS_PER_UNIT, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
        && known_eq (bytelen, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode))){
          tmps[i] = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,tmps[i],
                  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,src, mode, bytepos));
      }else if (mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,mode)
           && GET_MODE (src) == mode&& known_eq (bytelen,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)))
        /* Let emit_move_complex do the bulk of the work.  */
         tmps[i] = src;
      else if (GET_CODE (src) == CONCAT){
          poly_int64 slen = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (src));
          poly_int64 slen0 = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (XEXP (src, 0)));
          unsigned int elt;
          poly_int64 subpos;

          if (can_div_trunc_p (bytepos, slen0, &elt, &subpos)  && known_le (subpos + bytelen, slen0)){
              /* The following assumes that the concatenated objects all
             have the same size.  In this case, a simple calculation
             can be used to determine the object and the bit field
             to be extracted.  */
              tmps[i] = XEXP (src, elt);
              if (maybe_ne (subpos, 0) || maybe_ne (subpos + bytelen, slen0)
                || (!CONSTANT_P (tmps[i]) && (!REG_P (tmps[i]) || GET_MODE (tmps[i]) != mode)))
                tmps[i] = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,tmps[i], bytelen * BITS_PER_UNIT,
                                 subpos * BITS_PER_UNIT,1, NULL_RTX, mode, mode, false,NULL);
          }else{
              rtx mem;
              gcc_assert (known_eq (bytepos, 0));
              mem = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (src), slen);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,mem, src);
              tmps[i] = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,mem, bytelen * BITS_PER_UNIT,
                           0, 1, NULL_RTX, mode, mode, false,   NULL);
          }
      }else if (CONSTANT_P (src) && GET_MODE (dst) != mtcsMode->modes.M_BLKmode && XVECLEN (dst, 0) > 1)
          tmps[i] = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,mode, src, GET_MODE (dst), bytepos);
      else if (CONSTANT_P (src)){
          if (known_eq (bytelen, ssize))
            tmps[i] = src;
          else{
              rtx first, second;
              /* TODO: const_wide_int can have sizes other than this...  */
              gcc_assert (known_eq (2 * bytelen, ssize));
              mtcs_rtlanal_split_double/*!split_double*/(mtcsRtlanal,src, &first, &second);
              if (i)
                  tmps[i] = second;
              else
                  tmps[i] = first;
          }
      }else if (REG_P (src) && GET_MODE (src) == mode)
          tmps[i] = src;
      else
          tmps[i] = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,src, bytelen * BITS_PER_UNIT,
                     bytepos * BITS_PER_UNIT, 1, NULL_RTX,mode, mode, false, NULL);

      if (maybe_ne (shift, 0))
          tmps[i] = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, tmps[i],shift, tmps[i], 0);
  }
}


/* Store the value of EXP (an expression tree)
   into a subfield of TARGET which has mode MODE and occupies
   BITSIZE bits, starting BITPOS bits from the start of TARGET.
   If MODE is VOIDmode, it means that we are storing into a bit-field.

   BITREGION_START is bitpos of the first bitfield in this region.
   BITREGION_END is the bitpos of the ending bitfield in this region.
   These two fields are 0, if the C++ memory model does not apply,
   or we are not interested in keeping track of bitfield regions.

   Always return const0_rtx unless we have something particular to
   return.

   ALIAS_SET is the alias set for the destination.  This value will
   (in general) be different from that for TARGET, since TARGET is a
   reference to the containing structure.

   If NONTEMPORAL is true, try generating a nontemporal store.

   If REVERSE is true, the store is to be done in reverse order.  */
//原型 store_field expr.cc
static rtx store_field(MtcsExpr *self,rtx target, poly_int64 bitsize, poly_int64 bitpos,
         poly_uint64 bitregion_start, poly_uint64 bitregion_end,
         machine_mode mode, tree exp, alias_set_type alias_set, bool nontemporal,  bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  if (TREE_CODE (exp) == ERROR_MARK)
    return const0_rtx;

  /* If we have nothing to store, do nothing unless the expression has
     side-effects.  Don't do that for zero sized addressable lhs of
     calls.  */
  if (known_eq (bitsize, 0)  && (!TREE_ADDRESSABLE (TREE_TYPE (exp))
      || TREE_CODE (exp) != CALL_EXPR))
    return mtcs_expr_expand_expr/*!expand_expr*/(self,exp, const0_rtx, VOIDmode, EXPAND_NORMAL);

  if (GET_CODE (target) == CONCAT){
      /* We're storing into a struct containing a single __complex.  */
      gcc_assert (known_eq (bitpos, 0));
      return mtcs_expr_store_expr/*!store_expr*/(self,exp, target, 0, nontemporal, reverse);
  }

  /* If the structure is in a register or if the component
     is a bit field, we cannot use addressing to access it.
     Use bit-field techniques or SUBREG to store in it.  */

  poly_int64 decl_bitsize;
  if (mode == VOIDmode
      || (mode != mtcsMode->modes.M_BLKmode && ! mtcsReg->hardRegs.x_direct_store/*!direct_store*/[(int) mode]
      && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) != MODE_COMPLEX_INT
      && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) != MODE_COMPLEX_FLOAT)
      || REG_P (target) || GET_CODE (target) == SUBREG
      /* If the field isn't aligned enough to store as an ordinary memref,
     store it as a bit field.  */
      || (mode != mtcsMode->modes.M_BLKmode
      && ((((mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
        || !multiple_p (bitpos, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)))
           && mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,
                 mode, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target)))
          || !multiple_p (bitpos, BITS_PER_UNIT)))
      || (known_size_p (bitsize)  && mode != mtcsMode->modes.M_BLKmode
      && maybe_gt (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode), bitsize))
      /* If the RHS and field are a constant size and the size of the
     RHS isn't the same size as the bitfield, we must use bitfield
     operations.  */
      || (known_size_p (bitsize)   && poly_int_tree_p (TYPE_SIZE (TREE_TYPE (exp)))
      && maybe_ne (wi::to_poly_offset (TYPE_SIZE (TREE_TYPE (exp))), bitsize)
      /* Except for initialization of full bytes from a CONSTRUCTOR, which
         we will handle specially below.  */
      && !(TREE_CODE (exp) == CONSTRUCTOR  && multiple_p (bitsize, BITS_PER_UNIT))
      /* And except for bitwise copying of TREE_ADDRESSABLE types,
         where the FIELD_DECL has the right bitsize, but TREE_TYPE (exp)
         includes some extra padding.  store_expr / expand_expr will in
         that case call get_inner_reference that will have the bitsize
         we check here and thus the block move will not clobber the
         padding that shouldn't be clobbered.  In the future we could
         replace the TREE_ADDRESSABLE check with a check that
         get_base_address needs to live in memory.  */
      && (!TREE_ADDRESSABLE (TREE_TYPE (exp))
          || TREE_CODE (exp) != COMPONENT_REF
          || !multiple_p (bitsize, BITS_PER_UNIT)
          || !multiple_p (bitpos, BITS_PER_UNIT)
          || !poly_int_tree_p (DECL_SIZE (TREE_OPERAND (exp, 1)), &decl_bitsize)
          || maybe_ne (decl_bitsize, bitsize))
      /* A call with an addressable return type and return-slot
         optimization must not need bitfield operations but we must
         pass down the original target.  */
      && (TREE_CODE (exp) != CALL_EXPR || !TREE_ADDRESSABLE (TREE_TYPE (exp))
          || !CALL_EXPR_RETURN_SLOT_OPT (exp)))
      /* If we are expanding a MEM_REF of a non-BLKmode non-addressable
         decl we must use bitfield operations.  */
      || (known_size_p (bitsize) && TREE_CODE (exp) == MEM_REF
      && TREE_CODE (TREE_OPERAND (exp, 0)) == ADDR_EXPR && DECL_P (TREE_OPERAND (TREE_OPERAND (exp, 0), 0))
      && !TREE_ADDRESSABLE (TREE_OPERAND (TREE_OPERAND (exp, 0), 0))
      && DECL_MODE (TREE_OPERAND (TREE_OPERAND (exp, 0), 0)) != mtcsMode->modes.M_BLKmode))
    {
      rtx temp;
      gimple *nop_def;

      /* If EXP is a NOP_EXPR of precision less than its mode, then that
     implies a mask operation.  If the precision is the same size as
     the field we're storing into, that mask is redundant.  This is
     particularly common with bit field assignments generated by the
     C front end.  */
      nop_def = get_def_for_expr (exp, NOP_EXPR);
      if (nop_def){
          tree type = TREE_TYPE (exp);
          machine_mode mode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
          if (INTEGRAL_TYPE_P (type)  && maybe_ne (TYPE_PRECISION (type),
                   mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode/*!TYPE_MODE (type)*/))
                   && known_eq (bitsize, TYPE_PRECISION (type))){
              tree op = gimple_assign_rhs1 (nop_def);
              type = TREE_TYPE (op);
              if (INTEGRAL_TYPE_P (type)
              && known_ge (TYPE_PRECISION (type), bitsize))
            exp = op;
          }
      }

      temp = mtcs_expr_expand_normal/*!expand_normal*/(self,exp);

      /* We don't support variable-sized BLKmode bitfields, since our
     handling of BLKmode is bound up with the ability to break
     things into words.  */
      gcc_assert (mode != mtcsMode->modes.M_BLKmode || bitsize.is_constant ());

      /* Handle calls that return values in multiple non-contiguous locations.
     The Irix 6 ABI has examples of this.  */
      if (GET_CODE (temp) == PARALLEL){
          HOST_WIDE_INT size = int_size_in_bytes (TREE_TYPE (exp));
          machine_mode temp_mode = GET_MODE (temp);
          if (temp_mode == mtcsMode->modes.M_BLKmode || temp_mode == VOIDmode)
            temp_mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,size * BITS_PER_UNIT);
          rtx temp_target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,temp_mode);
          mtcs_expr_emit_group_store/*!emit_group_store*/(self,temp_target, temp, TREE_TYPE (exp), size);
          temp = temp_target;
      }
      /* Handle calls that return BLKmode values in registers.  */
      else if (mode == mtcsMode->modes.M_BLKmode && REG_P (temp) && TREE_CODE (exp) == CALL_EXPR){
          rtx temp_target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (temp));
          copy_blkmode_from_reg(self,temp_target, temp, TREE_TYPE (exp));
          temp = temp_target;
      }

      /* If the value has aggregate type and an integral mode then, if BITSIZE
     is narrower than this mode and this is for big-endian data, we first
     need to put the value into the low-order bits for store_bit_field,
     except when MODE is BLKmode and BITSIZE larger than the word size
     (see the handling of fields larger than a word in store_bit_field).
     Moreover, the field may be not aligned on a byte boundary; in this
     case, if it has reverse storage order, it needs to be accessed as a
     scalar field with reverse storage order and we must first put the
     value into target order.  */
      scalar_int_mode temp_mode;
      if (AGGREGATE_TYPE_P (TREE_TYPE (exp))  && mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,GET_MODE (temp), &temp_mode)){
          HOST_WIDE_INT size = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,temp_mode);
          reverse = TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (exp));
          if (reverse)
            temp = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,temp_mode, temp);
          gcc_checking_assert (known_le (bitsize, size));
          if (maybe_lt (bitsize, size)
              && reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN
              /* Use of to_constant for BLKmode was checked above.  */
              && !(mode == mtcsMode->modes.M_BLKmode && bitsize.to_constant () > BITS_PER_WORD))
            temp = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, temp_mode, temp,size - bitsize, NULL_RTX, 1);
      }

      /* Unless MODE is VOIDmode or BLKmode, convert TEMP to MODE.  */
      if (mode != VOIDmode && mode != mtcsMode->modes.M_BLKmode  && mode != TYPE_MODE (TREE_TYPE (exp)))
          temp = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, TYPE_MODE (TREE_TYPE (exp)), temp, 1);

      /* If the mode of TEMP and TARGET is BLKmode, both must be in memory
     and BITPOS must be aligned on a byte boundary.  If so, we simply do
     a block copy.  Likewise for a BLKmode-like TARGET.  */
      if (GET_MODE (temp) == mtcsMode->modes.M_BLKmode  && (GET_MODE (target) == mtcsMode->modes.M_BLKmode
          || (MEM_P (target)  && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,GET_MODE (target)) == MODE_INT
          && multiple_p (bitpos, BITS_PER_UNIT)  && multiple_p (bitsize, BITS_PER_UNIT)))){
          gcc_assert (MEM_P (target) && MEM_P (temp));
          poly_int64 bytepos = exact_div (bitpos, BITS_PER_UNIT);
          poly_int64 bytesize = bits_to_bytes_round_up (bitsize);
          target = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,target, VOIDmode, bytepos);
          mtcs_expr_emit_block_move/*!emit_block_move*/(self,
                target, temp,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,bytesize, mtcs_mode_get_Pmode(mtcsMode)),BLOCK_OP_NORMAL);
          return const0_rtx;
      }
      /* If the mode of TEMP is still BLKmode and BITSIZE not larger than the
     word size, we need to load the value (see again store_bit_field).  */
      if (GET_MODE (temp) == mtcsMode->modes.M_BLKmode && known_le (bitsize, BITS_PER_WORD)){
          temp_mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,bitsize);
          temp = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,
                temp, bitsize, 0, 1, NULL_RTX, temp_mode,temp_mode, false, NULL);
      }
      /* Store the value in the bitfield.  */
      gcc_checking_assert (known_ge (bitpos, 0));
      mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,
            target, bitsize, bitpos,bitregion_start, bitregion_end,mode, temp, reverse, false);
      return const0_rtx;
  }else{
      /* Now build a reference to just the desired component.  */
      rtx to_rtx = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,target, mode, exact_div (bitpos, BITS_PER_UNIT));
      if (to_rtx == target)
          to_rtx = copy_rtx (to_rtx);
      if (!MEM_KEEP_ALIAS_SET_P (to_rtx) && mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(mtcsRTL,to_rtx) != 0)
          mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,to_rtx, alias_set);

      /* Above we avoided using bitfield operations for storing a CONSTRUCTOR
     into a target smaller than its type; handle that case now.  */
      if (TREE_CODE (exp) == CONSTRUCTOR && known_size_p (bitsize)){
          poly_int64 bytesize = exact_div (bitsize, BITS_PER_UNIT);
          mtcs_expr_store_constructor/*!store_constructor*/(self,exp, to_rtx, 0, bytesize, reverse);
          return to_rtx;
      }
      return mtcs_expr_store_expr/*!store_expr*/(self,exp, to_rtx, 0, nontemporal, reverse);
  }
}

/* Return true if word I of OP lies entirely in the
   undefined bits of a paradoxical subreg.  */
//原型 undefined_operand_subword_p expr.cc
static bool undefined_operand_subword_p (MtcsExpr *self,const_rtx op, int i)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  if (GET_CODE (op) != SUBREG)
    return false;
  machine_mode innermostmode = GET_MODE (SUBREG_REG (op));
  poly_int64 offset = i * UNITS_PER_WORD + mtcs_rtl_subreg_memory_offset_with_rtx/*!subreg_memory_offset*/(mtcsRTL,op);
  return (known_ge (offset, mtcs_mode_get_size(mtcsMode,innermostmode))
      || known_le (offset, -UNITS_PER_WORD));
}

/* Subroutine of above: reduce EXP to the precision of TYPE (in the
   signedness of TYPE), possibly returning the result in TARGET.
   TYPE is known to be a partial integer type.  */
//原型 reduce_to_bit_field_precision expr.cc
static rtx reduce_to_bit_field_precision (MtcsExpr *self,rtx exp, rtx target, tree type)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  //scalar_int_mode mode =mtcs_mode_host2device_scalar_int/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
  scalar_int_mode mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);

  HOST_WIDE_INT prec = TYPE_PRECISION (type);
  gcc_assert ((GET_MODE (exp) == VOIDmode || GET_MODE (exp) == mode)  && (!target || GET_MODE (target) == mode));
  n_debug("mtcsexpr.c reduce_to_bit_field_precision 00 mode:%d prec:%d\n",mode,prec);
  /* For constant values, reduce using wide_int_to_tree. */
  if (poly_int_rtx_p (exp)){
      n_debug("mtcsexpr.c reduce_to_bit_field_precision 11 mode:%d prec:%d\n",mode,prec);

      auto value = mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(exp, mode);
      tree t = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, value);
      return mtcs_expr_expand_expr/*!expand_expr*/(self,t, target, VOIDmode, EXPAND_NORMAL);
  }else if (TYPE_UNSIGNED (type)){
     n_debug("mtcsexpr.c reduce_to_bit_field_precision 22 mode:%d prec:%d\n",mode,prec);

      rtx mask = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::mask (prec, false,
              mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)), mode);
      return mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,mode, exp, mask, target);
  }else{
     n_debug("mtcsexpr.c reduce_to_bit_field_precision 33 mode:%d prec:%d\n",mode,prec);

      int count = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode) - prec;
      exp = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, exp, count, target, 0);
      return mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, exp, count, target, 0);
  }
}

/* A subroutine of emit_move_insn_1.  Generate a move from Y into X.
   MODE is known to be MODE_CC.  Returns the last instruction emitted.  */
//原型 emit_move_ccmode expr.cc
static rtx_insn *emit_move_ccmode (MtcsExpr *self,machine_mode mode, rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx_insn *ret;
  /* Assume all MODE_CC modes are equivalent; if we have movcc, use it.  */
  if (mode != mtcsMode->modes.M_CCmode){
      enum insn_code code = mtcs_opinit_optab_handler(mtcsOpinit,mov_optab, mtcsMode->modes.M_CCmode);
      if (code != CODE_FOR_nothing){
          x = emit_move_change_mode (self,mtcsMode->modes.M_CCmode, mode, x, true);
          y = emit_move_change_mode (self,mtcsMode->modes.M_CCmode, mode, y, true);
          return mtcs_emit_emit_insn(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(code) (x, y));
      }
  }
  /* Otherwise, find the MODE_INT mode of the same width.  */
  ret = emit_move_via_integer (self,mode, x, y, false);
  gcc_assert (ret != NULL);
  return ret;
}

/* A subroutine of emit_move_insn_1.  Generate a move from Y into X.
   MODE is any multi-word or full-word mode that lacks a move_insn
   pattern.  Note that you will get better code if you define such
   patterns, even if they must turn into multiple assembler instructions.  */
//原型 emit_move_multi_word expr.cc
static rtx_insn *emit_move_multi_word (MtcsExpr *self,machine_mode mode, rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);

  rtx_insn *last_insn = 0;
  rtx_insn *seq;
  rtx inner;
  bool need_clobber;
  int i, mode_size;

  /* This function can only handle cases where the number of words is
     known at compile time.  */
  mode_size = mtcs_mode_get_size_poly (mtcsMode,mode).to_constant ();
  gcc_assert (mode_size >= UNITS_PER_WORD);

  /* If X is a push on the stack, do the push now and replace
     X with a reference to the stack pointer.  */
  if (mtcs_preds_push_operand/*!push_operand*/(mtcsPreds,x, mode))
    x = mtcs_expr_emit_move_resolve_push/*!emit_move_resolve_push*/ (self,mode, x);

  /* If we are in reload, see if either operand is a MEM whose address
     is scheduled for replacement.  */
  if (reload_in_progress && MEM_P (x)
      && (inner = mtcs_reload_find_replacement/*!find_replacement*/ (mtcsReload,&XEXP (x, 0))) != XEXP (x, 0))
    x = mtcs_rtl_replace_equiv_address_nv (mtcsRTL,x, inner);
  if (reload_in_progress && MEM_P (y)
      && (inner = mtcs_reload_find_replacement/*!find_replacement*/ (mtcsReload,&XEXP (y, 0))) != XEXP (y, 0))
    y = mtcs_rtl_replace_equiv_address_nv (mtcsRTL,y, inner);

  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  need_clobber = false;
  for (i = 0; i < CEIL (mode_size, UNITS_PER_WORD); i++){
      /* Do not generate code for a move if it would go entirely
     to the non-existing bits of a paradoxical subreg.  */
      if (undefined_operand_subword_p (self,x, i))
          continue;

      rtx xpart = mtcs_rtl_operand_subword (mtcsRTL,x, i, 1, mode);
      rtx ypart;

      /* Do not generate code for a move if it would come entirely
     from the undefined bits of a paradoxical subreg.  */
      if (undefined_operand_subword_p (self,y, i))
          continue;

      ypart = mtcs_rtl_operand_subword (mtcsRTL,y, i, 1, mode);

      /* If we can't get a part of Y, put Y into memory if it is a
     constant.  Otherwise, force it into a register.  Then we must
     be able to get a part of Y.  */
      if (ypart == 0 && CONSTANT_P (y)){
          y = mtcs_explow_use_anchored_address/*!use_anchored_address*/(mtcsExplow,mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,mode, y));
          ypart = mtcs_rtl_operand_subword (mtcsRTL,y, i, 1, mode);
      }else if (ypart == 0)
          ypart = mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,y, i, mode);

      gcc_assert (xpart && ypart);

      need_clobber |= (GET_CODE (xpart) == SUBREG);

      last_insn = mtcs_expr_emit_move_insn(self,xpart, ypart);
  }

  seq = mtcs_rtl_data_get_insns (mtcsRtlData);
  mtcs_emit_end_sequence (mtcsEmit);

  /* Show the output dies here.  This is necessary for SUBREGs
     of pseudos since we cannot track their lifetimes correctly;
     hard regs shouldn't appear here except as return values.
     We never want to emit such a clobber after reload.  */
  if (x != y  && ! (reload_in_progress || reload_completed)   && need_clobber != 0)
      mtcs_emit_emit_clobber/*!emit_clobber*/ (mtcsEmit,x);

  mtcs_emit_emit_insn/*!emit_insn*/ (mtcsEmit,seq);

  return last_insn;
}

/* Return the largest alignment we can use for doing a move (or store)
   of MAX_PIECES.  ALIGN is the largest alignment we could use.  */
//原型 alignment_for_piecewise_move expr.cc
static unsigned int alignment_for_piecewise_move (MtcsExpr *self,unsigned int max_pieces, unsigned int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  scalar_int_mode tmode= mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,max_pieces * BITS_PER_UNIT, 0).require ();
  if (align >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,tmode))
    align =  mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,tmode);
  else{
      scalar_int_mode xmode =mtcs_mode_get_narrowest_int_mode/*!NARROWEST_INT_MODE*/(mtcsMode);
      opt_scalar_int_mode mode_iter;
      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode_iter, MODE_INT){
          tmode = mode_iter.require ();
          if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,tmode) > max_pieces
              || mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,tmode, align))
            break;
          xmode = tmode;
     }
     align = MAX (align,mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,xmode));
  }
  return align;
}


/* If Y is representable exactly in a narrower mode, and the target can
   perform the extension directly from constant or memory, then emit the
   move as an extension.  */
//原型 compress_float_constant expr.cc
static rtx_insn *compress_float_constant (MtcsExpr *self,rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  machine_mode dstmode = GET_MODE (x);
  machine_mode orig_srcmode = GET_MODE (y);
  machine_mode srcmode;
  const REAL_VALUE_TYPE *r;
  int oldcost, newcost;
  bool speed = optimize_insn_for_speed_p ();
  r = CONST_DOUBLE_REAL_VALUE (y);
  if (mtcsTarget->legitimate_constant_p/*!targetm.legitimate_constant_p*/(mtcsTarget,dstmode, y))
    oldcost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,y, orig_srcmode, speed);
  else
    oldcost =  mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,dstmode, y), dstmode, speed);

  MTCS_FOR_EACH_MODE_UNTIL (mtcsMode,srcmode, orig_srcmode){
      enum insn_code ic;
      rtx trunc_y;
      rtx_insn *last_insn;
      /* Skip if the target can't extend this way.  */
      ic = mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,dstmode, srcmode, 0);
      if (ic == CODE_FOR_nothing)
          continue;
      /* Skip if the narrowed value isn't exact.  */
      if (! mtcs_real_exact_real_truncate/*!exact_real_truncate*/(mtcsReal,srcmode, r))
          continue;
      trunc_y = mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,*r, srcmode);
      if (mtcsTarget->legitimate_constant_p/*!targetm.legitimate_constant_p*/(mtcsTarget,srcmode, trunc_y)){
      /* Skip if the target needs extra instructions to perform
         the extension.  */
      if (!mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,ic, 1, trunc_y))
          continue;
      /* This is valid, but may not be cheaper than the original. */
      newcost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/ (mtcsRtlanal,gen_rtx_FLOAT_EXTEND (dstmode, trunc_y),dstmode, speed);
      if (oldcost < newcost)
          continue;
      }else if (float_extend_from_mem[dstmode][srcmode]){
          trunc_y = mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,srcmode, trunc_y);
          /* This is valid, but may not be cheaper than the original. */
          newcost = mtcs_rtlanal_set_src_cost/*!set_src_cost*/(mtcsRtlanal,gen_rtx_FLOAT_EXTEND (dstmode, trunc_y),dstmode, speed);
          if (oldcost < newcost)
            continue;
          trunc_y = mtcs_explow_validize_mem(mtcsExplow,trunc_y);
      }else
          continue;
      /* For CSE's benefit, force the compressed constant pool entry
     into a new pseudo.  This constant may be used in different modes,
     and if not, combine will put things back together for us.  */
      trunc_y = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,srcmode, trunc_y);
      /* If x is a hard register, perform the extension into a pseudo,
     so that e.g. stack realignment code is aware of it.  */
      rtx target = x;
      if (REG_P (x) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/ (mtcsReg,x))
          target = mtcs_emit_gen_reg_rtx(mtcsEmit,dstmode);
      mtcs_optabs_emit_unop_insn (mtcsOptabs,ic, target, trunc_y, UNKNOWN);
      last_insn = mtcs_rtl_data_get_last_insn (mtcsRtlData);
      if (REG_P (target))
         mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,last_insn, REG_EQUAL, y);
      if (target != x)
          return mtcs_expr_emit_move_insn(self,x, target);
      return last_insn;
  }
  return NULL;
}

/* Like convert_move, but deals only with scalar modes.  */
static void convert_mode_scalar (MtcsExpr *self,rtx to, rtx from, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);

  /* Both modes should be scalar types.  */
  scalar_mode from_mode = mtcs_mode_as_a <scalar_mode> (mtcsMode,GET_MODE (from));
  scalar_mode to_mode = mtcs_mode_as_a <scalar_mode> (mtcsMode,GET_MODE (to));
  bool to_real = mtcs_mode_is_scalar_float_p(mtcsMode,to_mode);
  bool from_real = mtcs_mode_is_scalar_float_p(mtcsMode,from_mode);
  enum insn_code code;
  rtx libcall;
  gcc_assert (to_real == from_real);
  /* rtx code for making an equivalent value.  */
  enum rtx_code equiv_code = (unsignedp < 0 ? UNKNOWN: (unsignedp ? ZERO_EXTEND : SIGN_EXTEND));

   auto acceptable_same_precision_modes
   = [] (scalar_mode from_mode, scalar_mode to_mode,MtcsMode *mtcsMode) -> bool
   {
      if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,from_mode)
      != mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,to_mode))
         return true;

      /* arm_bfloat_half_format <-> ieee_half_format */
      if ((mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,from_mode) == &arm_bfloat_half_format
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,to_mode) == &ieee_half_format)
      || (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,to_mode) == &arm_bfloat_half_format
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,from_mode) == &ieee_half_format))
         return true;

      /* ibm_extended_format <-> ieee_quad_format */
      if ((mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,from_mode) == &ibm_extended_format
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,to_mode) == &ieee_quad_format)
      || (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,from_mode) == &ieee_quad_format
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,to_mode) == &ibm_extended_format))
         return true;

      return false;
   };

  n_debug("mtcsexpr.c convert_mode_scalar 00 to_real:%d from_real:%d\n",to_real,from_real);
  if (to_real){
      rtx value;
      rtx_insn *insns;
      convert_optab tab;
      gcc_assert ((mtcs_mode_get_precision(mtcsMode,from_mode)
           != mtcs_mode_get_precision(mtcsMode,to_mode))
          || (mtcs_mode_is_decimal_float_p(mtcsMode,from_mode)
              != mtcs_mode_is_decimal_float_p(mtcsMode,to_mode))
          || (mtcs_mode_get_real_format(mtcsMode,from_mode) == &arm_bfloat_half_format
              && mtcs_mode_get_real_format(mtcsMode,to_mode) == &ieee_half_format)
          || (mtcs_mode_get_real_format(mtcsMode,to_mode) == &arm_bfloat_half_format
              && mtcs_mode_get_real_format(mtcsMode,from_mode) == &ieee_half_format));

      if (mtcs_mode_get_precision(mtcsMode,from_mode) == mtcs_mode_get_precision(mtcsMode,to_mode))
        /* Conversion between decimal float and binary float, same size.  */
        tab = mtcs_mode_is_decimal_float_p(mtcsMode,from_mode) ? trunc_optab : sext_optab;
      else if (mtcs_mode_get_precision(mtcsMode,from_mode) < mtcs_mode_get_precision(mtcsMode,to_mode))
          tab = sext_optab;
      else
          tab = trunc_optab;
      /* Try converting directly if the insn is supported.  */
      code = mtcs_opinit_convert_optab_handler(mtcsOpinit,tab, to_mode, from_mode);
      if (code != CODE_FOR_nothing){
          mtcs_optabs_emit_unop_insn (mtcsOptabs,code, to, from,tab == sext_optab ? FLOAT_EXTEND : FLOAT_TRUNCATE);
          return;
      }
#ifdef HAVE_SFmode
      if (mtcs_mode_get_real_format(mtcsMode,from_mode) == &arm_bfloat_half_format
              && mtcs_mode_get_real_format(mtcsMode,mtcsMode->modes.M_SFmode) == &ieee_single_format){
          if (mtcs_mode_get_precision(mtcsMode,to_mode) > mtcs_mode_get_precision(mtcsMode,mtcsMode->modes.M_SFmode)){
              /* To cut down on libgcc size, implement
             BFmode -> {DF,XF,TF}mode conversions by
             BFmode -> SFmode -> {DF,XF,TF}mode conversions.  */
              rtx temp = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->modes.M_SFmode);
              convert_mode_scalar (self,temp, from, unsignedp);
              convert_mode_scalar (self,to, temp, unsignedp);
              return;
          }
          if (mtcs_mode_get_real_format(mtcsMode,to_mode) == &ieee_half_format){
              /* Similarly, implement BFmode -> HFmode as
             BFmode -> SFmode -> HFmode conversion where SFmode
             has superset of BFmode values.  We don't need
             to handle sNaNs by raising exception and turning
             it into qNaN though, as that can be done in the
             SFmode -> HFmode conversion too.  */
              rtx temp = mtcs_emit_gen_reg_rtx(mtcsEmit,mtcsMode->modes.M_SFmode);
              int save_flag_finite_math_only = flag_finite_math_only;
              flag_finite_math_only = true;
              convert_mode_scalar (self,temp, from, unsignedp);
              flag_finite_math_only = save_flag_finite_math_only;
              convert_mode_scalar (self,to, temp, unsignedp);
              return;
          }
          if (to_mode == mtcsMode->modes.M_SFmode && !mtcs_mode_honor_nans/*!HONOR_NANS*/(mtcsMode,from_mode)
                  && !mtcs_mode_honor_nans/*!HONOR_NANS*/ (mtcsMode,to_mode) && optimize_insn_for_speed_p ()){
              /* If we don't expect sNaNs, for BFmode -> SFmode we can just
               shift the bits up.  */
              machine_mode fromi_mode, toi_mode;
              if (mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,from_mode),0).exists (&fromi_mode)
                      && mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,to_mode),0).exists (&toi_mode)){
                  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
                  rtx fromi = mtcs_simplify_rtx_lowpart_subreg(mtcsSimplifyRtx,fromi_mode, from, from_mode);
                  rtx tof = NULL_RTX;
                  if (fromi){
                      rtx toi;
                      if (GET_MODE (fromi) == VOIDmode)
                          toi = mtcs_simplify_rtx_unary_operation/*!simplify_unary_operation*/(mtcsSimplifyRtx,
                                ZERO_EXTEND, toi_mode,fromi, fromi_mode);
                      else{
                          toi = mtcs_emit_gen_reg_rtx(mtcsEmit,toi_mode);
                          convert_mode_scalar (self,toi, fromi, 1);
                      }
                      toi = mtcs_expmed_maybe_expand_shift(mtcsExpmed,LSHIFT_EXPR, toi_mode, toi,
                                  mtcs_mode_get_precision(mtcsMode,to_mode)
                                  - mtcs_mode_get_precision(mtcsMode,from_mode),
                                  NULL_RTX, 1);
                      if (toi){
                          tof = mtcs_simplify_rtx_lowpart_subreg(mtcsSimplifyRtx,to_mode, toi, toi_mode);
                          if (tof)
                            mtcs_expr_emit_move_insn(self,to, tof);
                      }
                  }
                  insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
                  mtcs_emit_end_sequence(mtcsEmit);
                  if (tof){
                      mtcs_emit_emit_insn(mtcsEmit,insns);
                      return;
                  }
              }
          }
      }
      if (mtcs_mode_get_real_format(mtcsMode,from_mode) == &ieee_single_format
          && mtcs_mode_get_real_format(mtcsMode,to_mode) == &arm_bfloat_half_format
          && !mtcs_mode_honor_nans (mtcsMode,from_mode)
          && !mtcs_mode_honor_nans (mtcsMode,to_mode)
          && !flag_rounding_math
          && optimize_insn_for_speed_p ()){
          /* If we don't expect qNaNs nor sNaNs and can assume rounding
             to nearest, we can expand the conversion inline as
             (fromi + 0x7fff + ((fromi >> 16) & 1)) >> 16.  */
          machine_mode fromi_mode, toi_mode;
          if (mtcs_mode_int_mode_for_size(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,from_mode),0).exists (&fromi_mode)
              && mtcs_mode_int_mode_for_size(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,to_mode),0).exists (&toi_mode)){
              mtcs_emit_start_sequence (mtcsEmit);
              rtx fromi = mtcs_simplify_rtx_lowpart_subreg/*lowpart_subreg*/ (mtcsSimplifyRtx,fromi_mode, from, from_mode);
              rtx tof = NULL_RTX;
              do{
                  if (!fromi)
                    break;
                  int shift = (mtcs_mode_get_precision(mtcsMode,from_mode)- mtcs_mode_get_precision(mtcsMode,to_mode));
                  rtx temp1= mtcs_expmed_maybe_expand_shift(mtcsExpmed,RSHIFT_EXPR, fromi_mode, fromi,shift, NULL_RTX, 1);
                  if (!temp1)
                    break;
                  rtx temp2 = mtcs_optabs_expand_binop(mtcsOptabs,fromi_mode, and_optab, temp1, const1_rtx,NULL_RTX, 1, OPTAB_DIRECT);
                  if (!temp2)
                    break;
                  rtx temp3 = mtcs_optabs_expand_binop(mtcsOptabs,fromi_mode, add_optab, fromi,
                          mtcs_rtl_gen_int_mode (mtcsRTL,(HOST_WIDE_INT_1U << (shift - 1)) - 1,fromi_mode), NULL_RTX,1, OPTAB_DIRECT);
                  if (!temp3)
                    break;
                  rtx temp4 = mtcs_optabs_expand_binop(mtcsOptabs,fromi_mode, add_optab, temp3, temp2, NULL_RTX, 1, OPTAB_DIRECT);
                  if (!temp4)
                    break;
                  rtx temp5 = mtcs_expmed_maybe_expand_shift(mtcsExpmed,RSHIFT_EXPR, fromi_mode,temp4, shift, NULL_RTX, 1);
                  if (!temp5)
                    break;
                  rtx temp6 = mtcs_simplify_rtx_lowpart_subreg (mtcsSimplifyRtx,toi_mode, temp5, fromi_mode);
                  if (!temp6)
                    break;
                  tof = mtcs_simplify_rtx_lowpart_subreg (mtcsSimplifyRtx,to_mode, mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,toi_mode, temp6),toi_mode);
                  if (tof)
                    mtcs_expr_emit_move_insn(self,to, tof);
              }while (0);
              insns =mtcs_rtl_data_get_insns (mtcsRtlData);
              mtcs_emit_end_sequence (mtcsEmit);
              if (tof){
                  mtcs_emit_emit_insn(mtcsEmit,insns);
                  return;
              }
          }
      }
#endif

      n_debug("mtcsexpr.c convert_mode_scalar 11 tomode:%d frommode:%d\n",to_mode,from_mode);

      /* Otherwise use a libcall.  */
      libcall = mtcs_libfuncs_convert_optab_libfunc/*!convert_optab_libfunc*/(mtcsLibfuncs,tab, to_mode, from_mode);
      /* Is this conversion implemented yet?  */
      gcc_assert (libcall);
      mtcs_emit_start_sequence (mtcsEmit);
      value = mtcs_calls_emit_library_call_value/*!emit_library_call_value*/(mtcsCalls,
            libcall, NULL_RTX, LCT_CONST, to_mode,from, from_mode);
      insns = mtcs_rtl_data_get_insns (mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);
      mtcs_optabs_emit_libcall_block (mtcsOptabs,insns, to, value,
              tab == trunc_optab ? gen_rtx_FLOAT_TRUNCATE (to_mode,from) : gen_rtx_FLOAT_EXTEND (to_mode, from));
      return;
  }
  /* Handle pointer conversion.  */         /* SPEE 900220.  */
  /* If the target has a converter from FROM_MODE to TO_MODE, use it.  */
  {
    convert_optab ctab;
    if (mtcs_mode_get_precision(mtcsMode,from_mode) > mtcs_mode_get_precision(mtcsMode,to_mode))
      ctab = trunc_optab;
    else if (unsignedp)
      ctab = zext_optab;
    else
      ctab = sext_optab;
    n_debug("mtcsexpr.c convert_mode_scalar 22 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode,ctab);

    if (mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,ctab, to_mode, from_mode)!= CODE_FOR_nothing){
        mtcs_optabs_emit_unop_insn (mtcsOptabs,mtcs_opinit_convert_optab_handler(mtcsOpinit,ctab, to_mode, from_mode),
                to, from, UNKNOWN);
        n_debug("mtcsexpr.c convert_mode_scalar 22aa 返回 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode,ctab);

        return;
    }
  }
  n_debug("mtcsexpr.c convert_mode_scalar 33 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Targets are expected to provide conversion insns between PxImode and
     xImode for all MODE_PARTIAL_INT modes they use, but no others.  */
  if (mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,to_mode) == MODE_PARTIAL_INT) {
      scalar_int_mode full_mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,to_mode));
      gcc_assert (mtcs_opinit_convert_optab_handler(mtcsOpinit,trunc_optab, to_mode, full_mode)!= CODE_FOR_nothing);
      if (full_mode != from_mode)
          from = mtcs_expr_convert_to_mode(self,full_mode, from, unsignedp);
      mtcs_optabs_emit_unop_insn (mtcsOptabs,mtcs_opinit_convert_optab_handler(mtcsOpinit,trunc_optab, to_mode, full_mode),
              to, from, UNKNOWN);
      return;
  }
  n_debug("mtcsexpr.c convert_mode_scalar 44 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  if (mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,from_mode) == MODE_PARTIAL_INT){
      rtx new_from;
      scalar_int_mode full_mode= mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,mtcs_mode_get_bitsize (mtcsMode,from_mode));
      convert_optab ctab = unsignedp ? zext_optab : sext_optab;
      enum insn_code icode;
      icode = mtcs_opinit_convert_optab_handler(mtcsOpinit,ctab, full_mode, from_mode);
      gcc_assert (icode != CODE_FOR_nothing);
      if (to_mode == full_mode){
          mtcs_optabs_emit_unop_insn (mtcsOptabs,icode, to, from, UNKNOWN);
          return;
      }
      new_from = mtcs_emit_gen_reg_rtx(mtcsEmit,full_mode);
      mtcs_optabs_emit_unop_insn (mtcsOptabs,icode, new_from, from, UNKNOWN);
      /* else proceed to integer conversions below.  */
      from_mode = full_mode;
      from = new_from;
  }
  n_debug("mtcsexpr.c convert_mode_scalar 55 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

   /* Make sure both are fixed-point modes or both are not.  */
   gcc_assert (mtcs_mode_is_all_scalar_fixed_point_p (mtcsMode,from_mode) ==mtcs_mode_is_all_scalar_fixed_point_p (mtcsMode,to_mode));
   if (mtcs_mode_is_all_scalar_fixed_point_p (mtcsMode,from_mode)){
      /* If we widen from_mode to to_mode and they are in the same class,
     we won't saturate the result.
     Otherwise, always saturate the result to play safe.  */
      if (mtcs_mode_get_class(mtcsMode,from_mode) == mtcs_mode_get_class (mtcsMode,to_mode)
        && mtcs_mode_get_size (mtcsMode,from_mode) < mtcs_mode_get_size (mtcsMode,to_mode))
          mtcs_optabs_expand_fixed_convert/*!expand_fixed_convert*/(mtcsOptabs,to, from, 0, 0);
      else
          mtcs_optabs_expand_fixed_convert/*!expand_fixed_convert*/(mtcsOptabs,to, from, 0, 1);
      return;
  }
   n_debug("mtcsexpr.c convert_mode_scalar 66 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Now both modes are integers.  */

  /* Handle expanding beyond a word.  */
  if (mtcs_mode_get_precision(mtcsMode,from_mode) < mtcs_mode_get_precision(mtcsMode,to_mode)
      && mtcs_mode_get_precision(mtcsMode,to_mode) > BITS_PER_WORD){
      rtx_insn *insns;
      rtx lowpart;
      rtx fill_value;
      rtx lowfrom;
      int i;
      scalar_mode lowpart_mode;
      int nwords = CEIL (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,to_mode), UNITS_PER_WORD);
      /* Try converting directly if the insn is supported.  */
      if ((code = mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,to_mode, from_mode, unsignedp))!= CODE_FOR_nothing){
          /* If FROM is a SUBREG, put it into a register.  Do this
             so that we always generate the same set of insns for
             better cse'ing; if an intermediate assignment occurred,
             we won't be doing the operation directly on the SUBREG.  */
          if (optimize > 0 && GET_CODE (from) == SUBREG)
            from = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,from_mode, from);
          mtcs_optabs_emit_unop_insn (mtcsOptabs,code, to, from, equiv_code);
          return;
      }
      /* Next, try converting via full word.  */
      else if (mtcs_mode_get_precision(mtcsMode,from_mode) < BITS_PER_WORD
           && ((code = mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,to_mode, word_mode, unsignedp))!= CODE_FOR_nothing)){
          rtx word_to = mtcs_emit_gen_reg_rtx(mtcsEmit,word_mode);
          if (REG_P (to)){
              if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,to, from))
                  from = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,from_mode, from);
              mtcs_emit_emit_clobber/*!emit_clobber*/ (mtcsEmit,to);
          }
          mtcs_expr_convert_move(self,word_to, from, unsignedp);
          mtcs_optabs_emit_unop_insn (mtcsOptabs,code, to, word_to, equiv_code);
          return;
      }
      /* No special multiword conversion insn; do it by hand.  */
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      /* Since we will turn this into a no conflict block, we must ensure
         the source does not overlap the target so force it into an isolated
         register when maybe so.  Likewise for any MEM input, since the
         conversion sequence might require several references to it and we
         must ensure we're getting the same value every time.  */

      if (MEM_P (from) || mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,to, from))
          from = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,from_mode, from);

      /* Get a copy of FROM widened to a word, if necessary.  */
      if (mtcs_mode_get_precision(mtcsMode,from_mode) < BITS_PER_WORD)
          lowpart_mode = word_mode;
      else
          lowpart_mode = from_mode;

      lowfrom = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,lowpart_mode, from, unsignedp);
      lowpart = gen_lowpart (lowpart_mode, to);
      mtcs_expr_emit_move_insn(self,lowpart, lowfrom);
      /* Compute the value to put in each remaining word.  */
      if (unsignedp)
          fill_value = const0_rtx;
      else
        fill_value = mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,
                mtcs_emit_gen_reg_rtx(mtcsEmit,word_mode),LT, lowfrom, const0_rtx, lowpart_mode, 0, -1);

      /* Fill the remaining words.  */
      for (i = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,lowpart_mode) / UNITS_PER_WORD; i < nwords; i++){
          int index = (WORDS_BIG_ENDIAN ? nwords - i - 1 : i);
          rtx subword = mtcs_rtl_operand_subword(mtcsRTL,to, index, 1, to_mode);
          gcc_assert (subword);
          if (fill_value != subword)
            mtcs_expr_emit_move_insn(self,subword, fill_value);
      }
      insns = mtcs_rtl_data_get_insns (mtcsRtlData);
      mtcs_emit_end_sequence (mtcsEmit);
      mtcs_emit_emit_insn(mtcsEmit,insns);
      return;
  }
  n_debug("mtcsexpr.c convert_mode_scalar 77 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Truncating multi-word to a word or less.  */
  if (mtcs_mode_get_precision(mtcsMode,from_mode) > BITS_PER_WORD
      && mtcs_mode_get_precision(mtcsMode,to_mode) <= BITS_PER_WORD){
      if (!((MEM_P (from) && ! MEM_VOLATILE_P (from)
         && mtcs_reg_get_direct_load/*!direct_load[(int) to_mode]*/(mtcsReg,(int)to_mode)
         && ! mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(mtcsRecog,XEXP (from, 0),
                 mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,from)))
        || REG_P (from) || GET_CODE (from) == SUBREG))
          from = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,from_mode, from);
      mtcs_expr_convert_move(self,to, gen_lowpart (word_mode, from), 0);
      return;
  }
  n_debug("mtcsexpr.c convert_mode_scalar 88 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Now follow all the conversions between integers
     no more than a word long.  */

  /* For truncation, usually we can just refer to FROM in a narrower mode.  */
  if (mtcs_mode_get_bitsize (mtcsMode,to_mode) < mtcs_mode_get_bitsize (mtcsMode,from_mode)
      && TRULY_NOOP_TRUNCATION_MODES_P (to_mode, from_mode)){
      if (!((MEM_P (from)  && ! MEM_VOLATILE_P (from)
         && mtcs_reg_get_direct_load/*!direct_load[(int) to_mode]*/(mtcsReg,to_mode)
         && ! mtcs_recog_mode_dependent_address_p/*!mode_dependent_address_p*/(mtcsRecog,XEXP (from, 0),
                 mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,from)))
        || REG_P (from)  || GET_CODE (from) == SUBREG))
          from = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,from_mode, from);
      if (REG_P (from) && REGNO (from) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
          && !mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,REGNO (from), to_mode))
          from = mtcs_explow_copy_to_reg (mtcsExplow,from);
      mtcs_expr_emit_move_insn(self,to, gen_lowpart (to_mode, from));
      return;
  }
  n_debug("mtcsexpr.c convert_mode_scalar 99 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Handle extension.  */
  if (mtcs_mode_get_precision(mtcsMode,to_mode) > mtcs_mode_get_precision(mtcsMode,from_mode)){
      /* Convert directly if that works.  */
      if ((code = mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,to_mode, from_mode, unsignedp))!= CODE_FOR_nothing){
          mtcs_optabs_emit_unop_insn (mtcsOptabs,code, to, from, equiv_code);
          return;
      }else{
          rtx tmp;
          int shift_amount;

          /* Search for a mode to convert via.  */
          opt_scalar_mode intermediate_iter;
          FOR_EACH_MODE_FROM (intermediate_iter, from_mode){
              scalar_mode intermediate = intermediate_iter.require ();
              if (((mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,to_mode, intermediate, unsignedp)!= CODE_FOR_nothing)
                || (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,to_mode) < mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,intermediate)
                && TRULY_NOOP_TRUNCATION_MODES_P (to_mode,intermediate)))
                && (mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,intermediate, from_mode, unsignedp)!= CODE_FOR_nothing)){
                  mtcs_expr_convert_move(self,to, mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,intermediate, from,unsignedp), unsignedp);
                  return;
              }
          }
          /* No suitable intermediate mode.
             Generate what we need with shifts.  */
          shift_amount = (mtcs_mode_get_precision(mtcsMode,to_mode) - mtcs_mode_get_precision(mtcsMode,from_mode));
          from = gen_lowpart (to_mode, mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,from_mode, from));
          tmp = mtcs_expmed_expand_shift(mtcsExpmed,LSHIFT_EXPR, to_mode, from, shift_amount,to, unsignedp);
          tmp = mtcs_expmed_expand_shift(mtcsExpmed,RSHIFT_EXPR, to_mode, tmp, shift_amount,to, unsignedp);
          if (tmp != to)
            mtcs_expr_emit_move_insn(self,to, tmp);
          return;
      }
  }
  n_debug("mtcsexpr.c convert_mode_scalar 100 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Support special truncate insns for certain modes.  */
  if (mtcs_opinit_convert_optab_handler(mtcsOpinit,trunc_optab, to_mode,from_mode) != CODE_FOR_nothing){
      mtcs_optabs_emit_unop_insn (mtcsOptabs,mtcs_opinit_convert_optab_handler(mtcsOpinit,trunc_optab, to_mode, from_mode),to, from, UNKNOWN);
      return;
  }
  n_debug("mtcsexpr.c convert_mode_scalar 101 tomode:%d frommode:%d convert_optab:%d\n",to_mode,from_mode);

  /* Handle truncation of volatile memrefs, and so on;
     the things that couldn't be truncated directly,
     and for which there was no special instruction.

     ??? Code above formerly short-circuited this, for most integer
     mode pairs, with a force_reg in from_mode followed by a recursive
     call to this routine.  Appears always to have been wrong.  */
  if (mtcs_mode_get_precision(mtcsMode,to_mode) < mtcs_mode_get_precision(mtcsMode,from_mode)){
      rtx temp = mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,to_mode, gen_lowpart (to_mode, from));
      mtcs_expr_emit_move_insn(self,to, temp);
      return;
  }
  /* Mode combination is not recognized.  */
  gcc_unreachable ();
}


static rtx get_subtarget (MtcsExpr *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  return (optimize || x == 0
       /* Only registers can be subtargets.  */
       || !REG_P (x)
       /* Don't use hard regs to avoid extending their life.  */
       || REGNO (x) <mtcs_reg_get_first_pseudo_register(mtcsReg)/*! FIRST_PSEUDO_REGISTER*/
      ? 0 : x);
}

/* Given an rtx VALUE that may contain additions and multiplications, return
   an equivalent value that just refers to a register, memory, or constant.
   This is done by generating instructions to perform the arithmetic and
   returning a pseudo-register containing the value.

   The returned value may be a REG, SUBREG, MEM or constant.  */
/* Given an rtx VALUE that may contain additions and multiplications, return
   an equivalent value that just refers to a register, memory, or constant.
   This is done by generating instructions to perform the arithmetic and
   returning a pseudo-register containing the value.

   The returned value may be a REG, SUBREG, MEM or constant.  */
//原型 force_operand rtl.h expr.cc
rtx mtcs_expr_force_operand (MtcsExpr *self,rtx value, rtx target)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOptabs   *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpmed   *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx op1, op2;
  /* Use subtarget as the target for operand 0 of a binary operation.  */
  rtx subtarget = get_subtarget (self,target);
  enum rtx_code code = GET_CODE (value);
  /* Check for subreg applied to an expression produced by loop optimizer.  */
  if (code == SUBREG  && !REG_P (SUBREG_REG (value)) && !MEM_P (SUBREG_REG (value))){
      rtx x=mtcs_expr_force_operand(self,SUBREG_REG (value),NULL_RTX);
      rtx y=mtcs_explow_force_reg(mtcsExplow,GET_MODE (SUBREG_REG (value)),x);
      value = mtcs_simplify_rtx_gen_subreg(mtcsSimplifyRtx,GET_MODE (value), y,GET_MODE (SUBREG_REG (value)),SUBREG_BYTE (value));
      /*
      value = simplify_gen_subreg (GET_MODE (value),force_reg (GET_MODE (SUBREG_REG (value)),
                          force_operand (SUBREG_REG (value), NULL_RTX)),
                       GET_MODE (SUBREG_REG (value)), SUBREG_BYTE (value));
      */
      code = GET_CODE (value);
  }

  /* Check for a PIC address load.  */
  if ((code == PLUS || code == MINUS) && XEXP (value, 0) == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(mtcsRTL)
        && (GET_CODE (XEXP (value, 1)) == SYMBOL_REF
          || GET_CODE (XEXP (value, 1)) == LABEL_REF || GET_CODE (XEXP (value, 1)) == CONST)){
      if (!subtarget)
          subtarget = mtcs_emit_gen_reg_rtx (mtcsEmit,GET_MODE (value));
      mtcs_expr_emit_move_insn(self,subtarget, value);
      return subtarget;
  }

  if (ARITHMETIC_P (value)){
      op2 = XEXP (value, 1);
      if (!CONSTANT_P (op2) && !(REG_P (op2) && op2 != subtarget))
          subtarget = 0;
      if (code == MINUS && CONST_INT_P (op2)){
          code = PLUS;
          op2 = mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,GET_MODE (value), op2);
      }

      /* Check for an addition with OP2 a constant integer and our first
         operand a PLUS of a virtual register and something else.  In that
         case, we want to emit the sum of the virtual register and the
         constant first and then add the other value.  This allows virtual
         register instantiation to simply modify the constant rather than
         creating another one around this addition.  */
      if (code == PLUS && CONST_INT_P (op2) && GET_CODE (XEXP (value, 0)) == PLUS
              && REG_P (XEXP (XEXP (value, 0), 0))  && mtcs_reg_virtual_register_p/*!VIRTUAL_REGISTER_P*/(mtcsReg,
                    XEXP (XEXP (value, 0), 0))){
          rtx temp = mtcs_optabs_expand_simple_binop/*expand_simple_binop*/(mtcsOptabs,GET_MODE (value),
                  code,XEXP (XEXP (value, 0), 0),op2,subtarget, 0, OPTAB_LIB_WIDEN);
          return mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,GET_MODE (value), code, temp,
                  mtcs_expr_force_operand(self,XEXP (XEXP (value, 0), 1), 0),target, 0, OPTAB_LIB_WIDEN);
      }

      op1 = mtcs_expr_force_operand(self,XEXP (value, 0), subtarget);
      op2 = mtcs_expr_force_operand(self,op2, NULL_RTX);
      switch (code){
        case MULT:
           n_debug("mtcsexpr.c mtcs_expr_force_operand 00 MULT\n");
          return mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,GET_MODE (value), op1, op2, target, 1);
        case DIV:
           n_debug("mtcsexpr.c mtcs_expr_force_operand 11 MULT\n");

          if (!mtcs_mode_is_integral_p (mtcsMode,GET_MODE (value)))
            return mtcs_optabs_expand_simple_binop(mtcsOptabs,GET_MODE (value), code, op1, op2,target, 1, OPTAB_LIB_WIDEN);
          else
            return mtcs_expmed_expand_divmod (mtcsExpmed,0,mtcs_mode_is_float_p (mtcsMode,GET_MODE (value))? RDIV_EXPR :
                    TRUNC_DIV_EXPR,GET_MODE (value), op1, op2, target, 0);
        case MOD:
          return mtcs_expmed_expand_divmod (mtcsExpmed,1, TRUNC_MOD_EXPR, GET_MODE (value), op1, op2,target, 0);
        case UDIV:
          return mtcs_expmed_expand_divmod (mtcsExpmed,0, TRUNC_DIV_EXPR, GET_MODE (value), op1, op2,target, 1);
        case UMOD:
          return mtcs_expmed_expand_divmod (mtcsExpmed,1, TRUNC_MOD_EXPR, GET_MODE (value), op1, op2,target, 1);
        case ASHIFTRT:
          return mtcs_optabs_expand_simple_binop(mtcsOptabs,GET_MODE (value), code, op1, op2,target, 0, OPTAB_LIB_WIDEN);
        default:
          return mtcs_optabs_expand_simple_binop(mtcsOptabs,GET_MODE (value), code, op1, op2,target, 1, OPTAB_LIB_WIDEN);
        }
  }
  if (UNARY_P (value)){
      if (!target)
          target = mtcs_emit_gen_reg_rtx (mtcsEmit,GET_MODE (value));
      op1 = mtcs_expr_force_operand(self,XEXP (value, 0), NULL_RTX);
      switch (code){
        case ZERO_EXTEND:
        case SIGN_EXTEND:
        case TRUNCATE:
        case FLOAT_EXTEND:
        case FLOAT_TRUNCATE:
          n_debug("mtcxexpr.c mtcs_expr_force_operand code:%d FLOAT_EXTEND:%d\n",code,FLOAT_EXTEND);
          mtcs_expr_convert_move(self,target, op1, code == ZERO_EXTEND);
          return target;

        case FIX:
        case UNSIGNED_FIX:
            mtcs_optabs_expand_fix/*!expand_fix*/(mtcsOptabs,target, op1, code == UNSIGNED_FIX);
          return target;

        case FLOAT:
        case UNSIGNED_FLOAT:
            mtcs_optabs_expand_float/*!expand_float*/(mtcsOptabs,target, op1, code == UNSIGNED_FLOAT);
          return target;

        default:
          return mtcs_optabs_expand_simple_unop (mtcsOptabs,GET_MODE (value), code, op1, target, 0);
      }
  }

  //INSN_SCHEDULING host=1 nvptx=0
//#ifdef INSN_SCHEDULING
//  /* On machines that have insn scheduling, we want all memory reference to be
//     explicit, so we need to deal with such paradoxical SUBREGs.  */
//  if (paradoxical_subreg_p (value) && MEM_P (SUBREG_REG (value)))
//    value
//      = simplify_gen_subreg (GET_MODE (value),
//                 force_reg (GET_MODE (SUBREG_REG (value)),
//                    force_operand (SUBREG_REG (value),
//                               NULL_RTX)),
//                 GET_MODE (SUBREG_REG (value)),
//                 SUBREG_BYTE (value));
//#endif

  return value;
}

/* A subroutine of emit_move_insn_1.  Generate a move from Y into X.
   MODE is known to be complex.  Returns the last instruction emitted.  */
static rtx_insn *emit_move_complex (MtcsExpr *self,machine_mode mode, rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  bool try_int;
  /* Need to take special care for pushes, to maintain proper ordering
     of the data, and possibly extra padding.  */
  if (mtcs_preds_push_operand/*push_operand*/ (mtcsPreds,x, mode))
    return mtcs_expr_emit_move_complex_push/*!emit_move_complex_push*/(self,mode, x, y);

  /* See if we can coerce the target into moving both values at once, except
     for floating point where we favor moving as parts if this is easy.  */
  mtcs_mode innerMode=mtcs_mode_get_inner(mtcsMode,mode);
  enum insn_code code=mtcs_opinit_optab_handler(mtcsOpinit,mov_optab,innerMode);
  if (mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) == MODE_COMPLEX_FLOAT
      && code/*optab_handler (mov_optab, GET_MODE_INNER (mode))*/ != CODE_FOR_nothing
      && !(REG_P (x)  &&mtcs_reg_is_hard_rtx(mtcsReg,x)/*HARD_REGISTER_P (x)*/  && REG_NREGS (x) == 1)
      && !(REG_P (y) && mtcs_reg_is_hard_rtx(mtcsReg,y)/*HARD_REGISTER_P (y)*/ && REG_NREGS (y) == 1))
    try_int = false;
  /* Not possible if the values are inherently not adjacent.  */
  else if (GET_CODE (x) == CONCAT || GET_CODE (y) == CONCAT)
    try_int = false;
  /* Is possible if both are registers (or subregs of registers).  */
  else if (mtcs_preds_register_operand/*register_operand*/ (mtcsPreds,x, mode)
          && mtcs_preds_register_operand/*register_operand*/ (mtcsPreds,y, mode))
    try_int = true;
  /* If one of the operands is a memory, and alignment constraints
     are friendly enough, we may be able to do combined memory operations.
     We do not attempt this if Y is a constant because that combination is
     usually better with the by-parts thing below.  */
  else if ((MEM_P (x) ? !CONSTANT_P (y) : MEM_P (y))
       && (!mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign) ||
               mtcs_mode_get_alignment/*get_mode_alignment*/ (mtcsMode,mode)
                  == mtcs_align_get_biggest_alignment(mtcsAlign)/*BIGGEST_ALIGNMENT*/))
    try_int = true;
  else
    try_int = false;

  if (try_int){
      rtx_insn *ret;

      /* For memory to memory moves, optimal behavior can be had with the
     existing block move logic.  But use normal expansion if optimizing
     for size.  */
      if (MEM_P (x) && MEM_P (y)){
          rtx rtxint=mtcs_rtl_gen_int_mode(mtcsRTL,mtcs_mode_get_size(mtcsMode,mode),mtcs_mode_get_Pmode(mtcsMode));
          mtcs_expr_emit_block_move/*!emit_block_move*/(self,x, y, rtxint/*!gen_int_mode (GET_MODE_SIZE(mode), Pmode)*/,
                  (optimize_insn_for_speed_p() ? BLOCK_OP_NO_LIBCALL : BLOCK_OP_NORMAL));
          return mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      }
      ret = emit_move_via_integer(self,mode, x, y, true);
      if (ret)
          return ret;
  }
  return mtcs_expr_emit_move_complex_parts/*!emit_move_complex_parts*/(self,x, y);
}

/* A subroutine of emit_move_insn_1.  Generate a move from Y into X using
   an integer mode of the same size as MODE.  Returns the instruction
   emitted, or NULL if such a move could not be generated.  */

static rtx_insn *emit_move_via_integer (MtcsExpr *self,machine_mode mode, rtx x, rtx y, bool force)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  scalar_int_mode imode;
  enum insn_code code;
  /* There must exist a mode of the exact size we require.  */
  if (!mtcs_mode_int_mode_for_mode/* int_mode_for_mode*/ (mtcsMode,mode).exists (&imode))
    return NULL;

  /* The target must support moves in this mode.  */
  code =mtcs_opinit_optab_handler/* optab_handler*/ (mtcsOpinit,mov_optab, (mtcs_mode)imode);
  if (code == CODE_FOR_nothing)
    return NULL;

  x = emit_move_change_mode (self,imode, mode, x, force);
  if (x == NULL_RTX)
    return NULL;
  y = emit_move_change_mode (self,imode, mode, y, force);
  if (y == NULL_RTX)
    return NULL;
  return mtcs_emit_emit_insn(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(code) (x, y));
}

/* A subroutine of emit_move_insn_1.  Yet another lowpart generator.
   NEW_MODE and OLD_MODE are the same size.  Return NULL if X cannot be
   represented in NEW_MODE.  If FORCE is true, this will never happen, as
   we'll force-create a SUBREG if needed.  */
//原型 emit_move_change_mode expr.cc
static rtx emit_move_change_mode (MtcsExpr *self,machine_mode new_mode, machine_mode old_mode, rtx x, bool force)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);

  rtx ret;
  if (mtcs_preds_push_operand/* push_operand*/ (mtcsPreds,x, GET_MODE (x))){
      ret = gen_rtx_MEM (new_mode, XEXP (x, 0));
      MEM_COPY_ATTRIBUTES (ret, x);
  }else if (MEM_P (x)){
      /* We don't have to worry about changing the address since the
     size in bytes is supposed to be the same.  */
      if (reload_in_progress){
          /* Copy the MEM to change the mode and move any
             substitutions from the old MEM to the new one.  */
          ret = mtcs_rtl_adjust_address_nv (mtcsRTL,x, new_mode, 0);
          mtcs_reload_copy_replacements (mtcsReload,x, ret);
      }else
          ret = mtcs_rtl_adjust_address(mtcsRTL,x, new_mode, 0);
  }else{
      /* Note that we do want simplify_subreg's behavior of validating
     that the new mode is ok for a hard register.  If we were to use
     simplify_gen_subreg, we would create the subreg, but would
     probably run into the target not being able to implement it.  */
      /* Except, of course, when FORCE is true, when this is exactly what
     we want.  Which is needed for CCmodes on some targets.  */
      if (force)
          ret = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,new_mode, x, old_mode, 0);
      else
          ret = mtcs_simplify_rtx_subreg/*simplify_subreg*/(mtcsSimplifyRtx,new_mode, x, old_mode, 0);
  }
  return ret;
}
/* Low level part of emit_move_insn.
   Called just like emit_move_insn, but assumes X and Y
   are basically valid.  */
//原型 emit_move_insn_1 expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_insn_1 (MtcsExpr *self,rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

  machine_mode mode = GET_MODE (x);
  enum insn_code code;
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn_1 00 mod:%d MAX_MACHINE_MODE:%d mov_optab:%d LAST_CONV_OPTAB:%d\n",
          mode,mtcs_mode_get_max_number(mtcsMode),mov_optab,LAST_CONV_OPTAB);
  gcc_assert ((unsigned int) mode < (unsigned int) mtcs_mode_get_max_number(mtcsMode)/*MAX_MACHINE_MODE*/);
  code = mtcs_opinit_optab_handler/*!optab_handler*/ (mtcsOpinit,mov_optab, mode);
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn_1 11 mode:%d code:%d %d\n",mode,code,CODE_FOR_nothing);

  if (code != CODE_FOR_nothing){
      n_debug("mtcsexpr.c mtcs_expr_emit_move_insn_1 11---有效的 code mode:%d code:%d %d genfn:%s\n",
            mode,code,CODE_FOR_nothing,mtcsTarget->mtcsOutput->insn_data[code].name);
     mtcs_print_rtl(stderr,x);
     mtcs_print_rtl(stderr,y);
    return mtcs_emit_emit_insn(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(code) (x, y));
  }
  /* Expand complex moves by moving real part and imag part.  */
  if (mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,mode))
    return emit_move_complex (self,mode, x, y);

  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_DECIMAL_FLOAT
          ||mtcs_mode_is_all_fixed_point_p/*! ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode)){
      rtx_insn *result = emit_move_via_integer(self,mode, x, y, true);
      n_debug("mtcsexpr.c mtcs_expr_emit_move_insn_1 22 mode:%d code:%d result:%p\n",mode,code,result);
      /* If we can't find an integer mode, use multi words.  */
      if (result)
          return result;
      else
          return emit_move_multi_word (self,mode, x, y);
  }

  if (mtcs_mode_get_class (mtcsMode,mode) == MODE_CC)
    return emit_move_ccmode (self,mode, x, y);
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn_1 33 mode:%d code:%d\n",mode,code);

  /* Try using a move pattern for the corresponding integer mode.  This is
     only safe when simplify_subreg can convert MODE constants into integer
     constants.  At present, it can only do this reliably if the value
     fits within a HOST_WIDE_INT.  */
  if (!CONSTANT_P (y)|| known_le (mtcs_mode_get_bitsize(mtcsMode,mode), HOST_BITS_PER_WIDE_INT)){
      rtx_insn *ret = emit_move_via_integer(self,mode, x, y, lra_in_progress);
      if (ret){
          if (! lra_in_progress || mtcs_recog_recog/*!recog*/(mtcsRecog,PATTERN (ret), ret, 0) >= 0)
            return ret;
      }
  }
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn_1 44 mode:%d code:%d\n",mode,code);
  return emit_move_multi_word (self,mode, x, y);
}


/* Generate code to copy Y into X.
   Both Y and X must have the same mode, except that
   Y can be a constant with VOIDmode.
   This mode cannot be BLKmode; use emit_block_move for that.

   Return the last instruction emitted.  */
//原型 emit_move_insn expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_insn (MtcsExpr *self,rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  machine_mode mode = GET_MODE (x);
  rtx y_cst = NULL_RTX;
  rtx_insn *last_insn;
  rtx set;
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn 00 x:%d y:%d %d %d\n",
          mode,GET_MODE (y),mtcsMode->modes.M_BLKmode,mtcsMode->modes.M_VOIDmode);
  gcc_assert (mode != mtcsMode->modes.M_BLKmode
          && (GET_MODE (y) == mode || GET_MODE (y) == mtcsMode->modes.M_VOIDmode));
  /* If we have a copy that looks like one of the following patterns:
       (set (subreg:M1 (reg:M2 ...)) (subreg:M1 (reg:M2 ...)))
       (set (subreg:M1 (reg:M2 ...)) (mem:M1 ADDR))
       (set (mem:M1 ADDR) (subreg:M1 (reg:M2 ...)))
       (set (subreg:M1 (reg:M2 ...)) (constant C))
     where mode M1 is equal in size to M2, try to detect whether the
     mode change involves an implicit round trip through memory.
     If so, see if we can avoid that by removing the subregs and
     doing the move in mode M2 instead.  */
  rtx x_inner = NULL_RTX;
  rtx y_inner = NULL_RTX;
  auto candidate_subreg_p = [&](rtx subreg) {
    return (REG_P (SUBREG_REG (subreg))
        && known_eq (mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,GET_MODE (SUBREG_REG (subreg))),
                mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,GET_MODE (subreg)))
        && mtcs_opinit_optab_handler/*!optab_handler*/ (mtcsOpinit,mov_optab, GET_MODE (SUBREG_REG (subreg)))
           != CODE_FOR_nothing);
  };
  nuint allRegs=mtcs_reg_get_all_regs(mtcsReg);/*!ALL_REGS*/
  auto candidate_mem_p = [&](machine_mode innermode, rtx mem) {
    return (!mtcsTarget->can_change_mode_class (mtcsTarget,innermode, GET_MODE (mem),allRegs/*!ALL_REGS*/)
        && !mtcs_preds_push_operand (mtcsPreds,mem, GET_MODE (mem))
        /* Not a candiate if innermode requires too much alignment.  */
        && (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,mem) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/ (mtcsMode,innermode)
        || mtcsTarget->slow_unaligned_access (mtcsTarget,GET_MODE (mem),mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,mem))
        || !mtcsTarget->slow_unaligned_access (mtcsTarget,innermode,mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,mem))));
  };
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn 11 allRegs:%d\n",allRegs);
  if (SUBREG_P (x) && candidate_subreg_p (x))
    x_inner = SUBREG_REG (x);

  if (SUBREG_P (y) && candidate_subreg_p (y))
    y_inner = SUBREG_REG (y);

  if (x_inner != NULL_RTX && y_inner != NULL_RTX && GET_MODE (x_inner) == GET_MODE (y_inner)
      && !mtcsTarget->can_change_mode_class(mtcsTarget,GET_MODE (x_inner), mode, ALL_REGS)){
      x = x_inner;
      y = y_inner;
      mode = GET_MODE (x_inner);
  }else if (x_inner != NULL_RTX && MEM_P (y) && candidate_mem_p (GET_MODE (x_inner), y)){
      x = x_inner;
      y = mtcs_rtl_adjust_address(mtcsRTL,y, GET_MODE (x_inner), 0);
      mode = GET_MODE (x_inner);
  }else if (y_inner != NULL_RTX && MEM_P (x) && candidate_mem_p (GET_MODE (y_inner), x)){
      x = mtcs_rtl_adjust_address(mtcsRTL,x, GET_MODE (y_inner), 0);
      y = y_inner;
      mode = GET_MODE (y_inner);
  }else if (x_inner != NULL_RTX && CONSTANT_P (y) && !mtcsTarget->can_change_mode_class (mtcsTarget,
          GET_MODE (x_inner),mode, allRegs/*!ALL_REGS*/)
       && (y_inner = mtcs_simplify_rtx_subreg/*simplify_subreg*/(mtcsSimplifyRtx,GET_MODE (x_inner), y, mode, 0))){
      x = x_inner;
      y = y_inner;
      mode = GET_MODE (x_inner);
  }

  if (CONSTANT_P (y)){
      if (optimize  && mtcs_mode_is_scalar_float_p(mtcsMode,GET_MODE (x)) && (last_insn = compress_float_constant (self,x, y)))
          return last_insn;
      y_cst = y;
      n_debug("mtcsexpr.c mtcs_expr_emit_move_insn 22 allRegs:%d\n",allRegs);

      if (!mtcsTarget->legitimate_constant_p (mtcsTarget,mode, y)){
          n_debug("mtcsexpr.c mtcs_expr_emit_move_insn 33 allRegs:%d\n",allRegs);

          y = mtcs_asm_force_const_mem (mtcsAsm,mode, y);
          /* If the target's cannot_force_const_mem prevented the spill,
             assume that the target's move expanders will also take care
             of the non-legitimate constant.  */
          if (!y)
            y = y_cst;
          else
            y = mtcs_explow_use_anchored_address (mtcsExplow,y);
      }
  }
  /* If X or Y are memory references, verify that their addresses are valid
     for the machine.  */
  if (MEM_P (x) && (! memory_address_addr_space_p (GET_MODE (x), XEXP (x, 0),mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,x))
      && ! mtcs_preds_push_operand (mtcsPreds,x, GET_MODE (x))))
    x = mtcs_explow_validize_mem(mtcsExplow,x);

  if (MEM_P (y) && ! memory_address_addr_space_p (GET_MODE (y), XEXP (y, 0),mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,y)))
    y = mtcs_explow_validize_mem(mtcsExplow,y);
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn 44 allRegs:%d mode:%d BLK:%d\n",allRegs,mode,mtcsMode->modes.M_BLKmode);

  gcc_assert (mode != mtcsMode->modes.M_BLKmode);


  last_insn = mtcs_expr_emit_move_insn_1 (self,x, y);
  n_debug("mtcsexpr.c emit_move_insn 55 allRegs:%d mode:%d BLK:%d\n",allRegs,mode,mtcsMode->modes.M_BLKmode);

  if (y_cst && REG_P (x) && (set = single_set (last_insn)) != NULL_RTX && SET_DEST (set) == x && ! rtx_equal_p (y_cst, SET_SRC (set)))
     mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,last_insn, REG_EQUAL, copy_rtx (y_cst));
  n_debug("mtcsexpr.c mtcs_expr_emit_move_insn 66 allRegs:%d\n",allRegs);

  return last_insn;
}

/* Return an rtx for a value that would result
   from converting X from mode OLDMODE to mode MODE.
   Both modes may be floating, or both integer.
   UNSIGNEDP is nonzero if X is an unsigned value.

   This can be done by referring to a part of X in place
   or by copying to a new temporary with conversion.

   You can give VOIDmode for OLDMODE, if you are sure X has a nonvoid mode.  */
//原型 convert_modes expr.h expr.cc
rtx mtcs_expr_convert_modes (MtcsExpr *self,machine_mode mode, machine_mode oldmode, rtx x, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  n_debug("mtcsexpr.c mtcs_expr_convert_modes 00 mode:%d oldmode:%d\n",mode,oldmode);

  rtx temp;
  scalar_int_mode int_mode;
  /* If FROM is a SUBREG that indicates that we have already done at least
     the required extension, strip it.  */
  if (GET_CODE (x) == SUBREG && SUBREG_PROMOTED_VAR_P (x) && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)
      && (mtcs_mode_get_precision(mtcsMode,mtcs_subreg_promoted_mode/*!subreg_promoted_mode*/(mtcsMode,x)) >=
            mtcs_mode_get_precision(mtcsMode,int_mode))
      && SUBREG_CHECK_PROMOTED_SIGN (x, unsignedp)) {
      scalar_int_mode int_orig_mode;
      scalar_int_mode int_inner_mode;
      machine_mode orig_mode = GET_MODE (x);
      x = gen_lowpart (int_mode, SUBREG_REG (x));//gen_lowpart rtl.h中的宏指向rtlhooks域gen_lawpart

      /* Preserve SUBREG_PROMOTED_VAR_P if the new mode is wider than
     the original mode, but narrower than the inner mode.  */
      if (GET_CODE (x) == SUBREG   && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,orig_mode, &int_orig_mode)
          && mtcs_mode_get_precision(mtcsMode,int_mode) > mtcs_mode_get_precision(mtcsMode,int_orig_mode)
          && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (SUBREG_REG (x)), &int_inner_mode)
          && mtcs_mode_get_precision(mtcsMode,int_inner_mode) > mtcs_mode_get_precision(mtcsMode,int_mode)){
          SUBREG_PROMOTED_VAR_P (x) = 1;
          SUBREG_PROMOTED_SET (x, unsignedp);
     }
  }

  if (GET_MODE (x) != VOIDmode)
    oldmode = GET_MODE (x);

  n_debug("mtcsexpr.c mtcs_expr_convert_modes 11 mode:%d oldmode:%d\n",mode,oldmode);
  if (mode == oldmode)
    return x;

  if (CONST_SCALAR_INT_P (x) && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)){
     n_debug("mtcsexpr.c mtcs_expr_convert_modes 22 mode:%d oldmode:%d\n",mode,oldmode);

      /* If the caller did not tell us the old mode, then there is not
     much to do with respect to canonicalization.  We have to
     assume that all the bits are significant.  */
      if (!mtcs_mode_is_a <scalar_int_mode>(mtcsMode,oldmode))
          oldmode = mtcsMode->modesMinMax.max_INT/*!MAX_MODE_INT*/;
      wide_int w = wide_int::from (mtcs_rtx_mode_t/*!rtx_mode_t*/(x,
            oldmode),mtcs_mode_get_precision(mtcsMode,int_mode), unsignedp ? UNSIGNED : SIGNED);
      return mtcs_rtl_immed_wide_int_const (mtcsRTL,w, int_mode);
  }

  /* We can do this with a gen_lowpart if both desired and current modes
     are integer, and this is either a constant integer, a register, or a
     non-volatile MEM. */
  scalar_int_mode int_oldmode;
  if (mtcs_mode_is_int_mode(mtcsMode,mode, &int_mode)
      && mtcs_mode_is_int_mode(mtcsMode,oldmode, &int_oldmode)
      && mtcs_mode_get_precision(mtcsMode,int_mode) <= mtcs_mode_get_precision(mtcsMode,int_oldmode)
      && ((MEM_P (x) && !MEM_VOLATILE_P (x) && mtcs_reg_get_direct_load/*!direct_load[(int) int_mode]*/(mtcsReg,(int) int_mode))
      || CONST_POLY_INT_P (x)   || (REG_P (x)    && (!mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/ (mtcsReg,x)
          || mtcsTarget->hard_regno_mode_ok(mtcsTarget,REGNO (x), int_mode))
              && mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/ (mtcsMode,int_mode, GET_MODE (x)))))
      return gen_lowpart (int_mode, x);

  /* Converting from integer constant into mode is always equivalent to an
     subreg operation.  */
  if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode) && GET_MODE (x) == VOIDmode) {
     n_debug("mtcsexpr.c mtcs_expr_convert_modes 33 mode:%d oldmode:%d\n",mode,oldmode);

      gcc_assert (known_eq (mtcs_mode_get_bitsize(mtcsMode,mode),mtcs_mode_get_bitsize(mtcsMode,oldmode)));
      return mtcs_simplify_rtx_gen_subreg (mtcsSimplifyRtx,mode, x, oldmode, 0);
  }
  temp = mtcs_emit_gen_reg_rtx (mtcsEmit,mode);
  mtcs_expr_convert_move(self,temp, x, unsignedp);
  return temp;
}

/* Copy data from FROM to TO, where the machine modes are not the same.
   Both modes may be integer, or both may be floating, or both may be
   fixed-point.
   UNSIGNEDP should be nonzero if FROM is an unsigned type.
   This causes zero-extension instead of sign-extension.  */
//原型 convert_move expr.h expr.cc
void mtcs_expr_convert_move (MtcsExpr *self,rtx to, rtx from, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  machine_mode to_mode = GET_MODE (to);
  machine_mode from_mode = GET_MODE (from);
  gcc_assert (to_mode != mtcsMode->modes.M_BLKmode);
  gcc_assert (from_mode != mtcsMode->modes.M_BLKmode);
  /* If the source and destination are already the same, then there's
     nothing to do.  */
  if (to == from)
    return;
  n_debug("mtcsexpr.c mtcs_expr_convert_move 00 to_mode:%d from_mode:%d\n",to_mode,from_mode);
  /* If FROM is a SUBREG that indicates that we have already done at least
     the required extension, strip it.  We don't handle such SUBREGs as
     TO here.  */
  scalar_int_mode to_int_mode;
  if (GET_CODE (from) == SUBREG
      && SUBREG_PROMOTED_VAR_P (from)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,to_mode, &to_int_mode)
      && (mtcs_mode_get_precision(mtcsMode,mtcs_subreg_promoted_mode/*!subreg_promoted_mode*/(mtcsMode,from))
      >= mtcs_mode_get_precision(mtcsMode,to_int_mode))
      && SUBREG_CHECK_PROMOTED_SIGN (from, unsignedp)){
     n_debug("mtcsexpr.c mtcs_expr_convert_move 11 to_mode:%d from_mode:%d\n",to_mode,from_mode);

      scalar_int_mode int_orig_mode;
      scalar_int_mode int_inner_mode;
      machine_mode orig_mode = GET_MODE (from);

      from = gen_lowpart (to_int_mode, SUBREG_REG (from));
      from_mode = to_int_mode;

      /* Preserve SUBREG_PROMOTED_VAR_P if the new mode is wider than
     the original mode, but narrower than the inner mode.  */
      if (GET_CODE (from) == SUBREG
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,orig_mode, &int_orig_mode)
      && mtcs_mode_get_precision(mtcsMode,to_int_mode)
         > mtcs_mode_get_precision(mtcsMode,int_orig_mode)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (SUBREG_REG (from)),
                     &int_inner_mode)
      && mtcs_mode_get_precision(mtcsMode,int_inner_mode)
         > mtcs_mode_get_precision(mtcsMode,to_int_mode)){
         n_debug("mtcsexpr.c mtcs_expr_convert_move 22 to_mode:%d from_mode:%d\n",to_mode,from_mode);

          SUBREG_PROMOTED_VAR_P (from) = 1;
          SUBREG_PROMOTED_SET (from, unsignedp);
      }
  }

  gcc_assert (GET_CODE (to) != SUBREG || !SUBREG_PROMOTED_VAR_P (to));

  if (to_mode == from_mode || (from_mode == VOIDmode && CONSTANT_P (from))){
     n_debug("mtcsexpr.c mtcs_expr_convert_move 33 to_mode:%d from_mode:%d\n",to_mode,from_mode);
      mtcs_expr_emit_move_insn(self,to, from);
      return;
  }

  if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,to_mode)
        || mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,from_mode)){
     n_debug("mtcsexpr.c mtcs_expr_convert_move 44 to_mode:%d from_mode:%d\n",to_mode,from_mode);

      if (mtcs_mode_get_unit_precision (mtcsMode,to_mode)> mtcs_mode_get_unit_precision (mtcsMode,from_mode)){
          optab op = unsignedp ? zext_optab : sext_optab;
          insn_code icode = mtcs_opinit_convert_optab_handler(mtcsOpinit,op, to_mode, from_mode);
          if (icode != CODE_FOR_nothing){
              mtcs_optabs_emit_unop_insn (mtcsOptabs,icode, to, from,unsignedp ? ZERO_EXTEND : SIGN_EXTEND);
              return;
          }
      }

      if (mtcs_mode_get_unit_precision (mtcsMode,to_mode) < mtcs_mode_get_unit_precision (mtcsMode,from_mode)){
          insn_code icode = mtcs_opinit_convert_optab_handler(mtcsOpinit,trunc_optab,to_mode, from_mode);
          if (icode != CODE_FOR_nothing){
              mtcs_optabs_emit_unop_insn (mtcsOptabs,icode, to, from, TRUNCATE);
              return;
          }
      }

      gcc_assert (known_eq (mtcs_mode_get_bitsize (mtcsMode,from_mode),mtcs_mode_get_bitsize (mtcsMode,to_mode)));

      if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,to_mode))
          from = mtcs_simplify_rtx_gen_subreg (mtcsSimplifyRtx,to_mode, from, GET_MODE (from), 0);
      else
          to = mtcs_simplify_rtx_gen_subreg (mtcsSimplifyRtx,from_mode, to, GET_MODE (to), 0);
      mtcs_expr_emit_move_insn(self,to, from);
      return;
  }

  if (GET_CODE (to) == CONCAT && GET_CODE (from) == CONCAT){
      mtcs_expr_convert_move(self,XEXP (to, 0), XEXP (from, 0), unsignedp);
      mtcs_expr_convert_move(self,XEXP (to, 1), XEXP (from, 1), unsignedp);
      return;
  }
  n_debug("mtcsexpr.c mtcs_expr_convert_move 55 to_mode:%d from_mode:%d unsignedp:%d\n",to_mode,from_mode,unsignedp);

  convert_mode_scalar(self,to, from, unsignedp);
}

/* Return an rtx for a value that would result
   from converting X to mode MODE.
   Both X and MODE may be floating, or both integer.
   UNSIGNEDP is nonzero if X is an unsigned value.
   This can be done by referring to a part of X in place
   or by copying to a new temporary with conversion.  */
//原型 convert_to_mode expr.h expr.cc
rtx mtcs_expr_convert_to_mode (MtcsExpr *self,machine_mode mode, rtx x, int unsignedp)
{
  return mtcs_expr_convert_modes (self,mode, VOIDmode, x, unsignedp);
}

/* Generate the body of an instruction to copy Y into X.
   It may be a list of insns, if one insn isn't enough.  */
//原型 gen_move_insn expr.h expr.cc
rtx_insn * mtcs_expr_gen_move_insn (MtcsExpr *self,rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *seq;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  mtcs_expr_emit_move_insn_1(self,x, y);
  seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  return seq;
}

/* A subroutine of emit_move_insn_1.  X is a push_operand in MODE.
   Return an equivalent MEM that does not use an auto-increment.  */
//原型 emit_move_resolve_push expr.h expr.cc
rtx mtcs_expr_emit_move_resolve_push (MtcsExpr *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  enum rtx_code code = GET_CODE (XEXP (x, 0));
  rtx temp;

  poly_int64 adjust = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/ (mtcsMode,mode);
//#ifdef PUSH_ROUNDING //host=1 nvptx=0
//  adjust = PUSH_ROUNDING (adjust);
//#endif
  if (code == PRE_DEC || code == POST_DEC)
    adjust = -adjust;
  else if (code == PRE_MODIFY || code == POST_MODIFY){
      rtx expr = XEXP (XEXP (x, 0), 1);

      gcc_assert (GET_CODE (expr) == PLUS || GET_CODE (expr) == MINUS);
      poly_int64 val = rtx_to_poly_int64 (XEXP (expr, 1));
      if (GET_CODE (expr) == MINUS)
          val = -val;
      gcc_assert (known_eq (adjust, val) || known_eq (adjust, -val));
      adjust = val;
  }
  /* Do not use anti_adjust_stack, since we don't want to update
     stack_pointer_delta.  */
  mtcs_mode pmode=mtcs_mode_get_Pmode/*!Pmode*/(mtcsMode);
  rtx  stackPointerRtx= mtcs_rtl_get_stack_pointer_rtx(mtcsRTL);

  temp = mtcs_optabs_expand_simple_binop(mtcsOptabs,pmode/*!Pmode*/, PLUS, stackPointerRtx/*!stack_pointer_rtx*/,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,adjust, pmode/*!Pmode*/), stackPointerRtx/*!stack_pointer_rtx*/,
                  0, OPTAB_LIB_WIDEN);
  if (temp != stackPointerRtx/*!stack_pointer_rtx*/)
    mtcs_expr_emit_move_insn(self,stackPointerRtx/*!stack_pointer_rtx*/, temp);

  switch (code){
    case PRE_INC:
    case PRE_DEC:
    case PRE_MODIFY:
      temp = stackPointerRtx/*!stack_pointer_rtx*/;
      break;
    case POST_INC:
    case POST_DEC:
    case POST_MODIFY:
      temp = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pmode/*!Pmode*/, stackPointerRtx/*!stack_pointer_rtx*/, -adjust);
      break;
    default:
      gcc_unreachable ();
  }
  return mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,x, temp);
}

/* Determine whether an operation OP on LEN bytes with alignment ALIGN can
   and should be performed piecewise.  */

static bool can_do_by_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT len, unsigned int align,
          enum by_pieces_operation op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  return mtcsTarget->/*targetm.use_by_pieces_infrastructure_p*/use_by_pieces_infrastructure_p(mtcsTarget,len, align, op,
                         optimize_insn_for_speed_p ());
}

/* Determine whether the LEN bytes can be moved by using several move
   instructions.  Return nonzero if a call to move_by_pieces should
   succeed.  */
//原型 can_move_by_pieces expr.h expr.cc
bool mtcs_expr_can_move_by_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT len, unsigned int align)
{
  return can_do_by_pieces(self,len, align, MOVE_BY_PIECES);
}

/* This is run to set up which modes can be used
   directly in memory and to initialize the block move optab.  It is run
   at the beginning of compilation and when the target is reinitialized.  */
//原型 init_expr_target expr.h expr.cc
void mtcs_expr_init_expr_target (MtcsExpr *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

  rtx pat;
  int num_clobbers;
  rtx mem, mem1;
  rtx reg;

  /* Try indexing by frame ptr and try by stack ptr.
     It is known that on the Convex the stack ptr isn't a valid index.
     With luck, one or the other is valid on any machine.  */
  mem = gen_rtx_MEM (mtcsMode->word_mode, mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
  mem1 = gen_rtx_MEM (mtcsMode->word_mode, mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL));

  /* A scratch register we can modify in-place below to avoid
     useless RTL allocations.  */
  int  lastVirtualRegister= mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg);
  reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode, lastVirtualRegister/*!LAST_VIRTUAL_REGISTER*/ + 1);

  rtx_insn *insn = as_a<rtx_insn *> (rtx_alloc (INSN));
  pat = gen_rtx_SET (NULL_RTX, NULL_RTX);
  PATTERN (insn) = pat;
  int numMachineModes=mtcs_mode_get_number(mtcsMode);

  for (machine_mode mode = VOIDmode; (int) mode < numMachineModes/*!NUM_MACHINE_MODES*/;mode = (machine_mode) ((int) mode + 1)){
      int regno;

      /*!direct_load[(int) mode] = direct_store[(int) mode] = 0*/;
      mtcsReg->hardRegs.x_direct_load/*!direct_load*/[(int) mode]=mtcsReg->hardRegs.x_direct_store/*!direct_store*/[(int) mode]=0;
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,mem, mode);
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,mem1, mode);

      /* See if there is some register that can be used in this mode and
     directly loaded or stored from memory.  */

      if (mode != VOIDmode && mode != mtcsMode->modes.M_BLKmode)
        for (regno = 0; regno < mtcs_reg_get_first_pseudo_register(mtcsReg)/*! FIRST_PSEUDO_REGISTER*/
             && (mtcs_reg_get_direct_load/*!direct_load[(int) mode]*/(mtcsReg,mode) == 0
                     || mtcsReg->hardRegs.x_direct_store/*!direct_store*/[(int) mode] == 0); regno++){

            if (!mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode))
              continue;

            mtcs_rtl_set_mode_and_regno/*!set_mode_and_regno*/(mtcsRTL,reg, mode, regno);

            SET_SRC (pat) = mem;
            SET_DEST (pat) = reg;
            if (mtcs_recog_recog/*!recog*/(mtcsRecog,pat, insn, &num_clobbers) >= 0)
                mtcs_reg_set_direct_load(mtcsReg,mode,1)/*!direct_load[(int) mode] = 1*/;

            SET_SRC (pat) = mem1;
            SET_DEST (pat) = reg;
            if (mtcs_recog_recog/*!recog*/(mtcsRecog,pat, insn, &num_clobbers) >= 0)
                mtcs_reg_set_direct_load(mtcsReg,mode,1)/*!direct_load[(int) mode] = 1*/;

            SET_SRC (pat) = reg;
            SET_DEST (pat) = mem;
            if (mtcs_recog_recog/*!recog*/(mtcsRecog,pat, insn, &num_clobbers) >= 0)
                mtcsReg->hardRegs.x_direct_store/*!direct_store*/[(int) mode]=1;

            SET_SRC (pat) = reg;
            SET_DEST (pat) = mem1;
            if (mtcs_recog_recog/*!recog*/(mtcsRecog,pat, insn, &num_clobbers) >= 0)
                mtcsReg->hardRegs.x_direct_store/*!direct_store*/[(int) mode]=1;
          }
  }

  mem = gen_rtx_MEM (VOIDmode, mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,mtcs_mode_get_Pmode(mtcsMode),
          mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1));

  opt_scalar_float_mode mode_iter;
  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode_iter, MODE_FLOAT){
      scalar_float_mode mode = mode_iter.require ();
      scalar_float_mode srcmode;
      n_debug("mtcsexpr.c init_expr_target 00 mode:%d narrowest:%d\n",mode,mtcs_mode_get_narrowest_mode(mtcsMode,mode));

      MTCS_FOR_EACH_MODE_UNTIL (mtcsMode,srcmode, mode){
          enum insn_code ic;
          ic = mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,mode, srcmode, 0);
          n_debug("mtcsexpr.c init_expr_target 11 mode:%d srcmode:%d ic:%d\n",mode,srcmode,ic);

          if (ic == CODE_FOR_nothing)
            continue;
          mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,mem, srcmode);
          n_debug("mtcsexpr.c init_expr_target 22 mode:%d srcmode:%d\n",mode,srcmode);

          if (mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,ic, 1, mem))
              mtcsReg->hardRegs.x_float_extend_from_mem/*!float_extend_from_mem*/[mode][srcmode] = true;
     }
  }
}

/* A subroutine of emit_block_move.  Returns true if calling the
   block move libcall will not clobber any parameters which may have
   already been placed on the stack.  */
//原型 block_move_libcall_safe_for_call_parm expr.cc
static bool block_move_libcall_safe_for_call_parm (MtcsExpr *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsArgs  *mtcsArgs=mtcs_target_get_args(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  tree fn;
  /* If arguments are pushed on the stack, then they're safe.  */
  if (target_calls_push_argument/*!targetm.calls.push_argument*/(mtcsMachine->calls,0))
    return true;

  /* If registers go on the stack anyway, any argument is sure to clobber
     an outgoing argument.  */
//#if defined (REG_PARM_STACK_SPACE) //host=1 nvptx=0
//  fn = builtin_decl_implicit (BUILT_IN_MEMCPY);
//  /* Avoid set but not used warning if *REG_PARM_STACK_SPACE doesn't
//     depend on its argument.  */
//  (void) fn;
//  if (OUTGOING_REG_PARM_STACK_SPACE ((!fn ? NULL_TREE : TREE_TYPE (fn)))
//      && REG_PARM_STACK_SPACE (fn) != 0)
//    return false;
//#endif

  /* If any argument goes in memory, then it might clobber an outgoing
     argument.  */
  {
    MtcsCumulativeArgs/*!CUMULATIVE_ARGS*/ args_so_far_v;
    cumulative_args_t args_so_far;
    tree arg;

    fn = builtin_decl_implicit (BUILT_IN_MEMCPY);
    mtcs_args_init_cumulative_args/*!INIT_CUMULATIVE_ARGS*/(mtcsArgs,&args_so_far_v, TREE_TYPE (fn), NULL_RTX, 0, 3);
    args_so_far = pack_cumulative_args ((CUMULATIVE_ARGS*)&args_so_far_v);
    arg = TYPE_ARG_TYPES (TREE_TYPE (fn));
    for ( ; arg != void_list_node ; arg = TREE_CHAIN (arg)){
        machine_mode mode = TYPE_MODE (TREE_VALUE (arg));
        mtcs_function_arg_info arg_info(mtcsMode,mode, /*named=*/true);
        rtx tmp = target_calls_function_arg/*targetm.calls.function_arg*/(mtcsMachine->calls,args_so_far, arg_info);
        if (!tmp || !REG_P (tmp))
          return false;
        if (target_calls_arg_partial_bytes/*!targetm.calls.arg_partial_bytes*/(mtcsMachine->calls,args_so_far, arg_info))
          return false;
        target_calls_function_arg_advance/*!targetm.calls.function_arg_advance*/(mtcsMachine->calls,args_so_far, arg_info);
    }
  }
  return true;
}

/* A subroutine of emit_block_move.  Expand a cpymem or movmem pattern;
   return true if successful.

   X is the destination of the copy or move.
   Y is the source of the copy or move.
   SIZE is the size of the block to be moved.

   MIGHT_OVERLAP indicates this originated with expansion of a
   builtin_memmove() and the source and destination blocks may
   overlap.
  */
//原型 emit_block_move_via_pattern expr.cc
static bool emit_block_move_via_pattern (MtcsExpr *self,rtx x, rtx y, rtx size, unsigned int align,
                 unsigned int expected_align,
                 HOST_WIDE_INT expected_size,
                 unsigned HOST_WIDE_INT min_size,
                 unsigned HOST_WIDE_INT max_size,
                 unsigned HOST_WIDE_INT probable_max_size,
                 bool might_overlap)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  if (expected_align < align)
    expected_align = align;
  if (expected_size != -1){
      if ((unsigned HOST_WIDE_INT)expected_size > probable_max_size)
          expected_size = probable_max_size;
      if ((unsigned HOST_WIDE_INT)expected_size < min_size)
          expected_size = min_size;
  }
  n_debug("mtcsexpr.c emit_block_move_via_pattern 设 volatile_ok---\n");
  /* Since this is a move insn, we don't care about volatility.  */
  mtcs_temporary_volatile_ok v (mtcsRecog,true);
  /* Try the most limited insn first, because there's no point
     including more than one in the machine description unless
     the more limited one has some advantage.  */

  opt_scalar_int_mode mode_iter;
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode_iter, MODE_INT){
      scalar_int_mode mode = mode_iter.require ();
      enum insn_code code;
      if (might_overlap)
          code = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,movmem_optab, mode);
      else
          code = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,cpymem_optab, mode);

      if (code != CODE_FOR_nothing
      /* We don't need MODE to be narrower than BITS_PER_HOST_WIDE_INT
         here because if SIZE is less than the mode mask, as it is
         returned by the macro, it will definitely be less than the
         actual mode mask.  Since SIZE is within the Pmode address
         space, we limit MODE to Pmode.  */
      && ((CONST_INT_P (size)
           && ((unsigned HOST_WIDE_INT) INTVAL (size)
           <= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) >> 1)))
          || max_size <= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) >> 1)
          || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) >= mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,pMode))){
          class expand_operand ops[9];
          unsigned int nops;

          /* ??? When called via emit_block_move_for_call, it'd be
             nice if there were some way to inform the backend, so
             that it doesn't fail the expansion because it thinks
             emitting the libcall would be more efficient.  */
          nops =mtcs_output_get_n_generator_args(mtcsOutput,code)/*!insn_data[(int) code].n_generator_args*/;
          gcc_assert (nops == 4 || nops == 6 || nops == 8 || nops == 9);

          create_fixed_operand (&ops[0], x);
          create_fixed_operand (&ops[1], y);
          /* The check above guarantees that this size conversion is valid.  */
          create_convert_operand_to (&ops[2], size, mode, true);
          mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[3], align / BITS_PER_UNIT);
          if (nops >= 6){
              mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[4], expected_align / BITS_PER_UNIT);
              mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[5], expected_size);
          }
          if (nops >= 8){
              mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[6], min_size);
              /* If we cannot represent the maximal size,
             make parameter NULL.  */
              if ((HOST_WIDE_INT) max_size != -1)
                  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[7], max_size);
              else
                  create_fixed_operand (&ops[7], NULL);
          }
          if (nops == 9){
              /* If we cannot represent the maximal size,
             make parameter NULL.  */
              if ((HOST_WIDE_INT) probable_max_size != -1)
                  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[8], probable_max_size);
              else
                  create_fixed_operand (&ops[8], NULL);
          }
          if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,code, nops, ops))
            return true;
      }
  }
  return false;
}

/* Emit code to move a block Y to a block X.  This may be done with
   string-move instructions, with multiple scalar move instructions,
   or with a library call.

   Both X and Y must be MEM rtx's (perhaps inside VOLATILE) with mode BLKmode.
   SIZE is an rtx that says how long they are.
   ALIGN is the maximum alignment we can assume they have.
   METHOD describes what kind of copy this is, and what mechanisms may be used.
   MIN_SIZE is the minimal size of block to move
   MAX_SIZE is the maximal size of block to move, if it cannot be represented
   in unsigned HOST_WIDE_INT, than it is mask of all ones.
   CTZ_SIZE is the trailing-zeros count of SIZE; even a nonconstant SIZE is
   known to be a multiple of 1<<CTZ_SIZE.

   Return the address of the new block, if memcpy is called and returns it,
   0 otherwise.  */
//原型 emit_block_move_hints expr.h expr.cc
rtx mtcs_expr_emit_block_move_hints (MtcsExpr *self,rtx x, rtx y, rtx size, enum block_op_methods method,
               unsigned int expected_align, HOST_WIDE_INT expected_size,
               unsigned HOST_WIDE_INT min_size,
               unsigned HOST_WIDE_INT max_size,
               unsigned HOST_WIDE_INT probable_max_size,
               bool bail_out_libcall, bool *is_move_done,
               bool might_overlap, unsigned ctz_size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

  int may_use_call;
  rtx retval = 0;
  unsigned int align;
  if (is_move_done)
    *is_move_done = true;
  gcc_assert (size);
  if (CONST_INT_P (size) && INTVAL (size) == 0)
    return 0;
  switch (method){
    case BLOCK_OP_NORMAL:
    case BLOCK_OP_TAILCALL:
      may_use_call = 1;
      break;
    case BLOCK_OP_CALL_PARM:
      may_use_call = block_move_libcall_safe_for_call_parm (self);
      /* Make inhibit_defer_pop nonzero around the library call
     to force it to pop the arguments right away.  */
      /*!NO_DEFER_POP; expr.h 定义*/
      mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop+=1;
      break;
    case BLOCK_OP_NO_LIBCALL:
      may_use_call = 0;
      break;
    case BLOCK_OP_NO_LIBCALL_RET:
      may_use_call = -1;
      break;
    default:
      gcc_unreachable ();
  }

  gcc_assert (MEM_P (x) && MEM_P (y));
  align = MIN (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,x), mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,y));
  gcc_assert (align >= BITS_PER_UNIT);
  /* Make sure we've got BLKmode addresses; store_one_arg can decide that
     block copy is more efficient for other large modes, e.g. DCmode.  */
  x = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,x, mtcsMode->modes.M_BLKmode, 0);
  y = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,y, mtcsMode->modes.M_BLKmode, 0);
  /* If source and destination are the same, no need to copy anything.  */
  if (rtx_equal_p (x, y) && !MEM_VOLATILE_P (x)  && !MEM_VOLATILE_P (y))
    return 0;
  /* Set MEM_SIZE as appropriate for this block copy.  The main place this
     can be incorrect is coming from __builtin_memcpy.  */
  poly_int64 const_size;
  if (poly_int_rtx_p (size, &const_size)){
      x = shallow_copy_rtx (x);
      y = shallow_copy_rtx (y);
      mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,x, const_size);
      mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,y, const_size);
  }
  bool pieces_ok = CONST_INT_P (size) && mtcs_expr_can_move_by_pieces(self,INTVAL (size), align);
  bool pattern_ok = false;
  if (!pieces_ok || might_overlap){
      pattern_ok = emit_block_move_via_pattern(self,x, y, size, align,
                       expected_align, expected_size,min_size, max_size, probable_max_size,might_overlap);
      if (!pattern_ok && might_overlap){
          /* Do not try any of the other methods below as they are not safe
             for overlapping moves.  */
          *is_move_done = false;
          return retval;
      }
  }

  bool dynamic_direction = false;
  if (!pattern_ok && !pieces_ok && may_use_call
      && (flag_inline_stringops & (might_overlap ? ILSOP_MEMMOVE : ILSOP_MEMCPY))){
      may_use_call = 0;
      dynamic_direction = might_overlap;
  }

  if (pattern_ok)
    ;
  else if (pieces_ok)
      mtcs_expr_move_by_pieces/*!move_by_pieces*/(self,x, y, INTVAL (size), align, RETURN_BEGIN);
  else if (may_use_call && !might_overlap
       && ADDR_SPACE_GENERIC_P (mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,x))
       && ADDR_SPACE_GENERIC_P (mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,y))){
      if (bail_out_libcall){
          if (is_move_done)
            *is_move_done = false;
          return retval;
      }

      if (may_use_call < 0)
          return pc_rtx;

      retval = mtcs_expr_emit_block_copy_via_libcall/*!emit_block_copy_via_libcall*/(self,x, y, size,method == BLOCK_OP_TAILCALL);
  }else if (dynamic_direction)
    emit_block_move_via_oriented_loop(self,x, y, size, align, ctz_size);
  else if (might_overlap)
    *is_move_done = false;
  else
    emit_block_move_via_sized_loop(self,x, y, size, align, ctz_size);

  if (method == BLOCK_OP_CALL_PARM)
      /*!OK_DEFER_POP;expr.h 定义*/
      mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop-=1;

  return retval;
}

//原型 emit_block_move expr.h expr.cc
rtx mtcs_expr_emit_block_move (MtcsExpr *self,rtx x, rtx y, rtx size, enum block_op_methods method, unsigned int ctz_size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  unsigned HOST_WIDE_INT max, min = 0;
  if (GET_CODE (size) == CONST_INT)
    min = max = UINTVAL (size);
  else
    max = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (size));
  return mtcs_expr_emit_block_move_hints(self,x, y, size, method, 0, -1,
                min, max, max, false, NULL, false, ctz_size);
}

/* Return number of insns required to perform operation OP by pieces
   for L bytes.  ALIGN (in bits) is maximum alignment we can assume.  */
//原型 by_pieces_ninsns target.h expr.cc
unsigned HOST_WIDE_INT mtcs_expr_by_pieces_ninsns(MtcsExpr *self,unsigned HOST_WIDE_INT l, unsigned int align,
          unsigned int max_size, by_pieces_operation op)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  unsigned HOST_WIDE_INT n_insns = 0;
  fixed_size_mode mode;

  if (mtcsTarget/*!targetm.overlap_op_by_pieces_p*/->overlap_op_by_pieces_p(mtcsTarget)){
      /* NB: Round up L and ALIGN to the widest integer mode for
     MAX_SIZE.  */
      mode = widest_fixed_size_mode_for_size(self,max_size, op);
      gcc_assert (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode) != CODE_FOR_nothing);
      unsigned HOST_WIDE_INT up = ROUND_UP (l, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
      if (up > l)
          l = up;
      align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);
  }

  align = alignment_for_piecewise_move(self,mtcs_reg_get_move_max_pieces/*!MOVE_MAX_PIECES*/(mtcsReg), align);

  while (max_size > 1 && l > 0){
      mode = widest_fixed_size_mode_for_size(self,max_size, op);
      gcc_assert (mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode) != CODE_FOR_nothing);
      unsigned int modesize = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);

      if (align >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)){
          unsigned HOST_WIDE_INT n_pieces = l / modesize;
          l %= modesize;
          switch (op){
            default:
              n_insns += n_pieces;
              break;
            case COMPARE_BY_PIECES:
              int batch = mtcsTarget/*!targetm.compare_by_pieces_branch_ratio*/->compare_by_pieces_branch_ratio(mtcsTarget,mode);
              int batch_ops = 4 * batch - 1;
              unsigned HOST_WIDE_INT full = n_pieces / batch;
              n_insns += full * batch_ops;
              if (n_pieces % batch != 0)
                  n_insns++;
              break;
          }
      }
      max_size = modesize;
  }
  gcc_assert (!l);
  return n_insns;
}

/* Expand a call to memcpy or memmove or memcmp, and return the result.
   TAILCALL is true if this is a tail call.  */
//原型 emit_block_op_via_libcall expr.h expr.cc
rtx mtcs_expr_emit_block_op_via_libcall (MtcsExpr *self,enum built_in_function fncode, rtx dst, rtx src,
               rtx size, bool tailcall)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  rtx dst_addr, src_addr;
  tree call_expr, dst_tree, src_tree, size_tree;
  machine_mode size_mode;

  /* Since dst and src are passed to a libcall, mark the corresponding
     tree EXPR as addressable.  */
  tree dst_expr = mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,dst);
  tree src_expr = mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,src);
  if (dst_expr)
    mark_addressable (dst_expr);
  if (src_expr)
    mark_addressable (src_expr);

  dst_addr = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (dst, 0));
  dst_addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, dst_addr);
  dst_tree = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ptr_type_node, dst_addr);

  src_addr =  mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (src, 0));
  src_addr =  mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,ptr_mode, src_addr);
  src_tree = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,ptr_type_node, src_addr);

  size_mode = TYPE_MODE (sizetype);
  size = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,size_mode, size, 1);
  size = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,size_mode, size);
  size_tree = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,sizetype, size);

  /* It is incorrect to use the libcall calling conventions for calls to
     memcpy/memmove/memcmp because they can be provided by the user.  */
  tree fn = builtin_decl_implicit (fncode);
  call_expr = mtcs_tree_build_call_expr/*!build_call_expr*/(mtcsTree,fn, 3, dst_tree, src_tree, size_tree);
  CALL_EXPR_TAILCALL (call_expr) = tailcall;

  return mtcs_calls_expand_call(mtcsCalls,call_expr, NULL_RTX, false);
}

/* Emit code to move a block SRC of type TYPE to a block DST,
   where DST is non-consecutive registers represented by a PARALLEL.
   SSIZE represents the total size of block ORIG_SRC in bytes, or -1
   if not known.  */
//原型 emit_group_load expr.h expr.cc
void mtcs_expr_emit_group_load (MtcsExpr *self,rtx dst, rtx src, tree type, poly_int64 ssize)
{
  rtx *tmps;
  int i;
  tmps = XALLOCAVEC (rtx, XVECLEN (dst, 0));
  emit_group_load_1(self,tmps, dst, src, type, ssize);
  /* Copy the extracted pieces into the proper (probable) hard regs.  */
  for (i = 0; i < XVECLEN (dst, 0); i++){
      rtx d = XEXP (XVECEXP (dst, 0, i), 0);
      if (d == NULL)
          continue;
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,d, tmps[i]);
  }
}

/* Generate code to push X onto the stack, assuming it has mode MODE and
   type TYPE.
   MODE is redundant except when X is a CONST_INT (since they don't
   carry mode info).
   SIZE is an rtx for the size of data to be copied (in bytes),
   needed only if X is BLKmode.
   Return true if successful.  May return false if asked to push a
   partial argument during a sibcall optimization (as specified by
   SIBCALL_P) and the incoming and outgoing pointers cannot be shown
   to not overlap.

   ALIGN (in bits) is maximum alignment we can assume.

   If PARTIAL and REG are both nonzero, then copy that many of the first
   bytes of X into registers starting with REG, and push the rest of X.
   The amount of space pushed is decreased by PARTIAL bytes.
   REG must be a hard register in this case.
   If REG is zero but PARTIAL is not, take any all others actions for an
   argument partially in registers, but do not actually load any
   registers.

   EXTRA is the amount in bytes of extra space to leave next to this arg.
   This is ignored if an argument block has already been allocated.

   On a machine that lacks real push insns, ARGS_ADDR is the address of
   the bottom of the argument block for this call.  We use indexing off there
   to store the arg.  On machines with push insns, ARGS_ADDR is 0 when a
   argument block has not been preallocated.

   ARGS_SO_FAR is the size of args previously pushed for this call.

   REG_PARM_STACK_SPACE is nonzero if functions require stack space
   for arguments passed in registers.  If nonzero, it will be the number
   of bytes required.  */
//原型 emit_push_insn expr.h expr.cc
bool mtcs_expr_emit_push_insn(MtcsExpr *self,rtx x, machine_mode mode, tree type, rtx size,
        unsigned int align, int partial, rtx reg, poly_int64 extra,
        rtx args_addr, rtx args_so_far, int reg_parm_stack_space,
        rtx alignment_pad, bool sibcall_p)
{

  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx xinner;
  pad_direction stack_direction= mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)? PAD_DOWNWARD : PAD_UPWARD;

  /* Decide where to pad the argument: PAD_DOWNWARD for below,
     PAD_UPWARD for above, or PAD_NONE for don't pad it.
     Default is below for small data on big-endian machines; else above.  */
  pad_direction where_pad = target_calls_function_arg_padding/*!targetm.calls.function_arg_padding*/(mtcsMachine->calls,mode, type);

  /* Invert direction if stack is post-decrement.
     FIXME: why?  */
  if (mtcs_func_get_stack_push_code/*!STACK_PUSH_CODE*/(mtcsFunc) == POST_DEC)
    if (where_pad != PAD_NONE)
      where_pad = (where_pad == PAD_DOWNWARD ? PAD_UPWARD : PAD_DOWNWARD);

  xinner = x;

  int nregs = partial / UNITS_PER_WORD;
  rtx *tmp_regs = NULL;
  int overlapping = 0;

  if (mode == mtcsMode->modes.M_BLKmode
      || (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
              && align < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))){
      /* Copy a block into the stack, entirely or partially.  */

      rtx temp;
      int used;
      int offset;
      int skip;

      offset = partial % (mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT);
      used = partial - offset;

      if (mode != mtcsMode->modes.M_BLKmode){
          /* A value is to be stored in an insufficiently aligned
             stack slot; copy via a suitably aligned slot if
             necessary.  */
          size = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), pMode);
          if (!MEM_P (xinner)){
              temp = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 1, 1);
              mtcs_expr_emit_move_insn(self,temp, xinner);
              xinner = temp;
          }
      }

      gcc_assert (size);

      /* USED is now the # of bytes we need not copy to the stack
     because registers will take care of them.  */

      if (partial != 0)
          xinner = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,xinner, mtcsMode->modes.M_BLKmode, used);

      /* If the partial register-part of the arg counts in its stack size,
     skip the part of stack space corresponding to the registers.
     Otherwise, start copying to the beginning of the stack space,
     by setting SKIP to 0.  */
      skip = (reg_parm_stack_space == 0) ? 0 : used;

//#ifdef PUSH_ROUNDING host=1 nvptx=0
//      /* NB: Let the backend known the number of bytes to push and
//     decide if push insns should be generated.  */
//      unsigned int push_size;
//      if (CONST_INT_P (size))
//    push_size = INTVAL (size);
//      else
//    push_size = 0;
//
//      /* Do it with several push insns if that doesn't take lots of insns
//     and if there is no difficulty with push insns that skip bytes
//     on the stack for alignment purposes.  */
//      if (args_addr == 0
//      && targetm.calls.push_argument (push_size)
//      && CONST_INT_P (size)
//      && skip == 0
//      && MEM_ALIGN (xinner) >= align
//      && can_move_by_pieces ((unsigned) INTVAL (size) - used, align)
//      /* Here we avoid the case of a structure whose weak alignment
//         forces many pushes of a small amount of data,
//         and such small pushes do rounding that causes trouble.  */
//      && ((!targetm.slow_unaligned_access (word_mode, align))
//          || align >= BIGGEST_ALIGNMENT
//          || known_eq (PUSH_ROUNDING (align / BITS_PER_UNIT),
//               align / BITS_PER_UNIT))
//      && known_eq (PUSH_ROUNDING (INTVAL (size)), INTVAL (size)))
//    {
//      /* Push padding now if padding above and stack grows down,
//         or if padding below and stack grows up.
//         But if space already allocated, this has already been done.  */
//      if (maybe_ne (extra, 0)
//          && args_addr == 0
//          && where_pad != PAD_NONE
//          && where_pad != stack_direction)
//        anti_adjust_stack (gen_int_mode (extra, Pmode));
//
//      move_by_pieces (NULL, xinner, INTVAL (size) - used, align,
//              RETURN_BEGIN);
//    }
//      else
//#endif /* PUSH_ROUNDING  */
    {
      rtx target;

      /* Otherwise make space on the stack and copy the data
         to the address of that space.  */

      /* Deduct words put into registers from the size we must copy.  */
      if (partial != 0){
          if (CONST_INT_P (size))
              size = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,INTVAL (size) - used);
          else
            size = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,GET_MODE (size), sub_optab, size,
                    mtcs_rtl_gen_int_mode(mtcsRTL,used, GET_MODE (size)),NULL_RTX, 0, OPTAB_LIB_WIDEN);
      }

      /* Get the address of the stack space.
         In this case, we do not deal with EXTRA separately.
         A single stack adjust will do.  */
      poly_int64 const_args_so_far;
      if (! args_addr){
          temp = mtcs_expr_push_block/*!push_block*/(self,size, extra, where_pad == PAD_DOWNWARD);
          extra = 0;
      }else if (poly_int_rtx_p (args_so_far, &const_args_so_far))
         temp = mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,mtcsMode->modes.M_BLKmode,
                mtcs_rtl_plus_constant(mtcsRTL,pMode, args_addr,skip + const_args_so_far));
      else
         temp =mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,mtcsMode->modes.M_BLKmode,
                mtcs_rtl_plus_constant(mtcsRTL,pMode,gen_rtx_PLUS (pMode,args_addr,args_so_far),skip));

      if (!mtcs_func_is_accumulate_outgoing_args/*!ACCUMULATE_OUTGOING_ARGS*/(mtcsFunc)){
          /* If the source is referenced relative to the stack pointer,
         copy it to another register to stabilize it.  We do not need
         to do this if we know that we won't be changing sp.  */

          if (reg_mentioned_p (mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL), temp)
              || reg_mentioned_p (mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL), temp))
            temp = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,temp);
      }

      target = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, temp);

      /* We do *not* set_mem_attributes here, because incoming arguments
         may overlap with sibling call outgoing arguments and we cannot
         allow reordering of reads from function arguments with stores
         to outgoing arguments of sibling calls.  We do, however, want
         to record the alignment of the stack slot.  */
      /* ALIGN may well be better aligned than TYPE, e.g. due to
         PARM_BOUNDARY.  Assume the caller isn't lying.  */
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,target, align);

      /* If part should go in registers and pushing to that part would
         overwrite some of the values that need to go into regs, load the
         overlapping values into temporary pseudos to be moved into the hard
         regs at the end after the stack pushing has completed.
         We cannot load them directly into the hard regs here because
         they can be clobbered by the block move expansions.
         See PR 65358.  */

      if (partial > 0 && reg != 0 && mode == mtcsMode->modes.M_BLKmode && GET_CODE (reg) != PARALLEL){
          overlapping = memory_load_overlap (self,XEXP (x, 0), temp, partial);
          if (overlapping > 0){
              gcc_assert (overlapping % UNITS_PER_WORD == 0);
              overlapping /= UNITS_PER_WORD;

              tmp_regs = XALLOCAVEC (rtx, overlapping);
              //word_mode通过mtcsrtl备份host，然后用device的值赋值，最后恢复
              for (int i = 0; i < overlapping; i++)
                tmp_regs[i] = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->word_mode);

              for (int i = 0; i < overlapping; i++)
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,tmp_regs[i],
                          mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,target, i, mode));
          }else if (overlapping == -1)
              overlapping = 0;
          /* Could not determine whether there is overlap.
             Fail the sibcall.  */
          else{
              overlapping = 0;
              if (sibcall_p)
                return false;
          }
      }

      /* If source is a constant VAR_DECL with a simple constructor,
             store the constructor to the stack instead of moving it.  */
      const_tree decl;
      HOST_WIDE_INT sz;
      if (partial == 0
          && MEM_P (xinner)
          && SYMBOL_REF_P (XEXP (xinner, 0))
          && (decl = SYMBOL_REF_DECL (XEXP (xinner, 0))) != NULL_TREE
          && VAR_P (decl)
          && TREE_READONLY (decl)
          && !TREE_SIDE_EFFECTS (decl)
          && immediate_const_ctor_p (DECL_INITIAL (decl), 2)
          && (sz = int_expr_size (DECL_INITIAL (decl))) > 0
          && CONST_INT_P (size)
          && INTVAL (size) == sz)
          mtcs_expr_store_constructor/*!store_constructor*/(self,DECL_INITIAL (decl), target, 0, sz, false);
      else
        mtcs_expr_emit_block_move/*!emit_block_move*/(self,target, xinner, size, BLOCK_OP_CALL_PARM);
    }
  }else if (partial > 0){
      /* Scalar partly in registers.  This case is only supported
     for fixed-wdth modes.  */
      int num_words = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode).to_constant ();
      num_words /= UNITS_PER_WORD;
      int i;
      int not_stack;
      /* # bytes of start of argument
     that we must make space for but need not store.  */
      int offset = partial % (mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(mtcsFunc) / BITS_PER_UNIT);
      int args_offset = INTVAL (args_so_far);
      int skip;

      /* Push padding now if padding above and stack grows down,
     or if padding below and stack grows up.
     But if space already allocated, this has already been done.  */
      if (maybe_ne (extra, 0)
      && args_addr == 0
      && where_pad != PAD_NONE
      && where_pad != stack_direction)
          mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,extra, pMode));

      /* If we make space by pushing it, we might as well push
     the real data.  Otherwise, we can leave OFFSET nonzero
     and leave the space uninitialized.  */
      if (args_addr == 0)
          offset = 0;

      /* Now NOT_STACK gets the number of words that we don't need to
     allocate on the stack.  Convert OFFSET to words too.  */
      not_stack = (partial - offset) / UNITS_PER_WORD;
      offset /= UNITS_PER_WORD;

      /* If the partial register-part of the arg counts in its stack size,
     skip the part of stack space corresponding to the registers.
     Otherwise, start copying to the beginning of the stack space,
     by setting SKIP to 0.  */
      skip = (reg_parm_stack_space == 0) ? 0 : not_stack;

      if (CONSTANT_P (x) && !mtcsTarget->legitimate_constant_p/*!targetm.legitimate_constant_p*/(mtcsTarget,mode, x))
          x = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,mode, x));

      /* If X is a hard register in a non-integer mode, copy it into a pseudo;
     SUBREGs of such registers are not allowed.  */
      if ((REG_P (x) && REGNO (x) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
       && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,GET_MODE (x)) != MODE_INT))
          x = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,x);

      /* Loop over all the words allocated on the stack for this arg.  */
      /* We can do it by words, because any scalar bigger than a word
     has a size a multiple of a word.  */
      tree word_mode_type = lang_hooks.types.type_for_mode (word_mode, 1);
      for (i = num_words - 1; i >= not_stack; i--)
        if (i >= not_stack + offset)
          if (!mtcs_expr_emit_push_insn/*!emit_push_insn*/(self,mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,x, i, mode),
                  word_mode, word_mode_type, NULL_RTX, align, 0,
                  NULL_RTX, 0, args_addr,
                  mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,args_offset + ((i - not_stack + skip) * UNITS_PER_WORD)),
                  reg_parm_stack_space, alignment_pad, sibcall_p))
            return false;
  }else{
      rtx addr;
      rtx dest;

      /* Push padding now if padding above and stack grows down,
     or if padding below and stack grows up.
     But if space already allocated, this has already been done.  */
      if (maybe_ne (extra, 0)
      && args_addr == 0
      && where_pad != PAD_NONE
      && where_pad != stack_direction)
          mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,extra, pMode));

//#ifdef PUSH_ROUNDING host=1 ptx=0
//      if (args_addr == 0 && targetm.calls.push_argument (0))
//    emit_single_push_insn (mode, x, type);
//      else
//#endif
    {
      addr = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, pMode, args_addr, args_so_far);
      dest = gen_rtx_MEM (mode, mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,mode, addr));

      /* We do *not* set_mem_attributes here, because incoming arguments
         may overlap with sibling call outgoing arguments and we cannot
         allow reordering of reads from function arguments with stores
         to outgoing arguments of sibling calls.  We do, however, want
         to record the alignment of the stack slot.  */
      /* ALIGN may well be better aligned than TYPE, e.g. due to
         PARM_BOUNDARY.  Assume the caller isn't lying.  */
      mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,dest, align);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,dest, x);
    }
  }

  /* Move the partial arguments into the registers and any overlapping
     values that we moved into the pseudos in tmp_regs.  */
  if (partial > 0 && reg != 0){
      /* Handle calls that pass values in multiple non-contiguous locations.
     The Irix 6 ABI has examples of this.  */
      if (GET_CODE (reg) == PARALLEL)
          emit_group_load (reg, x, type, -1);
      else{
          gcc_assert (partial % UNITS_PER_WORD == 0);
          mtcs_expr_move_block_to_reg (self,REGNO (reg), x, nregs - overlapping, mode);
          for (int i = 0; i < overlapping; i++)
              mtcs_expr_emit_move_insn(self,mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,
                    word_mode, REGNO (reg)+ nregs - overlapping + i),tmp_regs[i]);

      }
  }

  if (maybe_ne (extra, 0) && args_addr == 0 && where_pad == stack_direction)
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,mtcs_rtl_gen_int_mode(mtcsRTL,extra, pMode));

  if (alignment_pad && args_addr == 0)
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,alignment_pad);

  return true;
}

/* Return a MEM that contains constant EXP.  DEFER is as for
   output_constant_def and MODIFIER is as for expand_expr.  */
//原型 expand_expr_constant expr.cc
static rtx expand_expr_constant (MtcsExpr *self,tree exp, int defer, enum expand_modifier modifier)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  n_debug("mtcsexpr.c expand_expr_constant 00 defer:%d modifier:%d %s\n",defer,modifier,get_tree_code_name(TREE_CODE(exp)));
  rtx mem;
  mem =mtcs_asm_output_constant_def/*!output_constant_def*/(mtcsAsm,exp, defer);
  n_debug("mtcsexpr.c expand_expr_constant 11 mem:%p mem mode:%d EXPAND_INITIALIZER:%d\n",mem,GET_MODE(mem),EXPAND_INITIALIZER);
  if (modifier != EXPAND_INITIALIZER)
    mem = mtcs_explow_use_anchored_address/*!use_anchored_address*/(mtcsExplow,mem);
  n_debug("mtcsexpr.c expand_expr_constant 22 mem:%p mem mode:%d EXPAND_INITIALIZER:%d %d\n",
        mem,GET_MODE(mem),EXPAND_INITIALIZER,GET_MODE(XEXP (mem, 0)));
  return mem;
}


/* A subroutine of expand_expr_addr_expr.  Evaluate the address of EXP.
   The TARGET, TMODE and MODIFIER arguments are as for expand_expr.  */
//原型 expand_expr_addr_expr_1 expr.cc
static rtx expand_expr_addr_expr_1(MtcsExpr *self,tree exp, rtx target, scalar_int_mode tmode,
                 enum expand_modifier modifier, addr_space_t as)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsSsaAddress  *mtcsSsaAddress=mtcs_target_get_ssa_address(mtcsTarget);

  rtx result, subtarget;
  tree inner, offset;
  poly_int64 bitsize, bitpos;
  int unsignedp, reversep, volatilep = 0;
  machine_mode mode1;
  n_debug("mtcsexpr.c expand_expr_addr_expr_1 00  target:%p tmode:%d modifier:%d as:%d %s\n",
        target,tmode,modifier,as,get_tree_code_name(TREE_CODE(exp)) );

  /* If we are taking the address of a constant and are at the top level,
     we have to use output_constant_def since we can't call force_const_mem
     at top level.  */
  /* ??? This should be considered a front-end bug.  We should not be
     generating ADDR_EXPR of something that isn't an LVALUE.  The only
     exception here is STRING_CST.  */
  /* 如果我们要获取常量的地址并且位于顶层，
  则必须使用 output_constant_def，因为我们无法在顶层调用 force_const_mem。
  */
  /* ??? 这应该被视为前端错误。我们不应该
  生成非左值类型的 ADDR_EXPR。这里唯一的例外是 STRING_CST。*/
  if (CONSTANT_CLASS_P (exp)){
     n_debug("mtcsexpr.c expand_expr_addr_expr_1 11  target:%p tmode:%d modifier:%d as:%d EXPAND_SUM:%d\n",
               target,tmode,modifier,as,EXPAND_SUM);
      result = XEXP (expand_expr_constant(self,exp, 0, modifier), 0);
      n_debug("mtcsexpr.c expand_expr_addr_expr_1 --11  target:%p tmode:%d modifier:%d as:%d EXPAND_SUM:%d rtx mode:%d\n",
                     target,tmode,modifier,as,EXPAND_SUM,GET_MODE(result));
      if (modifier < EXPAND_SUM){
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 22  target:%p tmode:%d modifier:%d as:%d\n",target,tmode,modifier,as );
          result = mtcs_expr_force_operand/*!force_operand*/(self,result, target);
      }
      return result;
  }

  /* Everything must be something allowed by is_gimple_addressable.  */
  switch (TREE_CODE (exp)){
    case INDIRECT_REF:
      /* This case will happen via recursion for &a->b.  */
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 33  INDIRECT_REF \n");
      return mtcs_expr_expand_expr/*!expand_expr*/(self,TREE_OPERAND (exp, 0), target, tmode, modifier);
    case MEM_REF:
      {
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 44  MEM_REF %d \n",integer_zerop (TREE_OPERAND (exp, 1)));
        tree tem = TREE_OPERAND (exp, 0);
        if (!integer_zerop (TREE_OPERAND (exp, 1)))
          tem = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,tem, TREE_OPERAND (exp, 1));
        return mtcs_expr_expand_expr/*!expand_expr*/(self,tem, target, tmode, modifier);
      }
    case TARGET_MEM_REF:
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 55  TARGET_MEM_REF \n");
      return mtcs_ssa_address_addr_for_mem_ref/*!addr_for_mem_ref*/(mtcsSsaAddress,exp, as, true);

    case CONST_DECL:
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 66 CONST_DECL modifier < EXPAND_SUM:%d \n",modifier < EXPAND_SUM);
      /* Expand the initializer like constants above.  */
      result = XEXP (expand_expr_constant(self,DECL_INITIAL (exp), 0, modifier), 0);
      if (modifier < EXPAND_SUM)
          result = mtcs_expr_force_operand/*!force_operand*/(self,result, target);
      return result;

    case REALPART_EXPR:
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 77 REALPART_EXPR \n");

      /* The real part of the complex number is always first, therefore
     the address is the same as the address of the parent object.  */
      offset = 0;
      bitpos = 0;
      inner = TREE_OPERAND (exp, 0);
      break;

    case IMAGPART_EXPR:
      /* The imaginary part of the complex number is always second.
     The expression is therefore always offset by the size of the
     scalar type.  */
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 88 IMAGPART_EXPR \n");

      offset = 0;
      bitpos = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,
            mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,TREE_TYPE (exp)));
      inner = TREE_OPERAND (exp, 0);
      break;

    case COMPOUND_LITERAL_EXPR:
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 99 COMPOUND_LITERAL_EXPR \n");

      /* Allow COMPOUND_LITERAL_EXPR in initializers or coming from
     initializers, if e.g. rtl_for_decl_init is called on DECL_INITIAL
     with COMPOUND_LITERAL_EXPRs in it, or ARRAY_REF on a const static
     array with address of COMPOUND_LITERAL_EXPR in DECL_INITIAL;
     the initializers aren't gimplified.  */
      if (COMPOUND_LITERAL_EXPR_DECL (exp) && is_global_var (COMPOUND_LITERAL_EXPR_DECL (exp)))
        return expand_expr_addr_expr_1(self,COMPOUND_LITERAL_EXPR_DECL (exp),
                        target, tmode, modifier, as);
      /* FALLTHRU */
    default:
      /* If the object is a DECL, then expand it for its rtl.  Don't bypass
     expand_expr, as that can have various side effects; LABEL_DECLs for
     example, may not have their DECL_RTL set yet.  Expand the rtl of
     CONSTRUCTORs too, which should yield a memory reference for the
     constructor's contents.  Assume language specific tree nodes can
     be expanded in some interesting way.  */
       n_debug("mtcsexpr.c expand_expr_addr_expr_1 100 default tmode:%d modifier:%d \n",tmode,modifier);

      gcc_assert (TREE_CODE (exp) < LAST_AND_UNUSED_TREE_CODE);
      if (DECL_P (exp) || TREE_CODE (exp) == CONSTRUCTOR || TREE_CODE (exp) == COMPOUND_LITERAL_EXPR){
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 101 default EXPAND_INITIALIZER:%d EXPAND_CONST_ADDRESS:%d EXPAND_SUM:%d\n",
               EXPAND_INITIALIZER,EXPAND_CONST_ADDRESS,EXPAND_SUM);

          result = mtcs_expr_expand_expr/*!expand_expr*/(self,exp, target, tmode,
                    modifier == EXPAND_INITIALIZER ? EXPAND_INITIALIZER : EXPAND_CONST_ADDRESS);
              /* If the DECL isn't in memory, then the DECL wasn't properly
             marked TREE_ADDRESSABLE, which will be either a front-end
             or a tree optimizer bug.  */
          gcc_assert (MEM_P (result));
          result = XEXP (result, 0);
          n_debug("mtcsexpr.c expand_expr_addr_expr_1 102 default tmode:%d modifier:%d result mode:%d\n",
                tmode,modifier,GET_MODE(result));

              /* ??? Is this needed anymore?  */
          if (DECL_P (exp))
            TREE_USED (exp) = 1;

          if (modifier != EXPAND_INITIALIZER  && modifier != EXPAND_CONST_ADDRESS  && modifier != EXPAND_SUM)
            result = mtcs_expr_force_operand/*!force_operand*/(self,result, target);
          return result;
      }

      /* Pass FALSE as the last argument to get_inner_reference although
     we are expanding to RTL.  The rationale is that we know how to
     handle "aligning nodes" here: we can just bypass them because
     they won't change the final object whose address will be returned
     (they actually exist only for that purpose).  */
      inner = mtcs_expr_get_inner_reference/*!get_inner_reference*/(self,exp, &bitsize, &bitpos, &offset, &mode1,
                   &unsignedp, &reversep, &volatilep);
      break;
  }
  /* We must have made progress.  */
  gcc_assert (inner != exp);
  subtarget = offset || maybe_ne (bitpos, 0) ? NULL_RTX : target;
  /* For VIEW_CONVERT_EXPR, where the outer alignment is bigger than
     inner alignment, force the inner to be sufficiently aligned.  */
  if (CONSTANT_CLASS_P (inner)  && TYPE_ALIGN (TREE_TYPE (inner)) < TYPE_ALIGN (TREE_TYPE (exp))){
     n_debug("mtcsexpr.c expand_expr_addr_expr_1 102 \n");

      inner = copy_node (inner);
      TREE_TYPE (inner) = copy_node (TREE_TYPE (inner));
      SET_TYPE_ALIGN (TREE_TYPE (inner), TYPE_ALIGN (TREE_TYPE (exp)));
      TYPE_USER_ALIGN (TREE_TYPE (inner)) = 1;
  }
  result = expand_expr_addr_expr_1(self,inner, subtarget, tmode, modifier, as);
  if (offset){
     n_debug("mtcsexpr.c expand_expr_addr_expr_1 103 offset!=null\n");

     rtx tmp;
     if (modifier != EXPAND_NORMAL){
        n_debug("mtcsexpr.c expand_expr_addr_expr_1 104 modifier != EXPAND_NORMAL\n");

         result = mtcs_expr_force_operand/*!force_operand*/(self,result, NULL);
     }
      tmp = mtcs_expr_expand_expr/*!expand_expr*/(self,offset, NULL_RTX, tmode,
             modifier == EXPAND_INITIALIZER ? EXPAND_INITIALIZER : EXPAND_NORMAL);
      /* expand_expr is allowed to return an object in a mode other
     than TMODE.  If it did, we need to convert.  */
      if (GET_MODE (tmp) != VOIDmode && tmode != GET_MODE (tmp)){
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 105 TYPE_UNSIGNED (TREE_TYPE (offset)):%d\n",TYPE_UNSIGNED (TREE_TYPE (offset)));
         tmp = mtcs_expr_convert_modes/*!convert_modes*/(self,tmode, GET_MODE (tmp), tmp, TYPE_UNSIGNED (TREE_TYPE (offset)));
      }
      result =mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/(mtcsExplow,tmode, result, as);
      tmp = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/(mtcsExplow,tmode, tmp, as);
      if (modifier == EXPAND_SUM || modifier == EXPAND_INITIALIZER){
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 106\n");

          result = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, tmode, result, tmp);
      }else{
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 107\n");

          subtarget = maybe_ne (bitpos, 0) ? NULL_RTX : target;
          result = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,
                tmode, PLUS, result, tmp, subtarget,1, OPTAB_LIB_WIDEN);
      }
  }

  if (maybe_ne (bitpos, 0)){
     n_debug("mtcsexpr.c expand_expr_addr_expr_1 108\n");

      /* Someone beforehand should have rejected taking the address
     of an object that isn't byte-aligned.  */
      poly_int64 bytepos = exact_div (bitpos, BITS_PER_UNIT);
      result = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/(mtcsExplow,tmode, result, as);
      result = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,tmode, result, bytepos);
      if (modifier < EXPAND_SUM){
         n_debug("mtcsexpr.c expand_expr_addr_expr_1 109\n");
          result = mtcs_expr_force_operand/*!force_operand*/(self,result, target);
      }
  }
  return result;
}



/* A subroutine of expand_expr.  Evaluate EXP, which is an ADDR_EXPR.
   The TARGET, TMODE and MODIFIER arguments are as for expand_expr.  */
//原型 expand_expr_addr_expr expr.cc
static rtx expand_expr_addr_expr (MtcsExpr *self,tree exp, rtx target, machine_mode tmode,enum expand_modifier modifier)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  mtcs_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  addr_space_t as = ADDR_SPACE_GENERIC;
  scalar_int_mode address_mode = pMode;
  scalar_int_mode pointer_mode = ptr_mode;
  machine_mode rmode;
  rtx result;
  n_debug("mtcsexpr.c expand_expr_addr_expr 00 target:%p tmode:%d modifier:%d\n",target,tmode,modifier);
  /* Target mode of VOIDmode says "whatever's natural".  */
  if (tmode == VOIDmode)
    tmode = TYPE_MODE (TREE_TYPE (exp));

  if (POINTER_TYPE_P (TREE_TYPE (exp))){
      as = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (exp)));
      address_mode = target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
      pointer_mode = target_addr_space_pointer_mode/*!targetm.addr_space.pointer_mode*/(mtcsMachine->addrSpace,as);
      n_debug("mtcsexpr.c expand_expr_addr_expr 11 as:%d address_mode:%d pointer_mode:%d\n",as,address_mode,pointer_mode);
  }

  /* We can get called with some Weird Things if the user does silliness
     like "(short) &a".  In that case, convert_memory_address won't do
     the right thing, so ignore the given target mode.  */
  scalar_int_mode new_tmode = (tmode == pointer_mode ? pointer_mode : address_mode);
  n_debug("mtcsexpr.c expand_expr_addr_expr 22 new_tmode:%d\n",new_tmode);

  result = expand_expr_addr_expr_1(self,TREE_OPERAND (exp, 0), target,new_tmode, modifier, as);

  /* Despite expand_expr claims concerning ignoring TMODE when not
     strictly convenient, stuff breaks if we don't honor it.  Note
     that combined with the above, we only do this for pointer modes.  */
  rmode = GET_MODE (result);
  n_debug("mtcsexpr.c expand_expr_addr_expr 33 rmode:%d\n",rmode);

  if (rmode == VOIDmode)
    rmode = new_tmode;
  if (rmode != new_tmode){
     n_debug("mtcsexpr.c expand_expr_addr_expr 44 rmode:%d new_tmode:%d\n",rmode,new_tmode);
    result = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/(mtcsExplow,new_tmode, result, as);
  }
  return result;
}


/* Pushing data onto the stack.  */

/* Push a block of length SIZE (perhaps variable)
   and return an rtx to address the beginning of the block.
   The value may be virtual_outgoing_args_rtx.

   EXTRA is the number of bytes of padding to push in addition to SIZE.
   BELOW nonzero means this padding comes at low addresses;
   otherwise, the padding comes at high addresses.  */
//原型 push_block expr.h expr.cc
rtx mtcs_expr_push_block (MtcsExpr *self,rtx size, poly_int64 extra, int below)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx temp;
  size = mtcs_expr_convert_modes/*!convert_modes*/(self,pMode, ptr_mode, size, 1);
  if (CONSTANT_P (size))
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, size, extra));
  else if (REG_P (size) && known_eq (extra, 0))
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,size);
  else{
      temp = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,pMode, size);
      if (maybe_ne (extra, 0))
        temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,pMode, add_optab, temp,
                mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,extra, pMode), temp, 0, OPTAB_LIB_WIDEN);
      mtcs_explow_anti_adjust_stack/*!anti_adjust_stack*/(mtcsExplow,temp);
  }

  if (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc)){
      temp = mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL);
      if (maybe_ne (extra, 0) && below)
          temp = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, temp, extra);
  }else{
      poly_int64 csize;
      if (poly_int_rtx_p (size, &csize))
        temp =mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL),
                      -csize - (below ? 0 : extra));
      else if (maybe_ne (extra, 0) && !below)
        temp = gen_rtx_PLUS (pMode, mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL),
                mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,pMode,
                      mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, size, extra)));
      else
        temp = gen_rtx_PLUS (pMode, mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL),
                     mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,pMode, size));
  }

  return mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,mtcs_mode_get_narrowest_int_mode/*!NARROWEST_INT_MODE*/(mtcsMode), temp);
}

/* Add USE expressions to *CALL_FUSAGE for each of NREGS consecutive regs,
   starting at REGNO.  All of these registers must be hard registers.  */
//原型 use_regs expr.h expr.cc
void mtcs_expr_use_regs (MtcsExpr *self,rtx *call_fusage, int regno, int nregs)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  //原型 FIRST_PSEUDO_REGISTER rtl.h
  int firstPseudoRegister= mtcs_reg_get_first_pseudo_register(mtcsReg);
  int i;
  gcc_assert (regno + nregs <= firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);
  for (i = 0; i < nregs; i++)
      mtcs_expr_use_reg/*!use_reg*/(self,call_fusage,  mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[regno + i]);
}

/* Add USE expressions to *CALL_FUSAGE for each REG contained in the
   PARALLEL REGS.  This is for calls that pass values in multiple
   non-contiguous locations.  The Irix 6 ABI has examples of this.  */
//原型 use_group_regs expr.h expr.cc
void mtcs_expr_use_group_regs (MtcsExpr *self,rtx *call_fusage, rtx regs)
{
  int i;
  for (i = 0; i < XVECLEN (regs, 0); i++){
      rtx reg = XEXP (XVECEXP (regs, 0, i), 0);
      /* A NULL entry means the parameter goes both on the stack and in
     registers.  This can also be a MEM for targets that pass values
     partially on the stack and partially in registers.  */
      if (reg != 0 && REG_P (reg))
          mtcs_expr_use_reg/*!use_reg*/(self,call_fusage, reg);
  }
}

/* Copy all or part of a value X into registers starting at REGNO.
   The number of registers to be filled is NREGS.  */
//原型 move_block_to_reg expr.h expr.cc
void mtcs_expr_move_block_to_reg (MtcsExpr *self ,int regno, rtx x, int nregs, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  if (nregs == 0)
    return;

  if (CONSTANT_P (x) && !mtcsTarget->/*!targetm.legitimate_constant_p*/legitimate_constant_p(mtcsTarget,mode, x))
    x = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,mode, x));

  /* See if the machine can do this with a load multiple insn.  */
  if (target_rtx_have_load_multiple/*!targetm.have_load_multiple*/(mtcsMachine->tmrtx)){
      rtx_insn *last = mtcs_rtl_data_get_last_insn(mtcsRtlData);
      rtx first = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, regno);
      if (rtx_insn *pat = target_rtx_gen_load_multiple/*targetm.gen_load_multiple*/(mtcsMachine->tmrtx,
              first, x,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,nregs))){
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
          return;
      }else
          mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
  }

  for (int i = 0; i < nregs; i++)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, regno + i),
              mtcs_rtl_operand_subword_force/*!operand_subword_force*/(mtcsRTL,x, i, mode));
}

/* Emit code to move a block SRC to a block ORIG_DST of type TYPE,
   where SRC is non-consecutive registers represented by a PARALLEL.
   SSIZE represents the total size of block ORIG_DST, or -1 if not
   known.  */
//原型 emit_group_store expr.h expr.cc
void mtcs_expr_emit_group_store (MtcsExpr *self,rtx orig_dst, rtx src, tree type ATTRIBUTE_UNUSED,poly_int64 ssize)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  rtx *tmps, dst;
  int start, finish, i;
  machine_mode m = GET_MODE (orig_dst);

  gcc_assert (GET_CODE (src) == PARALLEL);

  if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,m)  && !MEM_P (orig_dst) && GET_CODE (orig_dst) != CONCAT){
      scalar_int_mode imode;
      if (mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,GET_MODE (orig_dst)).exists (&imode)){
          dst = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,imode);
          mtcs_expr_emit_group_store (self,dst, src, type, ssize);
          dst = gen_lowpart (GET_MODE (orig_dst), dst);
      }else{
          dst = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (orig_dst), ssize);
          mtcs_expr_emit_group_store (self,dst, src, type, ssize);
      }
      mtcs_expr_emit_move_insn(self,orig_dst, dst);
      return;
  }

  /* Check for a NULL entry, used to indicate that the parameter goes
     both on the stack and in registers.  */
  if (XEXP (XVECEXP (src, 0, 0), 0))
    start = 0;
  else
    start = 1;
  finish = XVECLEN (src, 0);
  tmps = XALLOCAVEC (rtx, finish);
  /* Copy the (probable) hard regs into pseudos.  */
  for (i = start; i < finish; i++){
      rtx reg = XEXP (XVECEXP (src, 0, i), 0);
      if (!REG_P (reg) || REGNO (reg) < mtcs_reg_get_first_pseudo_register(mtcsReg)/*! FIRST_PSEUDO_REGISTER*/){
          tmps[i] = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (reg));
          mtcs_expr_emit_move_insn(self,tmps[i], reg);
      }else
          tmps[i] = reg;
  }
  /* If we won't be storing directly into memory, protect the real destination
     from strange tricks we might play.  */
  dst = orig_dst;
  if (GET_CODE (dst) == PARALLEL){
      rtx temp;
      /* We can get a PARALLEL dst if there is a conditional expression in
     a return statement.  In that case, the dst and src are the same,
     so no action is necessary.  */
      if (rtx_equal_p (dst, src))
          return;
      /* It is unclear if we can ever reach here, but we may as well handle
     it.  Allocate a temporary, and split this into a store/load to/from
     the temporary.  */
      temp = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (dst), ssize);
      emit_group_store (temp, src, type, ssize);
      emit_group_load (dst, temp, type, ssize);
      return;
  }else if (!MEM_P (dst) && GET_CODE (dst) != CONCAT){
      machine_mode outer = GET_MODE (dst);
      machine_mode inner;
      poly_int64 bytepos;
      bool done = false;
      rtx temp;
      if (!REG_P (dst) || REGNO (dst) < mtcs_reg_get_first_pseudo_register(mtcsReg)/*! FIRST_PSEUDO_REGISTER*/)
          dst = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,outer);
      /* Make life a bit easier for combine: if the first element of the
     vector is the low part of the destination mode, use a paradoxical
     subreg to initialize the destination.  */
      if (start < finish){
          inner = GET_MODE (tmps[start]);
          bytepos = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,inner, outer);
          if (known_eq (rtx_to_poly_int64 (XEXP (XVECEXP (src, 0, start), 1)),bytepos)){
              temp = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,outer, tmps[start], inner, 0);
              if (temp){
                  mtcs_expr_emit_move_insn(self,dst, temp);
                  done = true;
                  start++;
              }
          }
      }
      /* If the first element wasn't the low part, try the last.  */
      if (!done  && start < finish - 1){
          inner = GET_MODE (tmps[finish - 1]);
          bytepos = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,inner, outer);
          if (known_eq (rtx_to_poly_int64 (XEXP (XVECEXP (src, 0,finish - 1), 1)), bytepos)){
              temp = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,outer, tmps[finish - 1], inner, 0);
              if (temp){
                  mtcs_expr_emit_move_insn(self,dst, temp);
                  done = true;
                  finish--;
              }
          }
      }
      /* Otherwise, simply initialize the result to zero.  */
      if (!done)
          mtcs_expr_emit_move_insn(self,dst, CONST0_RTX (outer));
  }
  /* Process the pieces.  */
  for (i = start; i < finish; i++){
      poly_int64 bytepos = rtx_to_poly_int64 (XEXP (XVECEXP (src, 0, i), 1));
      machine_mode mode = GET_MODE (tmps[i]);
      poly_int64 bytelen = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
      poly_uint64 adj_bytelen;
      rtx dest = dst;
      /* Handle trailing fragments that run over the size of the struct.
     It's the target's responsibility to make sure that the fragment
     cannot be strictly smaller in some cases and strictly larger
     in others.  */
      gcc_checking_assert (ordered_p (bytepos + bytelen, ssize));
      if (known_size_p (ssize) && maybe_gt (bytepos + bytelen, ssize))
          adj_bytelen = ssize - bytepos;
      else
          adj_bytelen = bytelen;
      /* Deal with destination CONCATs by either storing into one of the parts
     or doing a copy after storing into a register or stack temporary.  */
      if (GET_CODE (dst) == CONCAT){
          if (known_le (bytepos + adj_bytelen,mtcs_mode_get_size(mtcsMode,GET_MODE (XEXP (dst, 0)))))
            dest = XEXP (dst, 0);
          else if (known_ge (bytepos, mtcs_mode_get_size(mtcsMode,GET_MODE (XEXP (dst, 0))))){
              bytepos -= mtcs_mode_get_size(mtcsMode,GET_MODE (XEXP (dst, 0)));
              dest = XEXP (dst, 1);
          }else{
              machine_mode dest_mode = GET_MODE (dest);
              machine_mode tmp_mode = GET_MODE (tmps[i]);
              scalar_int_mode dest_imode;
              gcc_assert (known_eq (bytepos, 0) && XVECLEN (src, 0));
              /* If the source is a single scalar integer register, and the
             destination has a complex mode for which a same-sized integer
             mode exists, then we can take the left-justified part of the
             source in the complex mode.  */
              if (finish == start + 1
                  && REG_P (tmps[i])
                  && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,tmp_mode)
                  && mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,dest_mode)
                  && mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,dest_mode).exists (&dest_imode)){
                  const scalar_int_mode tmp_imode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,tmp_mode);

                  if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,dest_imode) <
                          mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,tmp_imode)){
                      dest = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,dest_imode);
                      if (BYTES_BIG_ENDIAN)
                        tmps[i] =mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, tmp_mode, tmps[i],
                                mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode, tmp_imode)
                                    - mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode, dest_imode),NULL_RTX, 1);
                      mtcs_expr_emit_move_insn(self,dest, gen_lowpart (dest_imode, tmps[i]));
                      dst = gen_lowpart (dest_mode, dest);
                  }else
                    dst = gen_lowpart (dest_mode, tmps[i]);
              }
              /* Otherwise spill the source onto the stack using the more
             aligned of the two modes.  */
              else if (mtcs_mode_get_alignment(mtcsMode,dest_mode) >= mtcs_mode_get_alignment(mtcsMode,tmp_mode)){
                  dest = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,dest_mode,mtcs_mode_get_size(mtcsMode,dest_mode));
                  mtcs_expr_emit_move_insn(self,mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,dest, tmp_mode, bytepos),tmps[i]);
                  dst = dest;
              }else{
                  dest =mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,tmp_mode,mtcs_mode_get_size(mtcsMode,tmp_mode));
                  mtcs_expr_emit_move_insn(self,dest, tmps[i]);
                  dst = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,dest, dest_mode, bytepos);
              }
              break;
          }
      }
      /* Handle trailing fragments that run over the size of the struct.  */
      if (known_size_p (ssize) && maybe_gt (bytepos + bytelen, ssize)){
      /* store_bit_field always takes its value from the lsb.
         Move the fragment to the lsb if it's not already there.  */
      if (
#ifdef BLOCK_REG_PADDING //host=0 nvptx=0
          BLOCK_REG_PADDING (GET_MODE (orig_dst), type, i == start)
          == (BYTES_BIG_ENDIAN ? PAD_UPWARD : PAD_DOWNWARD)
#else
          BYTES_BIG_ENDIAN
#endif
          )
        {
          poly_int64 shift = (bytelen - (ssize - bytepos)) * BITS_PER_UNIT;
          tmps[i] =mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, tmps[i], shift, tmps[i], 0);
        }
      /* Make sure not to write past the end of the struct.  */
      mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,dest,
               adj_bytelen * BITS_PER_UNIT, bytepos * BITS_PER_UNIT,
               bytepos * BITS_PER_UNIT, ssize * BITS_PER_UNIT - 1,
               VOIDmode, tmps[i], false, false);
      }
      /* Optimize the access just a bit.  */
      else if (MEM_P (dest)
           && (!mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,
                   mode, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, dest))
           || mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, dest) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
           && multiple_p (bytepos * BITS_PER_UNIT, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
           && known_eq (bytelen, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode)))
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self, mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,dest, mode, bytepos), tmps[i]);
      else
          mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,dest, bytelen * BITS_PER_UNIT, bytepos * BITS_PER_UNIT,
             0, 0, mode, tmps[i], false, false);
  }

  /* Copy from the pseudo into the (probable) hard reg.  */
  if (orig_dst != dst)
    mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,orig_dst, dst);
}

/* expand_expr: generate code for computing expression EXP.
   An rtx for the computed value is returned.  The value is never null.
   In the case of a void EXP, const0_rtx is returned.

   The value may be stored in TARGET if TARGET is nonzero.
   TARGET is just a suggestion; callers must assume that
   the rtx returned may not be the same as TARGET.

   If TARGET is CONST0_RTX, it means that the value will be ignored.

   If TMODE is not VOIDmode, it suggests generating the
   result in mode TMODE.  But this is done only when convenient.
   Otherwise, TMODE is ignored and the value generated in its natural mode.
   TMODE is just a suggestion; callers must assume that
   the rtx returned may not have mode TMODE.

   Note that TARGET may have neither TMODE nor MODE.  In that case, it
   probably will not be used.

   If MODIFIER is EXPAND_SUM then when EXP is an addition
   we can return an rtx of the form (MULT (REG ...) (CONST_INT ...))
   or a nest of (PLUS ...) and (MINUS ...) where the terms are
   products as above, or REG or MEM, or constant.
   Ordinarily in such cases we would output mul or add instructions
   and then return a pseudo reg containing the sum.

   EXPAND_INITIALIZER is much like EXPAND_SUM except that
   it also marks a label as absolutely required (it can't be dead).
   It also makes a ZERO_EXTEND or SIGN_EXTEND instead of emitting extend insns.
   This is used for outputting expressions used in initializers.

   EXPAND_CONST_ADDRESS says that it is okay to return a MEM
   with a constant address even if that address is not normally legitimate.
   EXPAND_INITIALIZER and EXPAND_SUM also have this effect.

   EXPAND_STACK_PARM is used when expanding to a TARGET on the stack for
   a call parameter.  Such targets require special care as we haven't yet
   marked TARGET so that it's safe from being trashed by libcalls.  We
   don't want to use TARGET for anything but the final result;
   Intermediate values must go elsewhere.   Additionally, calls to
   emit_block_move will be flagged with BLOCK_OP_CALL_PARM.

   If EXP is a VAR_DECL whose DECL_RTL was a MEM with an invalid
   address, and ALT_RTL is non-NULL, then *ALT_RTL is set to the
   DECL_RTL of the VAR_DECL.  *ALT_RTL is also set if EXP is a
   COMPOUND_EXPR whose second argument is such a VAR_DECL, and so on
   recursively.
   If the result can be stored at TARGET, and ALT_RTL is non-NULL,
   then *ALT_RTL is set to TARGET (before legitimziation).

   If INNER_REFERENCE_P is true, we are expanding an inner reference.
   In this case, we don't adjust a returned MEM rtx that wouldn't be
   sufficiently aligned for its mode; instead, it's up to the caller
   to deal with it afterwards.  This is used to make sure that unaligned
   base objects for which out-of-bounds accesses are supported, for
   example record types with trailing arrays, aren't realigned behind
   the back of the caller.
   The normal operating mode is to pass FALSE for this parameter.  */
//原型 expand_expr_real expr.h expr.cc
rtx mtcs_expr_expand_expr_real (MtcsExpr *self,tree exp, rtx target, machine_mode tmode,
          enum expand_modifier modifier, rtx *alt_rtl, bool inner_reference_p)
{
  rtx ret;
  /* Handle ERROR_MARK before anybody tries to access its type.  */
  if (TREE_CODE (exp) == ERROR_MARK
      || (TREE_CODE (TREE_TYPE (exp)) == ERROR_MARK)){
      ret = CONST0_RTX (tmode);
      return ret ? ret : const0_rtx;
  }
  ret = mtcs_expr_expand_expr_real_1/*!expand_expr_real_1*/(self,exp, target, tmode, modifier, alt_rtl,inner_reference_p);
  return ret;
}

//原型 expand_expr_real_1 expr.h expr.cc
rtx mtcs_expr_expand_expr_real_1 (MtcsExpr *self,tree exp, rtx target, machine_mode tmode,
            enum expand_modifier modifier, rtx *alt_rtl,bool inner_reference_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsExpand  *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsBuiltins  *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
  MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);
  MtcsInternalFn *mtcsInternalFn = mtcs_target_get_internal_fn(mtcsTarget);
  MtcsSsaAddress  *mtcsSsaAddress=mtcs_target_get_ssa_address(mtcsTarget);
  MtcsMachine  *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  mtcs_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx op0, op1, temp, decl_rtl;
  tree type;
  int unsignedp;
  machine_mode mode, dmode;
  enum tree_code code = TREE_CODE (exp);
  rtx subtarget, original_target;
  int ignore;
  bool reduce_bit_field;
  location_t loc = EXPR_LOCATION (exp);
  struct separate_ops ops;
  tree treeop0, treeop1, treeop2;
  tree ssa_name = NULL_TREE;
  gimple *g;
  machine_mode typeMode,declMode;
  scalar_int_mode scalarIntMode;

  /* Some ABIs define padding bits in _BitInt uninitialized.  Normally, RTL
     expansion sign/zero extends integral types with less than mode precision
     when reading from bit-fields and after arithmetic operations (see
     REDUCE_BIT_FIELD in expand_expr_real_2) and on subsequent loads relies
     on those extensions to have been already performed, but because of the
     above for _BitInt they need to be sign/zero extended when reading from
     locations that could be exposed to ABI boundaries (when loading from
     objects in memory, or function arguments, return value).  Because we
     internally extend after arithmetic operations, we can avoid doing that
     when reading from SSA_NAMEs of vars.  */
#define EXTEND_BITINT(expr) \
  ((TREE_CODE (type) == BITINT_TYPE                 \
    && reduce_bit_field                         \
    && mode != mtcsMode->modes.M_BLKmode                          \
    && modifier != EXPAND_MEMORY                    \
    && modifier != EXPAND_WRITE                     \
    && modifier != EXPAND_INITIALIZER                   \
    && modifier != EXPAND_CONST_ADDRESS)                \
   ? reduce_to_bit_field_precision ((self),(expr), NULL_RTX, type) : (expr))

  type = TREE_TYPE (exp);
  mode = TYPE_MODE (type);
  //mode=mtcs_mode_host2device_by_tree(mtcsMode,type,mode);

  unsignedp = TYPE_UNSIGNED (type);
  treeop0 = treeop1 = treeop2 = NULL_TREE;
  if (!VL_EXP_CLASS_P (exp))
    switch (TREE_CODE_LENGTH (code)){
        default:
        case 3: treeop2 = TREE_OPERAND (exp, 2); /* FALLTHRU */
        case 2: treeop1 = TREE_OPERAND (exp, 1); /* FALLTHRU */
        case 1: treeop0 = TREE_OPERAND (exp, 0); /* FALLTHRU */
        case 0: break;
    }
  ops.code = code;
  ops.type = type;
  ops.op0 = treeop0;
  ops.op1 = treeop1;
  ops.op2 = treeop2;
  ops.location = loc;

  ignore = (target == const0_rtx  || ((CONVERT_EXPR_CODE_P (code)
         || code == COND_EXPR || code == VIEW_CONVERT_EXPR) && TREE_CODE (type) == VOID_TYPE));

  /* An operation in what may be a bit-field type needs the
     result to be reduced to the precision of the bit-field type,
     which is narrower than that of the type's mode.  */
  reduce_bit_field = (!ignore   && INTEGRAL_TYPE_P (type)
     && !mtcs_tree_type_has_mode_precision_p/*!type_has_mode_precision_p*/(mtcsTree,type));
  n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 00 typeMode:%d mode:%d unsignedp:%d ignore:%d reduce_bit_field:%d code:%s\n",
        TYPE_MODE (type),mode,unsignedp,ignore,reduce_bit_field,get_tree_code_name(code));
  //aet_print_tree(exp);

  /* If we are going to ignore this result, we need only do something
     if there is a side-effect somewhere in the expression.  If there
     is, short-circuit the most common cases here.  Note that we must
     not call expand_expr with anything but const0_rtx in case this
     is an initial expansion of a size that contains a PLACEHOLDER_EXPR.  */

  if (ignore){
      if (! TREE_SIDE_EFFECTS (exp))
          return const0_rtx;

       /* Ensure we reference a volatile object even if value is ignored, but
       don't do this if all we are doing is taking its address.  */
      if (TREE_THIS_VOLATILE (exp)
          && TREE_CODE (exp) != FUNCTION_DECL
          && mode != VOIDmode && mode != mtcsMode->modes.M_BLKmode
          && modifier != EXPAND_CONST_ADDRESS){
          temp = mtcs_expr_expand_expr(self,exp, NULL_RTX, VOIDmode, modifier);
          if (MEM_P (temp))
              mtcs_explow_copy_to_reg/*!copy_to_reg */(mtcsExplow,temp);
          return const0_rtx;
      }

      if (TREE_CODE_CLASS (code) == tcc_unary
          || code == BIT_FIELD_REF
          || code == COMPONENT_REF
          || code == INDIRECT_REF)
          return mtcs_expr_expand_expr(self,treeop0, const0_rtx, VOIDmode,modifier);

      else if (TREE_CODE_CLASS (code) == tcc_binary
           || TREE_CODE_CLASS (code) == tcc_comparison
           || code == ARRAY_REF || code == ARRAY_RANGE_REF){
          mtcs_expr_expand_expr(self,treeop0, const0_rtx, VOIDmode, modifier);
          mtcs_expr_expand_expr(self,treeop1, const0_rtx, VOIDmode, modifier);
          return const0_rtx;
      }
      target = 0;
  }

  if (reduce_bit_field && modifier == EXPAND_STACK_PARM)
    target = 0;
  /* Use subtarget as the target for operand 0 of a binary operation.  */
  subtarget = get_subtarget(self,target);
  original_target = target;
  n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 11 target:%p  subtarget:%p\n",target,subtarget);
  switch (code){
    case LABEL_DECL:
      {
        tree function = decl_function_context (exp);
        temp = mtcs_stmt_label_rtx/*!label_rtx*/(mtcsStmt,exp);
        temp = gen_rtx_LABEL_REF (pMode, temp);
        if (function != current_function_decl  && function != 0)
          LABEL_REF_NONLOCAL_P (temp) = 1;
        temp = gen_rtx_MEM (FUNCTION_MODE, temp);
        return temp;
      }

    case SSA_NAME:
      /* ??? ivopts calls expander, without any preparation from
         out-of-ssa.  So fake instructions as if this was an access to the
     base variable.  This unnecessarily allocates a pseudo, see how we can
     reuse it, if partition base vars have it set already.  */
      if (!mtcsExpand->currently_expanding_to_rtl){
          tree var = SSA_NAME_VAR (exp);
          if (var && DECL_RTL_SET_P (var))
            return mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,var);
          return mtcs_rtl_gen_raw_REG/*gen_raw_REG*/(mtcsRTL,TYPE_MODE (TREE_TYPE (exp)),
                mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1);
      }

      g = get_gimple_for_ssa_name (exp);
      /* For EXPAND_INITIALIZER try harder to get something simpler.  */
      if (g == NULL
          && modifier == EXPAND_INITIALIZER
          && !SSA_NAME_IS_DEFAULT_DEF (exp)
          && (optimize || !SSA_NAME_VAR (exp)
              || DECL_IGNORED_P (SSA_NAME_VAR (exp)))
          && stmt_is_replaceable_p (SSA_NAME_DEF_STMT (exp)))
          g = SSA_NAME_DEF_STMT (exp);
      n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 22 ssa 处理 g:%p\n",g);
      aet_print_gimple(g);
      if (safe_is_a <gassign *> (g))
         return mtcs_expr_expand_expr_real_gassign/*!expand_expr_real_gassign*/(self,as_a<gassign *> (g),
                       target, tmode,modifier, alt_rtl, inner_reference_p);
      else if (safe_is_a <gcall *> (g)){
         /* ???  internal call expansion doesn't follow the usual API
            of returning the destination RTX and being passed a desired
            target.  */
         n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 22aa ssa 处理 g:%p\n",g);
         rtx dest = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,TYPE_MODE (TREE_TYPE (exp)));
         tree tmplhs = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (exp), dest);
         gimple_call_set_lhs (g, tmplhs);
         mtcs_internal_fn_expand_internal_call/*!expand_internal_call*/(mtcsInternalFn,as_a <gcall *> (g));
         gimple_call_set_lhs (g, exp);
         return dest;
      }

      if (g)
        return mtcs_expr_expand_expr_real_gassign/*!expand_expr_real_gassign*/(self,as_a<gassign *> (g),
                target, tmode,modifier, alt_rtl, inner_reference_p);
      n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 33 ssa 处理 expr:%p\n",exp);
      ssa_name = exp;
      decl_rtl = get_rtx_for_ssa_name (ssa_name);
      exp = SSA_NAME_VAR (ssa_name);
      n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 44 decl_rtl:%p expr:%p\n",decl_rtl,exp);
      //aet_print_tree(exp);
      /* Optimize and avoid to EXTEND_BITINIT doing anything if it is an
     SSA_NAME computed within the current function.  In such case the
     value have been already extended before.  While if it is a function
     parameter, result or some memory location, we need to be prepared
     for some other compiler leaving the bits uninitialized.  */
      if (!exp || VAR_P (exp))
          reduce_bit_field = false;
      goto expand_decl_rtl;

    case VAR_DECL:
      /* Allow accel compiler to handle variables that require special
     treatment, e.g. if they have been modified in some way earlier in
     compilation by the adjust_private_decl OpenACC hook.  */
//      if (flag_openacc && targetm.goacc.expand_var_decl){//mtcs不需要
//          temp = targetm.goacc.expand_var_decl (exp);
//          if (temp)
//            return temp;
//      }
      /* Expand const VAR_DECLs with CONSTRUCTOR initializers that
     have scalar integer modes to a reg via store_constructor.  */
      if (TREE_READONLY (exp)
      && !TREE_SIDE_EFFECTS (exp)
      && (modifier == EXPAND_NORMAL || modifier == EXPAND_STACK_PARM)
      && immediate_const_ctor_p (DECL_INITIAL (exp))
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,TYPE_MODE (TREE_TYPE (exp)))
      && mtcsRtlData/*!crtl*/->emit.regno_pointer_align_length
      && !target){
          machine_mode mode=TYPE_MODE (TREE_TYPE (exp));//mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode/*!TYPE_MODE (TREE_TYPE (exp))*/);
          mtcs_expr_store_constructor/*!store_constructor*/(self,DECL_INITIAL (exp), target, 0,
                     int_expr_size (DECL_INITIAL (exp)), false);
          n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 99aa  变量处理 decl_rtl:%p expr:%p\n",decl_rtl,exp);

          return target;
      }
      /* ... fall through ...  */

    case PARM_DECL:
      /* If a static var's type was incomplete when the decl was written,
     but the type is complete now, lay out the decl now.  */
      if (DECL_SIZE (exp) == 0  && COMPLETE_OR_UNBOUND_ARRAY_TYPE_P (TREE_TYPE (exp))
        && (TREE_STATIC (exp) || DECL_EXTERNAL (exp)))
         mtcs_stor_layout_layout_decl/*!layout_decl*/(mtcsStorLayout,exp, 0);

      /* fall through */

    case FUNCTION_DECL:
    case RESULT_DECL:
      n_debug("mtcsexpr.cc mtcs_expr_expand_expr_real_1 99  变量处理 decl_rtl:%p expr:%p %s exp->decl_with_rtl.rtl:%p\n",
               decl_rtl,exp,get_tree_code_name(TREE_CODE(exp)),exp->decl_with_rtl.rtl);
      aet_print_tree(exp);
      if(exp && TREE_CODE(exp)==VAR_DECL){
         location_t loc=DECL_SOURCE_LOCATION(exp);
         n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 变量属性 :%p %d %d %d %d %d loc:%llu\n",exp->decl_with_rtl.rtl,
         VAR_P (exp),TREE_STATIC (exp),TREE_PUBLIC (exp), DECL_EXTERNAL (exp),DECL_REGISTER (exp),loc);
      }
      n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 xtt :%p\n",exp->decl_with_rtl.rtl);
      decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,exp);
      n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 xx %p expr:%p %d %d %d\n",
            decl_rtl,exp,GET_MODE(decl_rtl),DECL_MODE (exp),mode);
 expand_decl_rtl:
      gcc_assert (decl_rtl);
      //declMode=mtcs_mode_host2device_by_tree(mtcsMode,exp,DECL_MODE (exp));
      //typeMode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));

      /* DECL_MODE might change when TYPE_MODE depends on attribute target
     settings for VECTOR_TYPE_P that might switch for the function.  */
      n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 00 %p expr:%p\n",decl_rtl,exp);

      if (mtcsExpand->currently_expanding_to_rtl
      && code == VAR_DECL && MEM_P (decl_rtl)
      && VECTOR_TYPE_P (type) && exp && DECL_MODE (exp) != mode){
         n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 11 %p expr:%p\n",decl_rtl,exp);
         decl_rtl = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,decl_rtl, TYPE_MODE (type), 0);
      }else{
         n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 22 %p expr:%p\n",decl_rtl,exp);
         decl_rtl = copy_rtx (decl_rtl);
      }
      /* Record writes to register variables.  */
      if (modifier == EXPAND_WRITE   && REG_P (decl_rtl)  && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,decl_rtl)){
         n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 33 %p expr:%p\n",decl_rtl,exp);
         mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,&mtcsRtlData/*!crtl*/->asm_clobbers,
              GET_MODE (decl_rtl), REGNO (decl_rtl));
      }

      /* Ensure variable marked as used even if it doesn't go through
     a parser.  If it hasn't be used yet, write out an external
     definition.  */
      if (exp)
          TREE_USED (exp) = 1;
      /* Show we haven't gotten RTL for this yet.  */
      temp = 0;
      /* Variables inherited from containing functions should have
     been lowered by this point.  */
      if (exp){
          tree context = decl_function_context (exp);
          gcc_assert (SCOPE_FILE_SCOPE_P (context) || context == current_function_decl
                  || TREE_STATIC (exp) || DECL_EXTERNAL (exp)
                  /* ??? C++ creates functions that are not
                 TREE_STATIC.  */
                  || TREE_CODE (exp) == FUNCTION_DECL);
      }

      /* This is the case of an array whose size is to be determined
     from its initializer, while the initializer is still being parsed.
     ??? We aren't parsing while expanding anymore.  */

      if (MEM_P (decl_rtl) && REG_P (XEXP (decl_rtl, 0))){
         n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 44 %p expr:%p\n",decl_rtl,exp);

          temp = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,decl_rtl);
      }
      /* If DECL_RTL is memory, we are in the normal case and the
     address is not valid, get the address into a register.  */

      else if (MEM_P (decl_rtl) && modifier != EXPAND_INITIALIZER){
          if (alt_rtl)
            *alt_rtl = decl_rtl;
          decl_rtl = mtcs_explow_use_anchored_address/*!use_anchored_address*/(mtcsExplow,decl_rtl);
          if (modifier != EXPAND_CONST_ADDRESS && modifier != EXPAND_SUM
              && !mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,exp ? DECL_MODE (exp)
                               : GET_MODE (decl_rtl), XEXP (decl_rtl, 0),mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,decl_rtl)))
            temp = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,decl_rtl, copy_rtx (XEXP (decl_rtl, 0)));
      }

      /* If we got something, return it.  But first, set the alignment
     if the address is a register.  */
      if (temp != 0){
          if (exp && MEM_P (temp) && REG_P (XEXP (temp, 0)))
              mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,XEXP (temp, 0), DECL_ALIGN (exp));
      }else if (MEM_P (decl_rtl))
          temp = decl_rtl;

      if (temp != 0){
          if (MEM_P (temp)
              && modifier != EXPAND_WRITE
              && modifier != EXPAND_MEMORY
              && modifier != EXPAND_INITIALIZER
              && modifier != EXPAND_CONST_ADDRESS
              && modifier != EXPAND_SUM
              && !inner_reference_p
              && mode != mtcsMode->modes.M_BLKmode
              && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,temp) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
            temp = expand_misaligned_mem_ref(self,temp, mode, unsignedp,mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,temp), NULL_RTX, NULL);
          n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 44aa %p expr:%p\n",decl_rtl,exp);

          return EXTEND_BITINT (temp);
      }

      if (exp)
          dmode = DECL_MODE (exp);
      else
          dmode = TYPE_MODE (TREE_TYPE (ssa_name));
      n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 55 %p expr:%p dmode:%d\n",decl_rtl,exp,dmode);

      /* If the mode of DECL_RTL does not match that of the decl,
     there are two cases: we are dealing with a BLKmode value
     that is returned in a register, or we are dealing with
     a promoted value.  In the latter case, return a SUBREG
     of the wanted mode, but mark it so that we know that it
     was already extended.  */
      if (REG_P (decl_rtl) && dmode !=mtcsMode->modes.M_BLKmode && GET_MODE (decl_rtl) != dmode){
          machine_mode pmode;
          n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 66 %p expr:%p dmode:%d\n",decl_rtl,exp,dmode);

          /* Get the signedness to be used for this variable.  Ensure we get
             the same mode we got when the variable was declared.  */
          if (code != SSA_NAME){
             n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 77 %p expr:%p dmode:%d\n",decl_rtl,exp,dmode);

            pmode = mtcs_mode_promote_decl_mode/*!promote_decl_mode*/(mtcsMode,exp, &unsignedp);
          }else if ((g = SSA_NAME_DEF_STMT (ssa_name))  && gimple_code (g) == GIMPLE_CALL && !gimple_call_internal_p (g)){
             n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 88 %p expr:%p dmode:%p\n",decl_rtl,exp,dmode);

            pmode = mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,type, mode, &unsignedp, gimple_call_fntype (g),2);
          }else{
             n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 99 %p expr:%p dmode:%d\n",decl_rtl,exp,dmode);

            pmode = mtcs_mode_promote_ssa_mode/*!promote_ssa_mode*/(mtcsMode,ssa_name, &unsignedp);
          }
          gcc_assert (GET_MODE (decl_rtl) == pmode);
          n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 100 %p expr:%p dmode:%d\n",decl_rtl,exp,pmode);

          /* Some ABIs require scalar floating point modes to be passed
             in a wider scalar integer mode.  We need to explicitly
             truncate to an integer mode of the correct precision before
             using a SUBREG to reinterpret as a floating point value.  */
          if (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,mode)
                  && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,pmode)
                  && known_lt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pmode))){
             n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 expand_decl_rtl 101 %p expr:%p dmode:%d\n",decl_rtl,exp,pmode);

            return mtcs_expr_convert_wider_int_to_float/*!convert_wider_int_to_float*/(self,mode, pmode, decl_rtl);
          }

          temp = gen_lowpart_SUBREG (mode, decl_rtl);
          SUBREG_PROMOTED_VAR_P (temp) = 1;
          SUBREG_PROMOTED_SET (temp, unsignedp);
          return EXTEND_BITINT (temp);
      }

      return EXTEND_BITINT (decl_rtl);

    case INTEGER_CST:
      {
        if (TREE_CODE (type) == BITINT_TYPE){
            unsigned int prec = TYPE_PRECISION (type);
            struct bitint_info info;
            bool ok = target_c_bitint_type_info/*!targetm.c.bitint_type_info*/(mtcsMachine->c,prec, &info);
            gcc_assert (ok);
            scalar_int_mode limb_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,info.limb_mode);
            unsigned int limb_prec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode);
            if (prec > limb_prec && prec >mtcs_mode_get_max_fixed_size/*!MAX_FIXED_MODE_SIZE*/(mtcsMode)){
                /* Emit large/huge _BitInt INTEGER_CSTs into memory.  */
                exp = tree_output_constant_def (exp);
                return mtcs_expr_expand_expr/*!expand_expr*/(self,exp, target, VOIDmode, modifier);
            }
        }
        /* Given that TYPE_PRECISION (type) is not always equal to
           GET_MODE_PRECISION (TYPE_MODE (type)), we need to extend from
           the former to the latter according to the signedness of the
           type.  */
       // scalarIntMode=mtcs_mode_host2device_scalar_int/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
        n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 22aa 整形常数 变量处理 exp:%p %d %d\n",exp,mode,TYPE_MODE(type));

        scalarIntMode=mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
        n_debug("mtcsexpr.c  mtcs_expr_expand_expr_real_1 22 整形常数 变量处理 exp:%p scalarIntMode:%d\n",exp,scalarIntMode);
        temp = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,
                wi::to_wide (exp, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,
                      scalarIntMode)), scalarIntMode);
        return temp;
      }

    case VECTOR_CST:
      {
        tree tmp = NULL_TREE;
        if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode))
          return const_vector_from_tree(self,exp);
        scalar_int_mode int_mode;
        if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode)){
            tree type_for_mode = lang_hooks.types.type_for_mode (int_mode, 1);
            if (type_for_mode)
              tmp = mtcs_const_fold_unary_loc/*!fold_unary_loc*/(mtcsConst,loc, VIEW_CONVERT_EXPR,type_for_mode, exp);
        }
        if (!tmp){
            vec<constructor_elt, va_gc> *v;
            /* Constructors need to be fixed-length.  FIXME.  */
            unsigned int nunits = VECTOR_CST_NELTS (exp).to_constant ();
            vec_alloc (v, nunits);
            for (unsigned int i = 0; i < nunits; ++i)
              CONSTRUCTOR_APPEND_ELT (v, NULL_TREE, VECTOR_CST_ELT (exp, i));
            tmp = build_constructor (type, v);
        }
        return mtcs_expr_expand_expr/*!expand_expr*/(self,tmp, ignore ? const0_rtx : target, tmode, modifier);
      }

    case CONST_DECL:
      if (modifier == EXPAND_WRITE){
          /* Writing into CONST_DECL is always invalid, but handle it
             gracefully.  */
          addr_space_t as = TYPE_ADDR_SPACE (TREE_TYPE (exp));
          scalar_int_mode address_mode =target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
          op0 = expand_expr_addr_expr_1(self,exp, NULL_RTX, address_mode,
                         EXPAND_NORMAL, as);
          op0 = mtcs_explow_memory_address_addr_space/*!memory_address_addr_space*/(mtcsExplow,mode, op0, as);
          temp = gen_rtx_MEM (mode, op0);
          mtcs_rtl_set_mem_addr_space/*!set_mem_addr_space*/(mtcsRTL,temp, as);
          return temp;
      }
      return mtcs_expr_expand_expr/*!expand_expr*/(self,DECL_INITIAL (exp), target, VOIDmode, modifier);

    case REAL_CST:
      /* If optimized, generate immediate CONST_DOUBLE
     which will be turned into memory by reload if necessary.

     We used to force a register so that loop.c could see it.  But
     this does not allow gen_* patterns to perform optimizations with
     the constants.  It also produces two insns in cases like "x = 1.0;".
     On most machines, floating-point constants are not permitted in
     many insns, so we'd end up copying it to a register in any case.

     Now, we do the copying in expand_binop, if appropriate.  */
      return mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(mtcsRTL,TREE_REAL_CST (exp),TYPE_MODE (TREE_TYPE (exp)));

    case FIXED_CST:
      return mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(mtcsRTL,TREE_FIXED_CST (exp),TYPE_MODE (TREE_TYPE (exp)));

    case COMPLEX_CST:
      /* Handle evaluating a complex constant in a CONCAT target.  */
      if (original_target && GET_CODE (original_target) == CONCAT){
          rtx rtarg, itarg;

          mode = TYPE_MODE (TREE_TYPE (TREE_TYPE (exp)));
          rtarg = XEXP (original_target, 0);
          itarg = XEXP (original_target, 1);

          /* Move the real and imaginary parts separately.  */
          op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,TREE_REALPART (exp), rtarg, mode, EXPAND_NORMAL);
          op1 = mtcs_expr_expand_expr/*!expand_expr*/(self,TREE_IMAGPART (exp), itarg, mode, EXPAND_NORMAL);

          if (op0 != rtarg)
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,rtarg, op0);
          if (op1 != itarg)
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,itarg, op1);

          return original_target;
      }

      /* fall through */

    case STRING_CST:
      temp = expand_expr_constant(self,exp, 1, modifier);

      /* temp contains a constant address.
     On RISC machines where a constant address isn't valid,
     make some insns to get that address into a register.  */
      if (modifier != EXPAND_CONST_ADDRESS
          && modifier != EXPAND_INITIALIZER
          && modifier != EXPAND_SUM
          && ! mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,
                  mode, XEXP (temp, 0), mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,temp)))
              return mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,temp,copy_rtx (XEXP (temp, 0)));
      return temp;

    case POLY_INT_CST:
      return mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,poly_int_cst_value (exp), mode);

    case SAVE_EXPR:
      {
        tree val = treeop0;
        rtx ret =mtcs_expr_expand_expr_real_1/*!expand_expr_real_1*/(self,val, target, tmode, modifier, alt_rtl,inner_reference_p);
        if (!SAVE_EXPR_RESOLVED_P (exp)){
            /* We can indeed still hit this case, typically via builtin
               expanders calling save_expr immediately before expanding
               something.  Assume this means that we only have to deal
               with non-BLKmode values.  */
            gcc_assert (GET_MODE (ret) != mtcsMode->modes.M_BLKmode);
            val = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,curr_insn_location (),VAR_DECL, NULL, TREE_TYPE (exp));
            DECL_ARTIFICIAL (val) = 1;
            DECL_IGNORED_P (val) = 1;
            treeop0 = val;
            TREE_OPERAND (exp, 0) = treeop0;
            SAVE_EXPR_RESOLVED_P (exp) = 1;
            if (!CONSTANT_P (ret))
              ret = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,ret);
            mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,val, ret);
        }
        return ret;
      }
    case CONSTRUCTOR:
      /* If we don't need the result, just ensure we evaluate any
     subexpressions.  */
      if (ignore){
          unsigned HOST_WIDE_INT idx;
          tree value;
          FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (exp), idx, value)
            mtcs_expr_expand_expr/*!expand_expr*/(self,value, const0_rtx, VOIDmode, EXPAND_NORMAL);
          return const0_rtx;
      }
      return expand_constructor(self,exp, target, modifier, false);

    case TARGET_MEM_REF:
      {
        addr_space_t as= TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0))));
        unsigned int align;

        op0 = mtcs_ssa_address_addr_for_mem_ref/*!addr_for_mem_ref*/(mtcsSsaAddress,exp, as, true);
        op0 = mtcs_explow_memory_address_addr_space/*!memory_address_addr_space*/(mtcsExplow,mode, op0, as);
        temp = gen_rtx_MEM (mode, op0);
        mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,temp, exp, 0);
        mtcs_rtl_set_mem_addr_space/*!set_mem_addr_space*/(mtcsRTL,temp, as);
        align = mtcs_builtins_get_object_alignment/*!get_object_alignment*/(mtcsBuiltins,exp);
        if (modifier != EXPAND_WRITE
            && modifier != EXPAND_MEMORY
            && mode != mtcsMode->modes.M_BLKmode
            && align < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
            temp = expand_misaligned_mem_ref(self,temp, mode, unsignedp,align, NULL_RTX, NULL);
        return EXTEND_BITINT (temp);
      }

    case MEM_REF:
      {
        const bool reverse = REF_REVERSE_STORAGE_ORDER (exp);
        addr_space_t as = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0))));
        machine_mode address_mode;
        tree base = TREE_OPERAND (exp, 0);
        gimple *def_stmt;
        unsigned align;
        /* Handle expansion of non-aliased memory with non-BLKmode.  That
           might end up in a register.  */
        if (mtcs_expr_mem_ref_refers_to_non_mem_p/*!mem_ref_refers_to_non_mem_p*/(self,exp)){
            poly_int64 offset = mem_ref_offset (exp).force_shwi ();
            base = TREE_OPERAND (base, 0);
            poly_uint64 type_size;
            if (known_eq (offset, 0)  && !reverse  && poly_int_tree_p (TYPE_SIZE (type), &type_size)
                    && known_eq (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,DECL_MODE (base)), type_size))
              return mtcs_expr_expand_expr/*!expand_expr*/(self,build1 (VIEW_CONVERT_EXPR, type, base),target, tmode, modifier);

            typeMode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
            declMode=mtcs_mode_host2device_by_tree(mtcsMode,base,DECL_MODE (base));
            if (typeMode/*!TYPE_MODE (type)*/==mtcsMode->modes.M_BLKmode){
                temp = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,
                      declMode/*!DECL_MODE (base)*/,
                      mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,declMode/*!DECL_MODE (base)*/));
                mtcs_expr_store_expr/*!store_expr*/(self,base, temp, 0, false, false);
                temp = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,temp, mtcsMode->modes.M_BLKmode, offset);
                mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,temp, int_size_in_bytes (type));
                return temp;
             }
            exp = build3 (BIT_FIELD_REF, type, base, TYPE_SIZE (type),bitsize_int (offset * BITS_PER_UNIT));
            REF_REVERSE_STORAGE_ORDER (exp) = reverse;
            return mtcs_expr_expand_expr/*!expand_expr*/(self,exp, target, tmode, modifier);
        }
        address_mode = target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
        if ((def_stmt = get_def_for_expr (base, BIT_AND_EXPR))){
            tree mask = gimple_assign_rhs2 (def_stmt);
            base = build2 (BIT_AND_EXPR, TREE_TYPE (base),gimple_assign_rhs1 (def_stmt), mask);
            TREE_OPERAND (exp, 0) = base;
        }
        align = mtcs_builtins_get_object_alignment/*!get_object_alignment*/(mtcsBuiltins,exp);
        op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,base, NULL_RTX, VOIDmode, EXPAND_SUM);
        op0 = mtcs_explow_memory_address_addr_space/*!memory_address_addr_space*/(mtcsExplow,mode, op0, as);
        if (!integer_zerop (TREE_OPERAND (exp, 1))){
            rtx off = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,mem_ref_offset (exp), address_mode);
            op0 = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, address_mode, op0, off);
            op0 = mtcs_explow_memory_address_addr_space/*!memory_address_addr_space*/(mtcsExplow,mode, op0, as);
        }
        temp = gen_rtx_MEM (mode, op0);
        mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,temp, exp, 0);
        mtcs_rtl_set_mem_addr_space/*!set_mem_addr_space*/(mtcsRTL,temp, as);
        if (TREE_THIS_VOLATILE (exp))
          MEM_VOLATILE_P (temp) = 1;
        if (modifier == EXPAND_WRITE || modifier == EXPAND_MEMORY)
          return temp;
        if (!inner_reference_p
            && mode != mtcsMode->modes.M_BLKmode
            && align < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
          temp = expand_misaligned_mem_ref(self,temp, mode, unsignedp, align,
                            modifier == EXPAND_STACK_PARM? NULL_RTX : target, alt_rtl);
        if (reverse)
          temp = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,mode, temp);
        return EXTEND_BITINT (temp);
      }

    case ARRAY_REF:
      {
        tree array = treeop0;
        tree index = treeop1;
        tree init;
        /* Fold an expression like: "foo"[2].
           This is not done in fold so it won't happen inside &.
           Don't fold if this is for wide characters since it's too
           difficult to do correctly and this is a very rare case.  */
        if (modifier != EXPAND_CONST_ADDRESS  && modifier != EXPAND_INITIALIZER  && modifier != EXPAND_MEMORY){
            tree t = fold_read_from_constant_string (exp);
            if (t)
              return mtcs_expr_expand_expr/*!expand_expr*/(self,t, target, tmode, modifier);
        }

        /* If this is a constant index into a constant array,
           just get the value from the array.  Handle both the cases when
           we have an explicit constructor and when our operand is a variable
           that was declared const.  */

        if (modifier != EXPAND_CONST_ADDRESS
            && modifier != EXPAND_INITIALIZER
            && modifier != EXPAND_MEMORY
            && TREE_CODE (array) == CONSTRUCTOR
            && ! TREE_SIDE_EFFECTS (array)
            && TREE_CODE (index) == INTEGER_CST){
            unsigned HOST_WIDE_INT ix;
            tree field, value;
            FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (array), ix,field, value)
              if (tree_int_cst_equal (field, index)){
                  if (!TREE_SIDE_EFFECTS (value))
                    return mtcs_expr_expand_expr/*!expand_expr*/(self,fold (value), target, tmode, modifier);
                  break;
              }
        }else if (optimize >= 1
             && modifier != EXPAND_CONST_ADDRESS
             && modifier != EXPAND_INITIALIZER
             && modifier != EXPAND_MEMORY
             && TREE_READONLY (array) && ! TREE_SIDE_EFFECTS (array)
             && TREE_CODE (index) == INTEGER_CST
             && (VAR_P (array) || TREE_CODE (array) == CONST_DECL)
             && (init = ctor_for_folding (array)) != error_mark_node){
            if (init == NULL_TREE){
                tree value = build_zero_cst (type);
                if (TREE_CODE (value) == CONSTRUCTOR){
                    /* If VALUE is a CONSTRUCTOR, this optimization is only
                       useful if this doesn't store the CONSTRUCTOR into
                       memory.  If it does, it is more efficient to just
                       load the data from the array directly.  */
                    rtx ret = expand_constructor(self,value, target,modifier, true);
                    if (ret == NULL_RTX)
                      value = NULL_TREE;
                }
                if (value)
                    return mtcs_expr_expand_expr/*!expand_expr*/(self,value, target, tmode, modifier);
                }else if (TREE_CODE (init) == CONSTRUCTOR){
                    unsigned HOST_WIDE_INT ix;
                    tree field, value;
                    FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (init), ix,field, value)
                      if (tree_int_cst_equal (field, index)){
                          if (TREE_SIDE_EFFECTS (value))
                              break;
                          if (TREE_CODE (value) == CONSTRUCTOR){
                              /* If VALUE is a CONSTRUCTOR, this
                                 optimization is only useful if
                                 this doesn't store the CONSTRUCTOR
                                 into memory.  If it does, it is more
                                 efficient to just load the data from
                                 the array directly.  */
                              rtx ret = expand_constructor(self,value, target,modifier, true);
                              if (ret == NULL_RTX)
                                break;
                          }
                          return  mtcs_expr_expand_expr/*!expand_expr*/(self,fold (value), target, tmode, modifier);
                      }
                }else if (TREE_CODE (init) == STRING_CST){
                    tree low_bound = mtcs_tree_array_ref_low_bound/*!array_ref_low_bound*/(mtcsTree,exp);
                    tree index1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype, treeop1);
                    /* Optimize the special case of a zero lower bound.
                       We convert the lower bound to sizetype to avoid problems
                       with constant folding.  E.g. suppose the lower bound is
                       1 and its mode is QI.  Without the conversion
                          (ARRAY + (INDEX - (unsigned char)1))
                       becomes
                          (ARRAY + (-(unsigned char)1) + INDEX)
                       which becomes
                          (ARRAY + 255 + INDEX).  Oops!  */
                    if (!integer_zerop (low_bound))
                      index1 = size_diffop_loc (loc, index1, mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype,low_bound));

                    if (tree_fits_uhwi_p (index1) && compare_tree_int (index1, TREE_STRING_LENGTH (init)) < 0){
                        tree char_type = TREE_TYPE (TREE_TYPE (init));
                        scalar_int_mode char_mode;
                        if (mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,TYPE_MODE (char_type), &char_mode)
                                && mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,char_mode) == 1)
                          return mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,TREE_STRING_POINTER (init)[TREE_INT_CST_LOW (index1)],char_mode);
                    }
                }
             }
      }
      goto normal_inner_ref;

    case COMPONENT_REF:
      gcc_assert (TREE_CODE (treeop0) != CONSTRUCTOR);
      /* Fall through.  */
    case BIT_FIELD_REF:
    case ARRAY_RANGE_REF:
    normal_inner_ref:
      {
        machine_mode mode1, mode2;
        poly_int64 bitsize, bitpos, bytepos;
        tree offset;
        int reversep, volatilep = 0;
        tree tem = mtcs_expr_get_inner_reference/*!get_inner_reference*/(self,
              exp, &bitsize, &bitpos, &offset, &mode1, &unsignedp, &reversep, &volatilep);
        rtx orig_op0, memloc;
        bool clear_mem_expr = false;
        bool must_force_mem;
        /* If we got back the original object, something is wrong.  Perhaps
           we are evaluating an expression too early.  In any event, don't
           infinitely recurse.  */
        gcc_assert (tem != exp);
        /* Make sure bitpos is not negative, this can wreak havoc later.  */
        if (maybe_lt (bitpos, 0)){
            gcc_checking_assert (offset == NULL_TREE);
            offset = size_int (bits_to_bytes_round_down (bitpos));
            bitpos = num_trailing_bits (bitpos);
        }

        /* If we have either an offset, a BLKmode result, or a reference
           outside the underlying object, we must force it to memory.
           Such a case can occur in Ada if we have unchecked conversion
           of an expression from a scalar type to an aggregate type or
           for an ARRAY_RANGE_REF whose type is BLKmode, or if we were
           passed a partially uninitialized object or a view-conversion
           to a larger size.  */
        must_force_mem = offset != NULL_TREE
                 || mode1 == mtcsMode->modes.M_BLKmode
                 || (mode == mtcsMode->modes.M_BLKmode
                     && !mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,bitsize, 1).exists ());

        const enum expand_modifier tem_modifier = must_force_mem
            ? EXPAND_MEMORY : modifier == EXPAND_SUM ? EXPAND_NORMAL : modifier;

        /* If TEM's type is a union of variable size, pass TARGET to the inner
           computation, since it will need a temporary and TARGET is known
           to have to do.  This occurs in unchecked conversion in Ada.  */
        const rtx tem_target = TREE_CODE (TREE_TYPE (tem)) == UNION_TYPE
            && COMPLETE_TYPE_P (TREE_TYPE (tem))
            && TREE_CODE (TYPE_SIZE (TREE_TYPE (tem))) != INTEGER_CST
            && modifier != EXPAND_STACK_PARM ? target : NULL_RTX;

        orig_op0 = op0 =  mtcs_expr_expand_expr_real/*!expand_expr_real*/(self,tem, tem_target, VOIDmode, tem_modifier, NULL,true);

        /* If the field has a mode, we want to access it in the
           field's mode, not the computed mode.
           If a MEM has VOIDmode (external with incomplete type),
           use BLKmode for it instead.  */
        if (MEM_P (op0)){
            if (mode1 != VOIDmode)
              op0 = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mode1, 0);
            else if (GET_MODE (op0) == VOIDmode)
              op0 = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mtcsMode->modes.M_BLKmode, 0);
        }

        mode2 = CONSTANT_P (op0) ? TYPE_MODE (TREE_TYPE (tem)) : GET_MODE (op0);

        /* See above for the rationale.  */
        if (maybe_gt (bitpos + bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode2)))
          must_force_mem = true;

        /* Handle CONCAT first.  */
        if (GET_CODE (op0) == CONCAT && !must_force_mem){
            if (known_eq (bitpos, 0)
              && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (op0)))
              && mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,mode1)
              && mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,GET_MODE (op0))
              && (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode1))
                == mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (op0))))){
                if (reversep)
                  op0 = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,GET_MODE (op0), op0);
                if (mode1 != GET_MODE (op0)){
                    rtx parts[2];
                    for (int i = 0; i < 2; i++){
                        rtx op = read_complex_part (op0, i != 0);
                        if (GET_CODE (op) == SUBREG)
                          op = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (op), op);
                        temp = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(mtcsRTL,
                              mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode1), op);
                        if (temp)
                          op = temp;
                        else{
                            if (!REG_P (op) && !MEM_P (op))
                              op = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (op), op);
                            op = gen_lowpart (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode1), op);
                        }
                        parts[i] = op;
                    }
                    op0 = gen_rtx_CONCAT (mode1, parts[0], parts[1]);
                }
                return op0;
            }
            if (known_eq (bitpos, 0) && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (XEXP (op0, 0))))
                  && maybe_ne (bitsize, 0)){
                op0 = XEXP (op0, 0);
                mode2 = GET_MODE (op0);
            }else if (known_eq (bitpos,mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (XEXP (op0, 0))))
                 && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (XEXP (op0, 1))))
                 && maybe_ne (bitpos, 0) && maybe_ne (bitsize, 0)){
                op0 = XEXP (op0, 1);
                bitpos = 0;
                mode2 = GET_MODE (op0);
            }else
              /* Otherwise force into memory.  */
              must_force_mem = true;
        }

        /* If this is a constant, put it in a register if it is a legitimate
           constant and we don't need a memory reference.  */
        if (CONSTANT_P (op0) && mode2 !=mtcsMode->modes.M_BLKmode
            && mtcsTarget/*!targetm.legitimate_constant_p*/->legitimate_constant_p(mtcsTarget,mode2, op0)
            && !must_force_mem)
            op0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode2, op0);
        /* Otherwise, if this is a constant, try to force it to the constant
           pool.  Note that back-ends, e.g. MIPS, may refuse to do so if it
           is a legitimate constant.  */
        else if (CONSTANT_P (op0) && (memloc = mtcs_asm_force_const_mem/*!force_const_mem*/(mtcsAsm,mode2, op0)))
          op0 = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,memloc);

        /* Otherwise, if this is a constant or the object is not in memory
           and need be, put it there.  */
        else if (CONSTANT_P (op0) || (!MEM_P (op0) && must_force_mem)){
            memloc = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,TREE_TYPE (tem), 1, 1);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,memloc, op0);
            op0 = memloc;
            clear_mem_expr = true;
        }

        if (offset) {
            machine_mode address_mode;
            rtx offset_rtx = mtcs_expr_expand_expr/*!expand_expr*/(self,offset, NULL_RTX, VOIDmode,EXPAND_SUM);
            gcc_assert (MEM_P (op0));
            address_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,op0);
            if (GET_MODE (offset_rtx) != address_mode){
                /* We cannot be sure that the RTL in offset_rtx is valid outside
                   of a memory address context, so force it into a register
                   before attempting to convert it to the desired mode.  */
                offset_rtx = mtcs_expr_force_operand/*!force_operand*/(self,offset_rtx, NULL_RTX);
                offset_rtx = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,address_mode, offset_rtx, 0);
            }

            /* See the comment in expand_assignment for the rationale.  */
            if (mode1 != VOIDmode
                && maybe_ne (bitpos, 0)
                && maybe_gt (bitsize, 0)
                && multiple_p (bitpos, BITS_PER_UNIT, &bytepos)
                && multiple_p (bitpos, bitsize)
                && multiple_p (bitsize, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode1))
                && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode1)){
                op0 =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mode1, bytepos);
                bitpos = 0;
            }

            op0 =mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,op0, offset_rtx, highest_pow2_factor (offset));
        }
        /* If OFFSET is making OP0 more aligned than BIGGEST_ALIGNMENT,
           record its alignment as BIGGEST_ALIGNMENT.  */
        if (MEM_P (op0)  && known_eq (bitpos, 0)  && offset != 0  && is_aligning_offset(self,offset, tem))
          mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,op0, mtcs_align_get_biggest_alignment(mtcsAlign)/*BIGGEST_ALIGNMENT*/);
        /* Don't forget about volatility even if this is a bitfield.  */
        if (MEM_P (op0) && volatilep && ! MEM_VOLATILE_P (op0)){
            if (op0 == orig_op0)
              op0 = copy_rtx (op0);
            MEM_VOLATILE_P (op0) = 1;
        }
        if (MEM_P (op0) && TREE_CODE (tem) == FUNCTION_DECL){
            if (op0 == orig_op0)
              op0 = copy_rtx (op0);
            mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,op0, BITS_PER_UNIT);
        }
        /* In cases where an aligned union has an unaligned object
           as a field, we might be extracting a BLKmode value from
           an integer-mode (e.g., SImode) object.  Handle this case
           by doing the extract into an object as wide as the field
           (which we know to be the width of a basic mode), then
           storing into memory, and changing the mode to BLKmode.  */
        if (mode1 == VOIDmode
            || REG_P (op0) || GET_CODE (op0) == SUBREG
            || (mode1 != mtcsMode->modes.M_BLKmode && !mtcsReg->hardRegs.x_direct_load/*!direct_load*/[(int) mode1]
            && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) != MODE_COMPLEX_INT
            && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) != MODE_COMPLEX_FLOAT
            && modifier != EXPAND_CONST_ADDRESS
            && modifier != EXPAND_INITIALIZER
            && modifier != EXPAND_MEMORY)
            /* If the bitfield is volatile and the bitsize
               is narrower than the access size of the bitfield,
               we need to extract bitfields from the access.  */
            || (volatilep && TREE_CODE (exp) == COMPONENT_REF
            && DECL_BIT_FIELD_TYPE (TREE_OPERAND (exp, 1))
            && mode1 != mtcsMode->modes.M_BLKmode
            && maybe_lt (bitsize, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode1) * BITS_PER_UNIT))
            /* If the field isn't aligned enough to fetch as a memref,
               fetch it as a bit field.  */
            || (mode1 != mtcsMode->modes.M_BLKmode
            && (((MEM_P (op0)
                  ? mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode1)
                || !multiple_p (bitpos, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode1))
                  : TYPE_ALIGN (TREE_TYPE (tem)) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)
                || !multiple_p (bitpos, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)))
                 && modifier != EXPAND_MEMORY
                 && ((modifier == EXPAND_CONST_ADDRESS
                  || modifier == EXPAND_INITIALIZER)
                 ? mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
                 : mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode1,
                                  mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0))))
                || !multiple_p (bitpos, BITS_PER_UNIT)))
            /* If the type and the field are a constant size and the
               size of the type isn't the same size as the bitfield,
               we must use bitfield operations.  */
            || (known_size_p (bitsize)
            && TYPE_SIZE (TREE_TYPE (exp))
            && poly_int_tree_p (TYPE_SIZE (TREE_TYPE (exp)))
            && maybe_ne (wi::to_poly_offset (TYPE_SIZE (TREE_TYPE (exp))),bitsize))){

                machine_mode ext_mode = mode;
                if (ext_mode == mtcsMode->modes.M_BLKmode  && ! (target != 0 && MEM_P (op0)
                      && MEM_P (target)  && multiple_p (bitpos, BITS_PER_UNIT)))
                  ext_mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,bitsize, 1).else_blk ();

                if (ext_mode == mtcsMode->modes.M_BLKmode){
                    if (target == 0)
                      target = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 1, 1);

                    /* ??? Unlike the similar test a few lines below, this one is
                       very likely obsolete.  */
                    if (known_eq (bitsize, 0))
                      return target;

                    /* In this case, BITPOS must start at a byte boundary and
                       TARGET, if specified, must be a MEM.  */
                    gcc_assert (MEM_P (op0) && (!target || MEM_P (target)));

                    bytepos = exact_div (bitpos, BITS_PER_UNIT);
                    poly_int64 bytesize = bits_to_bytes_round_up (bitsize);
                    mtcs_expr_emit_block_move/*!emit_block_move*/(self,target,
                              mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, VOIDmode, bytepos),
                             mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,bytesize, pMode),(modifier == EXPAND_STACK_PARM
                              ? BLOCK_OP_CALL_PARM : BLOCK_OP_NORMAL));

                    return target;
                }

                /* If we have nothing to extract, the result will be 0 for targets
                   with SHIFT_COUNT_TRUNCATED == 0 and garbage otherwise.  Always
                   return 0 for the sake of consistency, as reading a zero-sized
                   bitfield is valid in Ada and the value is fully specified.  */
                if (known_eq (bitsize, 0))
                  return const0_rtx;

                op0 = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,op0);

                if (MEM_P (op0) && REG_P (XEXP (op0, 0)))
                    mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,XEXP (op0, 0), mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0));

                /* If the result has aggregate type and the extraction is done in
                   an integral mode, then the field may be not aligned on a byte
                   boundary; in this case, if it has reverse storage order, it
                   needs to be extracted as a scalar field with reverse storage
                   order and put back into memory order afterwards.  */
                if (AGGREGATE_TYPE_P (type)  && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,ext_mode) == MODE_INT)
                  reversep = TYPE_REVERSE_STORAGE_ORDER (type);

                gcc_checking_assert (known_ge (bitpos, 0));
                op0 = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,op0, bitsize, bitpos, unsignedp,
                             (modifier == EXPAND_STACK_PARM? NULL_RTX : target), ext_mode, ext_mode, reversep, alt_rtl);

                /* If the result has aggregate type and the mode of OP0 is an
                   integral mode then, if BITSIZE is narrower than this mode
                   and this is for big-endian data, we must put the field
                   into the high-order bits.  And we must also put it back
                   into memory order if it has been previously reversed.  */
                scalar_int_mode op0_mode;
                if (AGGREGATE_TYPE_P (type) && mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,GET_MODE (op0), &op0_mode)){
                    HOST_WIDE_INT size = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,op0_mode);

                    gcc_checking_assert (known_le (bitsize, size));
                    if (maybe_lt (bitsize, size) && reversep ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN)
                      op0 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, op0_mode, op0,size - bitsize, op0, 1);

                    if (reversep)
                      op0 = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,op0_mode, op0);
                }

                /* If the result type is BLKmode, store the data into a temporary
                   of the appropriate type, but with the mode corresponding to the
                   mode for the data we have (op0's mode).  */
                if (mode == mtcsMode->modes.M_BLKmode){
                    rtx new_rtx= mtcs_func_assign_stack_temp_for_type/*!assign_stack_temp_for_type*/(mtcsFunc,ext_mode,
                                    mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,ext_mode),type);
                    mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,new_rtx, op0);
                    op0 = copy_rtx (new_rtx);
                    mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,op0, mtcsMode->modes.M_BLKmode);
                }

               return op0;
        }

        /* If the result is BLKmode, use that to access the object
           now as well.  */
        if (mode == mtcsMode->modes.M_BLKmode)
          mode1 = mtcsMode->modes.M_BLKmode;

        /* Get a reference to just this component.  */
        bytepos = bits_to_bytes_round_down (bitpos);
        if (modifier == EXPAND_CONST_ADDRESS || modifier == EXPAND_SUM || modifier == EXPAND_INITIALIZER)
          op0 = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, mode1, bytepos);
        else
          op0 =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mode1, bytepos);

        if (op0 == orig_op0)
          op0 = copy_rtx (op0);

        /* Don't set memory attributes if the base expression is
           SSA_NAME that got expanded as a MEM or a CONSTANT.  In that case,
           we should just honor its original memory attributes.  */
        if (!(TREE_CODE (tem) == SSA_NAME  && (MEM_P (orig_op0) || CONSTANT_P (orig_op0))))
          mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,op0, exp, 0);

        if (REG_P (XEXP (op0, 0)))
            mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,XEXP (op0, 0), mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0));

        /* If op0 is a temporary because the original expressions was forced
           to memory, clear MEM_EXPR so that the original expression cannot
           be marked as addressable through MEM_EXPR of the temporary.  */
        if (clear_mem_expr)
            mtcs_rtl_set_mem_expr/*!set_mem_expr*/(mtcsRTL,op0, NULL_TREE);

        MEM_VOLATILE_P (op0) |= volatilep;

        if (reversep    && modifier != EXPAND_MEMORY  && modifier != EXPAND_WRITE)
           op0 = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,mode1, op0);

        op0 = EXTEND_BITINT (op0);

        if (mode == mode1 || mode1 == mtcsMode->modes.M_BLKmode || mode1 == tmode
            || modifier == EXPAND_CONST_ADDRESS  || modifier == EXPAND_INITIALIZER)
          return op0;

        if (target == 0)
          target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,tmode != VOIDmode ? tmode : mode);

        mtcs_expr_convert_move/*!convert_move*/(self,target, op0, unsignedp);
        return target;
      }

    case OBJ_TYPE_REF:
      return mtcs_expr_expand_expr/*!expand_expr*/(self,OBJ_TYPE_REF_EXPR (exp), target, tmode, modifier);

    case CALL_EXPR:
      /* All valid uses of __builtin_va_arg_pack () are removed during
     inlining.  */
      if (CALL_EXPR_VA_ARG_PACK (exp))
          error ("invalid use of %<__builtin_va_arg_pack ()%>");
      {
        tree fndecl = get_callee_fndecl (exp), attr;
        n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 22 CALL_EXPR fndecl:%p %s\n",
              fndecl,fndecl?IDENTIFIER_POINTER(DECL_NAME(fndecl)):"NULL");
        if (fndecl
            /* Don't diagnose the error attribute in thunks, those are
               artificially created.  */
            && !CALL_FROM_THUNK_P (exp)   && (attr = lookup_attribute ("error",DECL_ATTRIBUTES (fndecl))) != NULL){
            const char *ident = lang_hooks.decl_printable_name (fndecl, 1);
            error ("call to %qs declared with attribute error: %s",
               identifier_to_locale (ident),TREE_STRING_POINTER (TREE_VALUE (TREE_VALUE (attr))));
        }
        if (fndecl
            /* Don't diagnose the warning attribute in thunks, those are
               artificially created.  */
            && !CALL_FROM_THUNK_P (exp)
            && (attr = lookup_attribute ("warning",DECL_ATTRIBUTES (fndecl))) != NULL){
            const char *ident = lang_hooks.decl_printable_name (fndecl, 1);
            warning_at (EXPR_LOCATION (exp),
                OPT_Wattribute_warning,"call to %qs declared with attribute warning: %s",
                identifier_to_locale (ident),TREE_STRING_POINTER (TREE_VALUE (TREE_VALUE (attr))));
        }

        /* Check for a built-in function.  */
        if (fndecl && fndecl_built_in_p (fndecl)){
            gcc_assert (DECL_BUILT_IN_CLASS (fndecl) != BUILT_IN_FRONTEND);
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 33 内建函数处理 %p code:%s\n",fndecl,get_tree_code_name(code));
            return mtcs_builtins_expand_builtin/*!expand_builtin*/(mtcsBuiltins,exp, target, subtarget, tmode, ignore);
        }
        if(fndecl && mtcs_builtins_is_builtin_fn(fndecl)){
           n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 44 这是一个内建的mtcs函数------%s ignore:%d tmode:%d target:%p\n",
                 IDENTIFIER_POINTER(DECL_NAME(fndecl)),ignore,tmode,target);
           return mtcs_builtins_expand_mtcs_builtin(mtcsBuiltins,exp, target, subtarget, tmode, ignore);
        }
        if(fndecl && mtcs_builtins_is_internal_fn(fndecl)){
                n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 55 这是一个内部的mtcs函数------%s ignore:%d tmode:%d target:%p\n",
                      IDENTIFIER_POINTER(DECL_NAME(fndecl)),ignore,tmode,target);
           return mtcs_builtins_expand_mtcs_internal(mtcsBuiltins,exp, target, subtarget, tmode, ignore);
        }
      }
      temp = mtcs_calls_expand_call(mtcsCalls,exp, target, ignore);
      return EXTEND_BITINT (temp);

    case VIEW_CONVERT_EXPR:
      op0 = NULL_RTX;

      /* If we are converting to BLKmode, try to avoid an intermediate
     temporary by fetching an inner memory reference.  */
      if (mode == mtcsMode->modes.M_BLKmode
          && poly_int_tree_p (TYPE_SIZE (type))
          && TYPE_MODE (TREE_TYPE (treeop0)) != mtcsMode->modes.M_BLKmode
          && handled_component_p (treeop0))
      {
        machine_mode mode1;
        poly_int64 bitsize, bitpos, bytepos;
        tree offset;
        int reversep, volatilep = 0;
        tree tem = mtcs_expr_get_inner_reference/*!get_inner_reference*/(self,
              treeop0, &bitsize, &bitpos, &offset, &mode1,&unsignedp, &reversep, &volatilep);

        /* ??? We should work harder and deal with non-zero offsets.  */
        if (!offset
            && multiple_p (bitpos, BITS_PER_UNIT, &bytepos)
            && !reversep
            && known_size_p (bitsize)
            && known_eq (wi::to_poly_offset (TYPE_SIZE (type)), bitsize))
        {
            /* See the normal_inner_ref case for the rationale.  */
            rtx orig_op0 =  mtcs_expr_expand_expr_real/*!expand_expr_real*/(self,tem,
                      (TREE_CODE (TREE_TYPE (tem)) == UNION_TYPE
                       && (TREE_CODE (TYPE_SIZE (TREE_TYPE (tem)))!= INTEGER_CST)
                       && modifier != EXPAND_STACK_PARM? target : NULL_RTX),
                      VOIDmode,modifier == EXPAND_SUM ? EXPAND_NORMAL : modifier,
                      NULL, true);

            if (MEM_P (orig_op0)){
                op0 = orig_op0;
                /* Get a reference to just this component.  */
                if (modifier == EXPAND_CONST_ADDRESS || modifier == EXPAND_SUM || modifier == EXPAND_INITIALIZER)
                  op0 = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,op0, mode, bytepos);
                else
                  op0 =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mode, bytepos);
                if (op0 == orig_op0)
                  op0 = copy_rtx (op0);
                mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,op0, treeop0, 0);
                if (REG_P (XEXP (op0, 0)))
                    mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,XEXP (op0, 0), mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0));
                MEM_VOLATILE_P (op0) |= volatilep;
            }
        }
      }

      if (!op0)
        op0 =  mtcs_expr_expand_expr_real/*!expand_expr_real*/(self,treeop0, NULL_RTX, VOIDmode, modifier,
                    NULL, inner_reference_p);
      /* If the input and output modes are both the same, we are done.  */
      if (mode == GET_MODE (op0))
        ;
          /* Similarly if the output mode is BLKmode and input is a MEM,
         adjust_address done below is all we need.  */
      else if (mode == mtcsMode->modes.M_BLKmode && MEM_P (op0))
        ;
          /* If neither mode is BLKmode, and both modes are the same size
         then we can use gen_lowpart.  */
      else if (mode != mtcsMode->modes.M_BLKmode
           && GET_MODE (op0) != mtcsMode->modes.M_BLKmode
           && known_eq (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode),
                mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,GET_MODE (op0)))
           && !mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,GET_MODE (op0)))
      {
          if (GET_CODE (op0) == SUBREG)
            op0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (op0), op0);
          temp = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(mtcsRTL,mode, op0);
          if (temp)
            op0 = temp;
          else{
              if (!REG_P (op0) && !MEM_P (op0))
                  op0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (op0), op0);
              op0 = gen_lowpart (mode, op0);
          }
      }
      /* If both types are integral, convert from one mode to the other.  */
      else if (INTEGRAL_TYPE_P (type)
           && INTEGRAL_TYPE_P (TREE_TYPE (treeop0))
           && mode != mtcsMode->modes.M_BLKmode
           && GET_MODE (op0) != mtcsMode->modes.M_BLKmode)
          op0 = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, GET_MODE (op0), op0,TYPE_UNSIGNED (TREE_TYPE (treeop0)));
      /* If the output type is a bit-field type, do an extraction.  */
      else if (reduce_bit_field)
          return mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,op0, TYPE_PRECISION (type), 0,
                  TYPE_UNSIGNED (type), NULL_RTX, mode, mode, false, NULL);
      /* As a last resort, spill op0 to memory, and reload it in a
     different mode.  */
      else if (!MEM_P (op0)){
          /* If the operand is not a MEM, force it into memory.  Since we
             are going to be changing the mode of the MEM, don't call
             force_const_mem for constants because we don't allow pool
             constants to change mode.  */
          tree inner_type = TREE_TYPE (treeop0);

          gcc_assert (!TREE_ADDRESSABLE (exp));
          if (target == 0 || GET_MODE (target) != TYPE_MODE (inner_type))
            target= mtcs_func_assign_stack_temp_for_type/*!assign_stack_temp_for_type*/(mtcsFunc,TYPE_MODE (inner_type),
             mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,TYPE_MODE (inner_type)), inner_type);

          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, op0);
          op0 = target;
      }

      /* If OP0 is (now) a MEM, we need to deal with alignment issues.  If the
     output type is such that the operand is known to be aligned, indicate
     that it is.  Otherwise, we need only be concerned about alignment for
     non-BLKmode results.  */
      if (MEM_P (op0)){
          enum insn_code icode;
          if (modifier != EXPAND_WRITE    && modifier != EXPAND_MEMORY  && !inner_reference_p   && mode != mtcsMode->modes.M_BLKmode
              && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL, op0) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
          {
              /* If the target does have special handling for unaligned
             loads of mode then use them.  */
              if ((icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab, mode)) != CODE_FOR_nothing){
                  rtx reg;
                  op0 =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mode, 0);
                  /* We've already validated the memory, and we're creating a
                     new pseudo destination.  The predicates really can't
                     fail.  */
                  reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
                   /* Nor can the insn generator.  */
                  rtx_insn *insn = MTCS_GEN_FCN/*!GEN_FCN*/(icode) (reg, op0);
                  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insn);
                  return reg;
              }else if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)){
                  poly_uint64 mode_size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
                  poly_uint64 temp_size = mode_size;
                  if (GET_MODE (op0) != mtcsMode->modes.M_BLKmode)
                    temp_size = upper_bound (temp_size,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (op0)));
                  rtx new_rtx = mtcs_func_assign_stack_temp_for_type/*!assign_stack_temp_for_type*/(mtcsFunc,mode, temp_size, type);
                  rtx new_with_op0_mode=  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,new_rtx, GET_MODE (op0), 0);

                  gcc_assert (!TREE_ADDRESSABLE (exp));
                  if (GET_MODE (op0) == mtcsMode->modes.M_BLKmode){
                      rtx size_rtx = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,mode_size, Pmode);
                      mtcs_expr_emit_block_move/*!emit_block_move*/(self,new_with_op0_mode, op0, size_rtx,
                               (modifier == EXPAND_STACK_PARM? BLOCK_OP_CALL_PARM: BLOCK_OP_NORMAL));
                  }else
                    mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,new_with_op0_mode, op0);

                  op0 = new_rtx;
              }
          }
          op0 =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,op0, mode, 0);
      }

      return op0;

    case MODIFY_EXPR:
      {
        tree lhs = treeop0;
        tree rhs = treeop1;
        gcc_assert (ignore);

        /* Check for |= or &= of a bitfield of size one into another bitfield
           of size 1.  In this case, (unless we need the result of the
           assignment) we can do this more efficiently with a
           test followed by an assignment, if necessary.

           ??? At this point, we can't get a BIT_FIELD_REF here.  But if
           things change so we do, this code should be enhanced to
           support it.  */
        if (TREE_CODE (lhs) == COMPONENT_REF
            && (TREE_CODE (rhs) == BIT_IOR_EXPR
            || TREE_CODE (rhs) == BIT_AND_EXPR)
            && TREE_OPERAND (rhs, 0) == lhs
            && TREE_CODE (TREE_OPERAND (rhs, 1)) == COMPONENT_REF
            && integer_onep (DECL_SIZE (TREE_OPERAND (lhs, 1)))
            && integer_onep (DECL_SIZE (TREE_OPERAND (TREE_OPERAND (rhs, 1), 1)))){

                rtx_code_label *label =mtcs_rtl_gen_label_rtx(mtcsRTL);
                int value = TREE_CODE (rhs) == BIT_IOR_EXPR;
                profile_probability prob = profile_probability::uninitialized ();
                if (value)
                    mtcs_dojump_jumpifnot/*!jumpifnot*/(mtcsDojump,TREE_OPERAND (rhs, 1), label, prob);
                else
                    mtcs_dojump_jumpif/*!jumpif*/(mtcsDojump,TREE_OPERAND (rhs, 1), label, prob);
                mtcs_expr_expand_assignment/*!expand_assignment*/(self,lhs,
                      mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (rhs), value),false);
                mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
                mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
                return const0_rtx;
        }

        mtcs_expr_expand_assignment/*!expand_assignment*/(self,lhs, rhs, false);
        return const0_rtx;
      }

    case ADDR_EXPR:
       n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_1 ADDR_EXPR target:%p tmode:%d modifier:%d\n",target,tmode,modifier);
      return expand_expr_addr_expr(self,exp, target, tmode, modifier);

    case REALPART_EXPR:
      op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
      return read_complex_part (op0, false);

    case IMAGPART_EXPR:
      op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
      return read_complex_part (op0, true);

    case RETURN_EXPR:
    case LABEL_EXPR:
    case GOTO_EXPR:
    case SWITCH_EXPR:
    case ASM_EXPR:
      /* Expanded in cfgexpand.cc.  */
      gcc_unreachable ();

    case TRY_CATCH_EXPR:
    case CATCH_EXPR:
    case EH_FILTER_EXPR:
    case TRY_FINALLY_EXPR:
    case EH_ELSE_EXPR:
      /* Lowered by tree-eh.cc.  */
      gcc_unreachable ();

    case WITH_CLEANUP_EXPR:
    case CLEANUP_POINT_EXPR:
    case TARGET_EXPR:
    case CASE_LABEL_EXPR:
    case VA_ARG_EXPR:
    case BIND_EXPR:
    case INIT_EXPR:
    case CONJ_EXPR:
    case COMPOUND_EXPR:
    case PREINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case LOOP_EXPR:
    case EXIT_EXPR:
    case COMPOUND_LITERAL_EXPR:
      /* Lowered by gimplify.cc.  */
      gcc_unreachable ();

    case FDESC_EXPR:
      /* Function descriptors are not valid except for as
     initialization constants, and should not be expanded.  */
      gcc_unreachable ();

    case WITH_SIZE_EXPR:
      /* WITH_SIZE_EXPR expands to its first argument.  The caller should
     have pulled out the size to use in whatever context it needed.  */
      return  mtcs_expr_expand_expr_real/*!expand_expr_real*/(self,treeop0, original_target, tmode,
                   modifier, alt_rtl, inner_reference_p);

    default:
      return mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(self,&ops, target, tmode, modifier);
  }
}
#undef EXTEND_BITINT

/* Write to one of the components of the complex value CPLX.  Write VAL to
   the real part if IMAG_P is false, and the imaginary part if its true.
   If UNDEFINED_P then the value in CPLX is currently undefined.  */
//原型 write_complex_part expr.h expr.cc
void mtcs_expr_write_complex_part (MtcsExpr *self,rtx cplx, rtx val, bool imag_p, bool undefined_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  machine_mode cmode;
  scalar_mode imode;
  unsigned ibitsize;

  if (GET_CODE (cplx) == CONCAT){
      mtcs_expr_emit_move_insn(self,XEXP (cplx, imag_p), val);
      return;
  }

  cmode = GET_MODE (cplx);
  imode = scalar_mode::from_int((machine_mode)mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,cmode));
  ibitsize =  mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,imode);

  /* For MEMs simplify_gen_subreg may generate an invalid new address
     because, e.g., the original address is considered mode-dependent
     by the target, which restricts simplify_subreg from invoking
     adjust_address_nv.  Instead of preparing fallback support for an
     invalid address, we call adjust_address_nv directly.  */
  if (MEM_P (cplx)){
      mtcs_expr_emit_move_insn(self,mtcs_rtl_adjust_address_nv(mtcsRTL,cplx, imode,imag_p ? mtcs_mode_get_size(mtcsMode,imode) : 0),val);
      return;
  }

  /* If the sub-object is at least word sized, then we know that subregging
     will work.  This special case is important, since store_bit_field
     wants to operate on integer modes, and there's rarely an OImode to
     correspond to TCmode.  */
  if (ibitsize >= BITS_PER_WORD
      /* For hard regs we have exact predicates.  Assume we can split
     the original object if it spans an even number of hard regs.
     This special case is important for SCmode on 64-bit platforms
     where the natural size of floating-point regs is 32-bit.  */
      || (REG_P (cplx)
      && REGNO (cplx) < mtcs_reg_get_first_pseudo_register(mtcsReg)/*! FIRST_PSEUDO_REGISTER*/
      && REG_NREGS (cplx) % 2 == 0)){
      rtx part = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,imode,
              cplx, cmode,imag_p ? mtcs_mode_get_size (mtcsMode,imode) : 0);
      if (part){
          mtcs_expr_emit_move_insn(self,part, val);
          return;
      }else
        /* simplify_gen_subreg may fail for sub-word MEMs.  */
        gcc_assert (MEM_P (cplx) && ibitsize < BITS_PER_WORD);
  }

  mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,cplx, ibitsize, imag_p ? ibitsize : 0, 0, 0, imode, val,
           false, undefined_p);
}

/* Write zeros through the storage of OBJECT.  If OBJECT has BLKmode, SIZE is
   its length in bytes.  */
//原型 clear_storage_hints expr.h expr.cc
rtx mtcs_expr_clear_storage_hints (MtcsExpr *self,rtx object, rtx size, enum block_op_methods method,
             unsigned int expected_align, HOST_WIDE_INT expected_size,
             unsigned HOST_WIDE_INT min_size,
             unsigned HOST_WIDE_INT max_size,
             unsigned HOST_WIDE_INT probable_max_size,
             unsigned ctz_size)
{

    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
    MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
    MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

  machine_mode mode = GET_MODE (object);
  unsigned int align;
  gcc_assert (method == BLOCK_OP_NORMAL || method == BLOCK_OP_TAILCALL);
  /* If OBJECT is not BLKmode and SIZE is the same size as its mode,
     just move a zero.  Otherwise, do this a piece at a time.  */
  poly_int64 size_val;
  if (mode != mtcsMode->modes.M_BLKmode  && poly_int_rtx_p (size, &size_val)
        && known_eq (size_val, mtcs_mode_get_size(mtcsMode,mode))){
      rtx zero = CONST0_RTX (mode);
      if (zero != NULL){
          mtcs_expr_emit_move_insn(self,object, zero);
          return NULL;
      }

      if (mtcs_mode_is_complex_p(mtcsMode,mode)){
          zero = CONST0_RTX(mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode));
          if (zero != NULL){
              mtcs_expr_write_complex_part/*!write_complex_part*/(self,object, zero, 0, true);
              mtcs_expr_write_complex_part/*!write_complex_part*/(self,object, zero, 1, false);
              return NULL;
          }
      }
  }

  if (size == const0_rtx)
    return NULL;
  align = mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,object);
  if (CONST_INT_P (size)
      && mtcsTarget/*!targetm.use_by_pieces_infrastructure_p*/->use_by_pieces_infrastructure_p(mtcsTarget,INTVAL (size), align,
                         CLEAR_BY_PIECES,optimize_insn_for_speed_p ()))
    clear_by_pieces(self,object, INTVAL (size), align);
  else if (mtcs_expr_set_storage_via_setmem/*!set_storage_via_setmem*/(self,object, size, const0_rtx, align,
                   expected_align, expected_size,
                   min_size, max_size, probable_max_size))
    ;
  else if (try_store_by_multiple_pieces (object, size, ctz_size,
                     min_size, max_size,NULL_RTX, 0, align))
    ;
  else if (ADDR_SPACE_GENERIC_P (mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,object)))
    return set_storage_via_libcall (object, size, const0_rtx,method == BLOCK_OP_TAILCALL);
  else
    gcc_unreachable ();

  return NULL;
}

//原型 clear_storage expr.h expr.cc
rtx mtcs_expr_clear_storage(MtcsExpr *self,rtx object, rtx size, enum block_op_methods method)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  unsigned HOST_WIDE_INT max, min = 0;
  if (GET_CODE (size) == CONST_INT)
    min = max = UINTVAL (size);
  else
    max = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,GET_MODE (size));
  return clear_storage_hints (object, size, method, 0, -1, min, max, max, 0);
}


/* Helper function for store_constructor.
   TARGET, BITSIZE, BITPOS, MODE, EXP are as for store_field.
   CLEARED is as for store_constructor.
   ALIAS_SET is the alias set to use for any stores.
   If REVERSE is true, the store is to be done in reverse order.

   This provides a recursive shortcut back to store_constructor when it isn't
   necessary to go through store_field.  This is so that we can pass through
   the cleared field to let store_constructor know that we may not have to
   clear a substructure if the outer structure has already been cleared.  */
//原型 store_constructor_field expr.cc
static void store_constructor_field (MtcsExpr *self,rtx target, poly_uint64 bitsize, poly_int64 bitpos, poly_uint64 bitregion_start,
        poly_uint64 bitregion_end,  machine_mode mode, tree exp, int cleared, alias_set_type alias_set, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  poly_int64 bytepos;
  poly_uint64 bytesize;
  if (TREE_CODE (exp) == CONSTRUCTOR
      /* We can only call store_constructor recursively if the size and
     bit position are on a byte boundary.  */
      && multiple_p (bitpos, BITS_PER_UNIT, &bytepos)
      && maybe_ne (bitsize, 0U)
      && multiple_p (bitsize, BITS_PER_UNIT, &bytesize)
      /* If we have a nonzero bitpos for a register target, then we just
     let store_field do the bitfield handling.  This is unlikely to
     generate unnecessary clear instructions anyways.  */
      && (known_eq (bitpos, 0) || MEM_P (target))){
      if (MEM_P (target)){
          machine_mode target_mode = GET_MODE (target);
          if (target_mode != mtcsMode->modes.M_BLKmode
              && !multiple_p (bitpos, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,target_mode)))
            target_mode =  mtcsMode->modes.M_BLKmode;
          target = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,target, target_mode, bytepos);
      }


      /* Update the alias set, if required.  */
      if (MEM_P (target) && ! MEM_KEEP_ALIAS_SET_P (target) &&mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(mtcsRTL,target) != 0){
          target = copy_rtx (target);
          mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,target, alias_set);
      }

      mtcs_expr_store_constructor/*!store_constructor*/(self,exp, target, cleared, bytesize, reverse);
  }else
    store_field(self,target, bitsize, bitpos, bitregion_start, bitregion_end, mode,
         exp, alias_set, false, reverse);
}


/* Store the value of constructor EXP into the rtx TARGET.
   TARGET is either a REG or a MEM; we know it cannot conflict, since
   safe_from_p has been called.
   CLEARED is true if TARGET is known to have been zero'd.
   SIZE is the number of bytes of TARGET we are allowed to modify: this
   may not be the same as the size of EXP if we are assigning to a field
   which has been packed to exclude padding bits.
   If REVERSE is true, the store is to be done in reverse order.  */
//原型 store_constructor expr.h expr.cc
void mtcs_expr_store_constructor(MtcsExpr *self,tree exp, rtx target, int cleared, poly_int64 size, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
  MtcsReg *mtcsReg = mtcs_target_get_reg(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);

  tree type = TREE_TYPE (exp);
  HOST_WIDE_INT exp_size = int_size_in_bytes (type);
  poly_int64 bitregion_end = known_gt (size, 0) ? size * BITS_PER_UNIT - 1 : 0;

  switch (TREE_CODE (type)){
    case RECORD_TYPE:
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      {
        unsigned HOST_WIDE_INT idx;
        tree field, value;
        /* The storage order is specified for every aggregate type.  */
        reverse = TYPE_REVERSE_STORAGE_ORDER (type);
        /* If size is zero or the target is already cleared, do nothing.  */
        if (known_eq (size, 0) || cleared)
          cleared = 1;
        /* We either clear the aggregate or indicate the value is dead.  */
        else if ((TREE_CODE (type) == UNION_TYPE || TREE_CODE (type) == QUAL_UNION_TYPE)  && ! CONSTRUCTOR_ELTS (exp))
          /* If the constructor is empty, clear the union.  */
        {
            mtcs_expr_clear_storage/*!clear_storage*/(self,target,mtcs_expr_expr_size/*!expr_size*/(self,exp), BLOCK_OP_NORMAL);
            cleared = 1;
        }
        /* If we are building a static constructor into a register,
           set the initial value as zero so we can fold the value into
           a constant.  But if more than one register is involved,
           this probably loses.  */
        else if (REG_P (target) && TREE_STATIC (exp)
             && known_le (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (target)),
                     mtcs_mode_get_regmode_natural_size/*REGMODE_NATURAL_SIZE*/(mtcsMode,GET_MODE (target)))){
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, CONST0_RTX (GET_MODE (target)));
            cleared = 1;
        }

            /* If the constructor has fewer fields than the structure or
           if we are initializing the structure to mostly zeros, clear
           the whole structure first.  Don't do this if TARGET is a
           register whose mode size isn't equal to SIZE since
           clear_storage can't handle this case.  */
        else if (known_size_p (size)
             && (((int) CONSTRUCTOR_NELTS (exp) != fields_length (type)) || mostly_zeros_p(self,exp))
             && (!REG_P (target) || known_eq (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (target)), size))){
            mtcs_expr_clear_storage/*!clear_storage*/(self,target, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size, Pmode),BLOCK_OP_NORMAL);
            cleared = 1;
        }

        if (REG_P (target) && !cleared)
            mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,target);

        /* Store each element of the constructor into the
           corresponding field of TARGET.  */
        FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (exp), idx, field, value){
            machine_mode mode;
            HOST_WIDE_INT bitsize;
            HOST_WIDE_INT bitpos = 0;
            tree offset;
            rtx to_rtx = target;
            /* Just ignore missing fields.  We cleared the whole
               structure, above, if any fields are missing.  */
            if (field == 0)
              continue;
            if (cleared && initializer_zerop (value))
              continue;
            if (tree_fits_uhwi_p (DECL_SIZE (field)))
              bitsize = tree_to_uhwi (DECL_SIZE (field));
            else
              gcc_unreachable ();

            mode = DECL_MODE (field);
            if (DECL_BIT_FIELD (field))
              mode = VOIDmode;

            offset = DECL_FIELD_OFFSET (field);
            if (tree_fits_shwi_p (offset)  && tree_fits_shwi_p (bit_position (field))){
                bitpos = int_bit_position (field);
                offset = NULL_TREE;
            }else
              gcc_unreachable ();

            /* If this initializes a field that is smaller than a
               word, at the start of a word, try to widen it to a full
               word.  This special case allows us to output C++ member
               function initializations in a form that the optimizers
               can understand.  */
            if (mtcs_reg_get_word_register_operations/*!WORD_REGISTER_OPERATIONS*/(mtcsReg)
                && REG_P (target)
                && bitsize < BITS_PER_WORD
                && bitpos % BITS_PER_WORD == 0
                && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) == MODE_INT
                && TREE_CODE (value) == INTEGER_CST
                && exp_size >= 0
                && bitpos + BITS_PER_WORD <= exp_size * BITS_PER_UNIT){
                type = TREE_TYPE (value);

                if (TYPE_PRECISION (type) < BITS_PER_WORD){
                    type = lang_hooks.types.type_for_mode(word_mode, TYPE_UNSIGNED (type));
                    value = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, value);
                    /* Make sure the bits beyond the original bitsize are zero
                       so that we can correctly avoid extra zeroing stores in
                       later constructor elements.  */
                    tree bitsize_mask = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, wi::mask (bitsize, false,BITS_PER_WORD));
                    value = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,BIT_AND_EXPR, type, value, bitsize_mask);
                }

                if (BYTES_BIG_ENDIAN)
                  value = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,
                        LSHIFT_EXPR, type, value,mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,
                        type,BITS_PER_WORD - bitsize));
                bitsize = BITS_PER_WORD;
                mode = word_mode;
            }

            if (MEM_P (to_rtx) && !MEM_KEEP_ALIAS_SET_P (to_rtx)  && DECL_NONADDRESSABLE_P (field)){
                to_rtx = copy_rtx (to_rtx);
                MEM_KEEP_ALIAS_SET_P (to_rtx) = 1;
            }

            store_constructor_field(self,to_rtx, bitsize, bitpos, 0,
                    bitregion_end, mode, value, cleared, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,TREE_TYPE (field)),reverse);
        }
        break;
      }
    case ARRAY_TYPE:
      {
        tree value, index;
        unsigned HOST_WIDE_INT i;
        bool need_to_clear;
        tree domain;
        tree elttype = TREE_TYPE (type);
        bool const_bounds_p;
        HOST_WIDE_INT minelt = 0;
        HOST_WIDE_INT maxelt = 0;
        /* The storage order is specified for every aggregate type.  */
        reverse = TYPE_REVERSE_STORAGE_ORDER (type);
        domain = TYPE_DOMAIN (type);
        const_bounds_p = (TYPE_MIN_VALUE (domain)
                  && TYPE_MAX_VALUE (domain)
                  && tree_fits_shwi_p (TYPE_MIN_VALUE (domain))
                  && tree_fits_shwi_p (TYPE_MAX_VALUE (domain)));

        /* If we have constant bounds for the range of the type, get them.  */
        if (const_bounds_p){
            minelt = tree_to_shwi (TYPE_MIN_VALUE (domain));
            maxelt = tree_to_shwi (TYPE_MAX_VALUE (domain));
        }

        /* If the constructor has fewer elements than the array, clear
               the whole array first.  Similarly if this is static
               constructor of a non-BLKmode object.  */
        if (cleared)
          need_to_clear = false;
        else if (REG_P (target) && TREE_STATIC (exp))
          need_to_clear = true;
        else{
            unsigned HOST_WIDE_INT idx;
            HOST_WIDE_INT count = 0, zero_count = 0;
            need_to_clear = ! const_bounds_p;

            /* This loop is a more accurate version of the loop in
               mostly_zeros_p (it handles RANGE_EXPR in an index).  It
               is also needed to check for missing elements.  */
            FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (exp), idx, index, value){
                HOST_WIDE_INT this_node_count;
                if (need_to_clear)
                  break;

                if (index != NULL_TREE && TREE_CODE (index) == RANGE_EXPR){
                    tree lo_index = TREE_OPERAND (index, 0);
                    tree hi_index = TREE_OPERAND (index, 1);

                    if (! tree_fits_uhwi_p (lo_index) || ! tree_fits_uhwi_p (hi_index)){
                        need_to_clear = true;
                        break;
                    }

                    this_node_count = (tree_to_uhwi (hi_index)  - tree_to_uhwi (lo_index) + 1);
                }else
                  this_node_count = 1;

                count += this_node_count;
                if (mostly_zeros_p(self,value))
                  zero_count += this_node_count;
            }

            /* Clear the entire array first if there are any missing
               elements, or if the incidence of zero elements is >=
               75%.  */
            if (! need_to_clear  && (count < maxelt - minelt + 1 || 4 * zero_count >= 3 * count))
              need_to_clear = true;
        }

        if (need_to_clear && maybe_gt (size, 0)){
            if (REG_P (target))
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, CONST0_RTX (GET_MODE (target)));
            else
              mtcs_expr_clear_storage/*!clear_storage*/(self,target,
                      mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size, mtcs_mode_get_Pmode(mtcsMode)),BLOCK_OP_NORMAL);
            cleared = 1;
        }

        if (!cleared && REG_P (target))
          /* Inform later passes that the old value is dead.  */
            mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,target);

        /* Store each element of the constructor into the
           corresponding element of TARGET, determined by counting the
           elements.  */
        FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (exp), i, index, value) {
            machine_mode mode;
            poly_int64 bitsize;
            HOST_WIDE_INT bitpos;
            rtx xtarget = target;

            if (cleared && initializer_zerop (value))
              continue;

            mode = TYPE_MODE (elttype);
            if (mode != mtcsMode->modes.M_BLKmode)
              bitsize = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode);
            else if (!poly_int_tree_p (TYPE_SIZE (elttype), &bitsize))
              bitsize = -1;

            if (index != NULL_TREE && TREE_CODE (index) == RANGE_EXPR){
                tree lo_index = TREE_OPERAND (index, 0);
                tree hi_index = TREE_OPERAND (index, 1);
                rtx index_r, pos_rtx;
                HOST_WIDE_INT lo, hi, count;
                tree position;

                /* If the range is constant and "small", unroll the loop.  */
                if (const_bounds_p  && tree_fits_shwi_p (lo_index)  && tree_fits_shwi_p (hi_index)
                    && (lo = tree_to_shwi (lo_index), hi = tree_to_shwi (hi_index), count = hi - lo + 1,
                    (!MEM_P (target) || count <= 2 || (tree_fits_uhwi_p (TYPE_SIZE (elttype))
                     && (tree_to_uhwi (TYPE_SIZE (elttype)) * count <= 40 * 8))))){
                    lo -= minelt;  hi -= minelt;
                    for (; lo <= hi; lo++){
                        bitpos = lo * tree_to_shwi (TYPE_SIZE (elttype));

                        if (MEM_P (target)   && !MEM_KEEP_ALIAS_SET_P (target)
                            && TREE_CODE (type) == ARRAY_TYPE  && TYPE_NONALIASED_COMPONENT (type))            {
                            target = copy_rtx (target);
                            MEM_KEEP_ALIAS_SET_P (target) = 1;
                        }

                        store_constructor_field(self,target, bitsize, bitpos, 0, bitregion_end,
                           mode, value, cleared,mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,elttype), reverse);
                    }
                }else{
                    rtx_code_label *loop_start =mtcs_rtl_gen_label_rtx(mtcsRTL);
                    rtx_code_label *loop_end =mtcs_rtl_gen_label_rtx(mtcsRTL);
                    tree exit_cond;

                    mtcs_expr_expand_normal/*!expand_normal*/(self,hi_index);

                    index = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,EXPR_LOCATION (exp), VAR_DECL, NULL_TREE, domain);
                    index_r = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,
                          mtcs_mode_promote_decl_mode/*!promote_decl_mode*/(mtcsMode,index, NULL));
                    mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,index, index_r);
                    mtcs_expr_store_expr/*!store_expr*/(self,lo_index, index_r, 0, false, reverse);

                    /* Build the head of the loop.  */
                    mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
                    mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_start);

                    /* Assign value to element index.  */
                    position = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype,
                          mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR,
                                 TREE_TYPE (index), index, TYPE_MIN_VALUE (domain)));

                    position = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MULT_EXPR, position,
                          mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype, TYPE_SIZE_UNIT (elttype)));

                    pos_rtx = mtcs_expr_expand_normal/*!expand_normal*/(self,position);
                    xtarget =mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,target, pos_rtx,highest_pow2_factor (position));
                    xtarget =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,xtarget, mode, 0);
                    if (TREE_CODE (value) == CONSTRUCTOR)
                      mtcs_expr_store_constructor/*!store_constructor*/(self,value, xtarget, cleared,
                             exact_div (bitsize, BITS_PER_UNIT),reverse);
                    else
                       mtcs_expr_store_expr/*!store_expr*/(self,value, xtarget, 0, false, reverse);

                    /* Generate a conditional jump to exit the loop.  */
                    exit_cond = build2 (LT_EXPR, integer_type_node, index, hi_index);
                    mtcs_dojump_jumpif/*!jumpif*/(mtcsDojump,exit_cond, loop_end,profile_probability::uninitialized ());

                    /* Update the loop counter, and jump to the head of
                       the loop.  */
                    mtcs_expr_expand_assignment/*!expand_assignment*/(self,index,
                               build2 (PLUS_EXPR, TREE_TYPE (index),index, integer_one_node),false);
                    mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,loop_start);
                    /* Build the end of the loop.  */
                    mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_end);
                }
            }else if ((index != 0 && ! tree_fits_shwi_p (index)) || ! tree_fits_uhwi_p (TYPE_SIZE (elttype))){
                tree position;
                if (index == 0)
                  index = ssize_int (1);
                if (minelt)
                  index = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype,
                        mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR,TREE_TYPE (index), index,TYPE_MIN_VALUE (domain)));

                position =mtcs_const_size_binop/*!size_binop*/(mtcsConst,MULT_EXPR,
                      index,mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype,TYPE_SIZE_UNIT (elttype)));
                xtarget = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,target,
                              mtcs_expr_expand_normal/*!expand_normal*/(self,position),highest_pow2_factor (position));
                xtarget =  mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,xtarget, mode, 0);
                 mtcs_expr_store_expr/*!store_expr*/(self,value, xtarget, 0, false, reverse);
            }else{
                if (index != 0)
                  bitpos = ((tree_to_shwi (index) - minelt)* tree_to_uhwi (TYPE_SIZE (elttype)));
                else
                  bitpos = (i * tree_to_uhwi (TYPE_SIZE (elttype)));

                if (MEM_P (target) && !MEM_KEEP_ALIAS_SET_P (target)
                    && TREE_CODE (type) == ARRAY_TYPE  && TYPE_NONALIASED_COMPONENT (type)){
                    target = copy_rtx (target);
                    MEM_KEEP_ALIAS_SET_P (target) = 1;
                }
                store_constructor_field(self,target, bitsize, bitpos, 0,
                             bitregion_end, mode, value,cleared, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,elttype),reverse);
            }
          }
        break;
      }

    case VECTOR_TYPE:
      {
        unsigned HOST_WIDE_INT idx;
        constructor_elt *ce;
        int i;
        bool need_to_clear;
        insn_code icode = CODE_FOR_nothing;
        tree elt;
        tree elttype = TREE_TYPE (type);
        int elt_size = vector_element_bits (type);
        machine_mode eltmode = TYPE_MODE (elttype);
        eltmode=mtcs_mode_host2device_by_tree(mtcsMode,elttype,eltmode);
        HOST_WIDE_INT bitsize;
        HOST_WIDE_INT bitpos;
        rtvec vector = NULL;
        poly_uint64 n_elts;
        unsigned HOST_WIDE_INT const_n_elts;
        alias_set_type alias;
        bool vec_vec_init_p = false;
        machine_mode mode = GET_MODE (target);

        gcc_assert (eltmode != mtcsMode->modes.M_BLKmode);

        /* Try using vec_duplicate_optab for uniform vectors.  */
        if (!TREE_SIDE_EFFECTS (exp)
            && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)
            && eltmode == mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode)
            && ((icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,vec_duplicate_optab, mode))
            != CODE_FOR_nothing)
            && (elt = uniform_vector_p (exp))
            && !VECTOR_TYPE_P (TREE_TYPE (elt))) {
            class expand_operand ops[2];
            create_output_operand (&ops[0], target, mode);
            create_input_operand (&ops[1], mtcs_expr_expand_normal/*!expand_normal*/(self,elt), eltmode);
            mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 2, ops);
            if (!rtx_equal_p (target, ops[0].value))
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, ops[0].value);
            break;
        }
        /* Use sign-extension for uniform boolean vectors with
           integer modes and single-bit mask entries.
           Effectively "vec_duplicate" for bitmasks.  */
        machine_mode typeMode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
        machine_mode eltTypeMode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (elt),TYPE_MODE (TREE_TYPE (elt)));
        if (elt_size == 1
            && !TREE_SIDE_EFFECTS (exp)
            && VECTOR_BOOLEAN_TYPE_P (type)
            && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,typeMode/*!TYPE_MODE (type)*/)
            && (elt = uniform_vector_p (exp))
            && !VECTOR_TYPE_P (TREE_TYPE (elt))){
            rtx op0 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,eltTypeMode/*!TYPE_MODE (TREE_TYPE (elt))*/,
                     mtcs_expr_expand_normal/*!expand_normal*/(self,elt));
            rtx tmp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
            mtcs_expr_convert_move/*!convert_move*/(self,tmp, op0, 0);

            /* Ensure no excess bits are set.
               GCN needs this for nunits < 64.
               x86 needs this for nunits < 8.  */
            auto nunits = TYPE_VECTOR_SUBPARTS (type).to_constant ();
            if (maybe_ne (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode), nunits))
              tmp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, and_optab, tmp,
                      mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_1U << nunits) - 1),target, true, OPTAB_WIDEN);
            if (tmp != target)
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, tmp);
            break;
        }

        n_elts = TYPE_VECTOR_SUBPARTS (type);
        if (REG_P (target)  && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)  && n_elts.is_constant (&const_n_elts)){
            machine_mode emode = eltmode;
            bool vector_typed_elts_p = false;

            if (CONSTRUCTOR_NELTS (exp) && (TREE_CODE (TREE_TYPE (CONSTRUCTOR_ELT (exp, 0)->value))== VECTOR_TYPE)){
                tree etype = TREE_TYPE (CONSTRUCTOR_ELT (exp, 0)->value);
                gcc_assert (known_eq (CONSTRUCTOR_NELTS (exp) * TYPE_VECTOR_SUBPARTS (etype),n_elts));
                emode = TYPE_MODE (etype);
                emode=mtcs_mode_host2device_by_tree(mtcsMode,etype,emode);
                vector_typed_elts_p = true;
            }
            icode = mtcs_opinit_convert_optab_handler/*!convert_optab_handler*/(mtcsOpinit,vec_init_optab, mode, emode);
            if (icode != CODE_FOR_nothing){
                unsigned int n = const_n_elts;

                if (vector_typed_elts_p){
                    n = CONSTRUCTOR_NELTS (exp);
                    vec_vec_init_p = true;
                }
                vector = rtvec_alloc (n);
                for (unsigned int k = 0; k < n; k++)
                  RTVEC_ELT (vector, k) = CONST0_RTX (emode);
            }
        }

        /* Compute the size of the elements in the CTOR.  It differs
           from the size of the vector type elements only when the
           CTOR elements are vectors themselves.  */
        tree val_type = (CONSTRUCTOR_NELTS (exp) != 0 ? TREE_TYPE (CONSTRUCTOR_ELT (exp, 0)->value) : elttype);
        if (VECTOR_TYPE_P (val_type))
          bitsize = tree_to_uhwi (TYPE_SIZE (val_type));
        else
          bitsize = elt_size;

        /* If the constructor has fewer elements than the vector,
           clear the whole array first.  Similarly if this is static
           constructor of a non-BLKmode object.  */
        if (cleared)
          need_to_clear = false;
        else if (REG_P (target) && TREE_STATIC (exp))
          need_to_clear = true;
        else{
            unsigned HOST_WIDE_INT count = 0, zero_count = 0;
            tree value;
            FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (exp), idx, value){
                int n_elts_here = bitsize / elt_size;
                count += n_elts_here;
                if (mostly_zeros_p(self,value))
                  zero_count += n_elts_here;
            }

            /* Clear the entire vector first if there are any missing elements,
               or if the incidence of zero elements is >= 75%.  */
            need_to_clear = (maybe_lt (count, n_elts) || 4 * zero_count >= 3 * count);
        }

        if (need_to_clear && maybe_gt (size, 0) && !vector){
            if (REG_P (target))
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, CONST0_RTX (mode));
            else
              mtcs_expr_clear_storage/*!clear_storage*/(self,target,
                      mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size,mtcs_mode_get_Pmode(mtcsMode)),BLOCK_OP_NORMAL);
            cleared = 1;
        }

        /* Inform later passes that the old value is dead.  */
        if (!cleared && !vector && REG_P (target) && maybe_gt (n_elts, 1u)){
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, CONST0_RTX (mode));
            cleared = 1;
        }

        if (MEM_P (target))
          alias = mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(mtcsRTL,target);
        else
          alias = mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,elttype);

            /* Store each element of the constructor into the corresponding
           element of TARGET, determined by counting the elements.  */
        for (idx = 0, i = 0; vec_safe_iterate (CONSTRUCTOR_ELTS (exp), idx, &ce); idx++, i += bitsize / elt_size){
            HOST_WIDE_INT eltpos;
            tree value = ce->value;
            if (cleared && initializer_zerop (value))
              continue;
            if (ce->index)
              eltpos = tree_to_uhwi (ce->index);
            else
              eltpos = i;

            if (vector){
                if (vec_vec_init_p){
                    gcc_assert (ce->index == NULL_TREE);
                    gcc_assert (TREE_CODE (TREE_TYPE (value)) == VECTOR_TYPE);
                    eltpos = idx;
                }else
                  gcc_assert (TREE_CODE (TREE_TYPE (value)) != VECTOR_TYPE);
                RTVEC_ELT (vector, eltpos) = mtcs_expr_expand_normal/*!expand_normal*/(self,value);
            }else{
                machine_mode vmode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (value),TYPE_MODE (TREE_TYPE (value)));
                machine_mode value_mode= (TREE_CODE (TREE_TYPE (value)) == VECTOR_TYPE ?
                      vmode/*!TYPE_MODE (TREE_TYPE (value))*/ : eltmode);
                bitpos = eltpos * elt_size;
                store_constructor_field(self,target, bitsize, bitpos, 0,
                             bitregion_end, value_mode, value, cleared, alias, reverse);
            }
        }

        if (vector)
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,MTCS_GEN_FCN/*!GEN_FCN*/(icode) (target,gen_rtx_PARALLEL (mode, vector)));
        break;
      }

    default:
      gcc_unreachable ();
  }
}

/* Add a USE expression for REG to the (possibly empty) list pointed
   to by CALL_FUSAGE.  REG must denote a hard register.  */
//原型 use_reg_mode expr.h expr.cc
void mtcs_expr_use_reg_mode (MtcsExpr *self,rtx *call_fusage, rtx reg, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  gcc_assert (REG_P (reg));

  if (!mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg))
    return;

  *call_fusage= gen_rtx_EXPR_LIST (mode, gen_rtx_USE (VOIDmode, reg), *call_fusage);
}

/* Return an rtx for the size in bytes of the value of EXP.  */
//原型 expr_size expr.h expr.cc
rtx mtcs_expr_expr_size (MtcsExpr *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  tree size;
  if (TREE_CODE (exp) == WITH_SIZE_EXPR)
    size = TREE_OPERAND (exp, 1);
  else{
      size = tree_expr_size (exp);
      gcc_assert (size);
      gcc_assert (size == SUBSTITUTE_PLACEHOLDER_IN_EXPR (size, exp));
  }
  machine_mode typeMode=mtcs_mode_host2device_by_tree(mtcsMode,sizetype,TYPE_MODE (sizetype));
  return mtcs_expr_expand_expr/*!expand_expr*/(self,size, NULL_RTX, typeMode/*!TYPE_MODE (sizetype)*/, EXPAND_NORMAL);
}


/* Variant of convert_modes for ABI parameter passing/return.
   Return an rtx for a value that would result from converting X from
   a floating point mode FMODE to wider integer mode MODE.  */
//原型 convert_float_to_wider_int expr.h expr.cc
rtx mtcs_expr_convert_float_to_wider_int(MtcsExpr *self,machine_mode mode, machine_mode fmode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  gcc_assert (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode) && mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,fmode));
  scalar_int_mode tmp_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,fmode).require ();
  rtx tmp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,tmp_mode, gen_lowpart (tmp_mode, x));
  return mtcs_expr_convert_modes/*!convert_modes*/(self,mode, tmp_mode, tmp, 1);
}


/* Similar, but load SRC into new pseudos in a format that looks like
   PARALLEL.  This can later be fed to emit_group_move to get things
   in the right place.  */
//原型 emit_group_load_into_temps expr.h expr.cc
rtx mtcs_expr_emit_group_load_into_temps (MtcsExpr *self,rtx parallel, rtx src, tree type, poly_int64 ssize)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  rtvec vec;
  int i;
  vec = rtvec_alloc (XVECLEN (parallel, 0));
  emit_group_load_1(self,&RTVEC_ELT (vec, 0), parallel, src, type, ssize);
  /* Convert the vector to look just like the original PARALLEL, except
     with the computed values.  */
  for (i = 0; i < XVECLEN (parallel, 0); i++){
      rtx e = XVECEXP (parallel, 0, i);
      rtx d = XEXP (e, 0);
      if (d){
          d = mtcs_explow_force_reg/*!force_reg */(mtcsExplow,GET_MODE (d), RTVEC_ELT (vec, i));
          e = alloc_EXPR_LIST (REG_NOTE_KIND (e), d, XEXP (e, 1));
      }
      RTVEC_ELT (vec, i) = e;
  }
  return gen_rtx_PARALLEL (GET_MODE (parallel), vec);
}

/* Emit code to move a block SRC to block DST, where SRC and DST are
   non-consecutive groups of registers, each represented by a PARALLEL.  */
//原型 emit_group_move expr.h expr.cc
void mtcs_expr_emit_group_move (MtcsExpr *self,rtx dst, rtx src)
{
  int i;
  gcc_assert (GET_CODE (src) == PARALLEL
          && GET_CODE (dst) == PARALLEL
          && XVECLEN (src, 0) == XVECLEN (dst, 0));
  /* Skip first entry if NULL.  */
  for (i = XEXP (XVECEXP (src, 0, 0), 0) ? 0 : 1; i < XVECLEN (src, 0); i++)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (XVECEXP (dst, 0, i), 0),XEXP (XVECEXP (src, 0, i), 0));
}


/* Move a group of registers represented by a PARALLEL into pseudos.  */
//原型 emit_group_move_into_temps expr.h expr.cc
rtx mtcs_expr_emit_group_move_into_temps(MtcsExpr *self,rtx src)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  rtvec vec = rtvec_alloc (XVECLEN (src, 0));
  int i;
  for (i = 0; i < XVECLEN (src, 0); i++){
      rtx e = XVECEXP (src, 0, i);
      rtx d = XEXP (e, 0);
      if (d)
          e = alloc_EXPR_LIST (REG_NOTE_KIND (e), mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,d), XEXP (e, 1));
      RTVEC_ELT (vec, i) = e;
  }
  return gen_rtx_PARALLEL (GET_MODE (src), vec);
}

/* Variant of convert_modes for ABI parameter passing/return.
   Return an rtx for a value that would result from converting X from
   an integer mode IMODE to a narrower floating point mode MODE.  */
//原型 convert_wider_int_to_float expr.h expr.cc
rtx mtcs_expr_convert_wider_int_to_float (MtcsExpr *self,machine_mode mode, machine_mode imode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  gcc_assert (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,mode)
          && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,imode));
  scalar_int_mode tmp_mode = mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(mtcsMode,mode).require ();
  rtx tmp = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,tmp_mode, gen_lowpart (tmp_mode, x));
  return gen_lowpart_SUBREG (mode, tmp);
}

//原型 fixup_args_size_notes rtl.h expr.cc
poly_int64 mtcs_expr_fixup_args_size_notes (MtcsExpr *self,rtx_insn *prev, rtx_insn *last,poly_int64 end_args_size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  poly_int64 args_size = end_args_size;
  bool saw_unknown = false;
  rtx_insn *insn;

  for (insn = last; insn != prev; insn = PREV_INSN (insn)){
      if (!NONDEBUG_INSN_P (insn))
          continue;

      /* We might have existing REG_ARGS_SIZE notes, e.g. when pushing
     a call argument containing a TLS address that itself requires
     a call to __tls_get_addr.  The handling of stack_pointer_delta
     in emit_single_push_insn is supposed to ensure that any such
     notes are already correct.  */
      rtx note = find_reg_note (insn, REG_ARGS_SIZE, NULL_RTX);
      gcc_assert (!note || known_eq (args_size, get_args_size (note)));

      poly_int64 this_delta = mtcs_expr_find_args_size_adjust/*!find_args_size_adjust*/(self,insn);
      if (known_eq (this_delta, 0)){
          if (!CALL_P (insn)|| mtcs_func_is_accumulate_outgoing_args/*!ACCUMULATE_OUTGOING_ARGS*/(mtcsFunc)
                  || find_reg_note (insn, REG_NORETURN, NULL_RTX) == NULL_RTX)
            continue;
      }

      gcc_assert (!saw_unknown);
      if (known_eq (this_delta, HOST_WIDE_INT_MIN))
          saw_unknown = true;

      if (!note)
          mtcs_rtlanal_add_args_size_note/*!add_args_size_note*/(mtcsRtlanal,insn, args_size);
      if (mtcs_func_get_stack_grows_downward/*!STACK_GROWS_DOWNWARD*/(mtcsFunc))
          this_delta = -poly_uint64 (this_delta);

      if (saw_unknown)
          args_size = HOST_WIDE_INT_MIN;
      else
          args_size -= this_delta;
  }

  return args_size;
}

/* A utility routine used here, in reload, and in try_split.  The insns
   after PREV up to and including LAST are known to adjust the stack,
   with a final value of END_ARGS_SIZE.  Iterate backward from LAST
   placing notes as appropriate.  PREV may be NULL, indicating the
   entire insn sequence prior to LAST should be scanned.

   The set of allowed stack pointer modifications is small:
     (1) One or more auto-inc style memory references (aka pushes),
     (2) One or more addition/subtraction with the SP as destination,
     (3) A single move insn with the SP as destination,
     (4) A call_pop insn,
     (5) Noreturn call insns if !ACCUMULATE_OUTGOING_ARGS.

   Insns in the sequence that do not modify the SP are ignored,
   except for noreturn calls.

   The return value is the amount of adjustment that can be trivially
   verified, via immediate operand or auto-inc.  If the adjustment
   cannot be trivially extracted, the return value is HOST_WIDE_INT_MIN.  */
//原型 find_args_size_adjust rtl.h expr.cc
poly_int64 mtcs_expr_find_args_size_adjust (MtcsExpr *self,rtx_insn *insn)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  rtx dest, set, pat;
  int i;

  pat = PATTERN (insn);
  set = NULL;

  /* Look for a call_pop pattern.  */
  if (CALL_P (insn)){
      /* We have to allow non-call_pop patterns for the case
     of emit_single_push_insn of a TLS address.  */
      if (GET_CODE (pat) != PARALLEL)
          return 0;

      /* All call_pop have a stack pointer adjust in the parallel.
     The call itself is always first, and the stack adjust is
     usually last, so search from the end.  */
      for (i = XVECLEN (pat, 0) - 1; i > 0; --i){
          set = XVECEXP (pat, 0, i);
          if (GET_CODE (set) != SET)
            continue;
          dest = SET_DEST (set);
          if (dest == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
            break;
      }
      /* We'd better have found the stack pointer adjust.  */
      if (i == 0)
          return 0;
      /* Fall through to process the extracted SET and DEST
     as if it was a standalone insn.  */
  }else if (GET_CODE (pat) == SET)
    set = pat;
  else if ((set = single_set (insn)) != NULL)
    ;
  else if (GET_CODE (pat) == PARALLEL){
      /* ??? Some older ports use a parallel with a stack adjust
     and a store for a PUSH_ROUNDING pattern, rather than a
     PRE/POST_MODIFY rtx.  Don't force them to update yet...  */
      /* ??? See h8300 and m68k, pushqi1.  */
      for (i = XVECLEN (pat, 0) - 1; i >= 0; --i){
          set = XVECEXP (pat, 0, i);
          if (GET_CODE (set) != SET)
            continue;
          dest = SET_DEST (set);
          if (dest == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
            break;

          /* We do not expect an auto-inc of the sp in the parallel.  */
          gcc_checking_assert (mem_autoinc_base (dest) != mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
          gcc_checking_assert (mem_autoinc_base (SET_SRC (set))!= mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
      }
      if (i < 0)
          return 0;
  }else
    return 0;

  dest = SET_DEST (set);

  /* Look for direct modifications of the stack pointer.  */
  if (REG_P (dest) && REGNO (dest) ==mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg)){
      /* Look for a trivial adjustment, otherwise assume nothing.  */
      /* Note that the SPU restore_stack_block pattern refers to
     the stack pointer in V4SImode.  Consider that non-trivial.  */
      poly_int64 offset;
      if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (dest))
      && strip_offset (SET_SRC (set), &offset) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
          return offset;
      /* ??? Reload can generate no-op moves, which will be cleaned
     up later.  Recognize it and continue searching.  */
      else if (rtx_equal_p (dest, SET_SRC (set)))
          return 0;
      else
          return HOST_WIDE_INT_MIN;
  }else{
      rtx mem, addr;

      /* Otherwise only think about autoinc patterns.  */
      if (mem_autoinc_base (dest) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)){
          mem = dest;
          gcc_checking_assert (mem_autoinc_base (SET_SRC (set))!= mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
      }else if (mem_autoinc_base (SET_SRC (set)) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL))
          mem = SET_SRC (set);
      else
          return 0;

      addr = XEXP (mem, 0);
      switch (GET_CODE (addr)){
        case PRE_INC:
        case POST_INC:
          return mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (mem));
        case PRE_DEC:
        case POST_DEC:
          return -mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (mem));
        case PRE_MODIFY:
        case POST_MODIFY:
          addr = XEXP (addr, 1);
          gcc_assert (GET_CODE (addr) == PLUS);
          gcc_assert (XEXP (addr, 0) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL));
          return rtx_to_poly_int64 (XEXP (addr, 1));
        default:
          gcc_unreachable ();
      }
  }
}


/* A subroutine of expand_expr_real_1.  Expand gimple assignment G,
   which is known to set an SSA_NAME result.  The other arguments are
   as for expand_expr_real_1.  */
//原型 expand_expr_real_gassign expr.h expr.cc
rtx mtcs_expr_expand_expr_real_gassign (MtcsExpr *self,gassign *g, rtx target, machine_mode tmode,
              enum expand_modifier modifier, rtx *alt_rtl,bool inner_reference_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsCcmp *mtcsCcmp=mtcs_target_get_ccmp(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  separate_ops ops;
  rtx r;
  location_t saved_loc = curr_insn_location ();
  auto loc = gimple_location (g);
  if (loc != UNKNOWN_LOCATION)
     mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,loc);
  tree lhs = gimple_assign_lhs (g);
  ops.code = gimple_assign_rhs_code (g);
  ops.type = TREE_TYPE (lhs);
  switch (get_gimple_rhs_class (ops.code)){
    case GIMPLE_TERNARY_RHS:
      ops.op2 = gimple_assign_rhs3 (g);
      /* Fallthru */
    case GIMPLE_BINARY_RHS:
      ops.op1 = gimple_assign_rhs2 (g);

      /* Try to expand conditonal compare.  */
      if (mtcsTarget->have_ccmp/*!targetm.have_ccmp*/(mtcsTarget)){
          gcc_checking_assert (mtcsTarget->gen_ccmp_next/*!targetm.gen_ccmp_next*/ != NULL);
          machine_mode typeMode=TYPE_MODE (ops.type);//mtcs_mode_host2device_by_tree(mtcsMode,ops.type,TYPE_MODE (ops.type));
          r = mtcs_ccmp_expand_ccmp_expr/*!expand_ccmp_expr*/(mtcsCcmp,g, typeMode/*!TYPE_MODE (ops.type)*/);
          if (r)
            break;
      }
      /* Fallthru */
    case GIMPLE_UNARY_RHS:
      ops.op0 = gimple_assign_rhs1 (g);
      ops.location = loc;
      n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_gassign 调用  mtcs_expr_expand_expr_real_2 tmode:%d target:%p\n",tmode,target);

      r = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(self,&ops, target, tmode, modifier);
      break;
    case GIMPLE_SINGLE_RHS:
      {
        n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_gassign GIMPLE_SINGLE_RHS tmode:%d target:%p\n",tmode,target);
        r = mtcs_expr_expand_expr_real/*!expand_expr_real*/(self,gimple_assign_rhs1 (g), target,tmode, modifier, alt_rtl,inner_reference_p);
        break;
      }
    default:
      gcc_unreachable ();
  }
  mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,saved_loc);
  if (REG_P (r) && !REG_EXPR (r))
      mtcs_rtl_set_reg_attrs_for_decl_rtl/*!set_reg_attrs_for_decl_rtl*/(mtcsRTL,lhs, r);
  return r;
}

/* Optimize x % C1 == C2 for signed modulo if C1 is a power of two and C2
   is non-zero and C3 ((1<<(prec-1)) | (C1 - 1)):
   for C2 > 0 to x & C3 == C2
   for C2 < 0 to x & C3 == (C2 & C3).  */
//原型 maybe_optimize_pow2p_mod_cmp expr.cc
static enum tree_code maybe_optimize_pow2p_mod_cmp (MtcsExpr *self,enum tree_code code, tree *arg0, tree *arg1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  gimple *stmt = get_def_for_expr (*arg0, TRUNC_MOD_EXPR);
  tree treeop0 = gimple_assign_rhs1 (stmt);
  tree treeop1 = gimple_assign_rhs2 (stmt);
  tree type = TREE_TYPE (*arg0);
  machine_mode typeMode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
  scalar_int_mode mode;
  if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,typeMode/*!TYPE_MODE (type)*/, &mode))
    return code;
  if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) != TYPE_PRECISION (type)
      || TYPE_PRECISION (type) <= 1
      || TYPE_UNSIGNED (type)
      /* Signed x % c == 0 should have been optimized into unsigned modulo
     earlier.  */
      || integer_zerop (*arg1)
      /* If c is known to be non-negative, modulo will be expanded as unsigned
     modulo.  */
      || get_range_pos_neg (treeop0) == 1)
    return code;

  /* x % c == d where d < 0 && d <= -c should be always false.  */
  if (tree_int_cst_sgn (*arg1) == -1  && -wi::to_widest (treeop1) >= wi::to_widest (*arg1))
    return code;

  int prec = TYPE_PRECISION (type);
  wide_int w = wi::to_wide (treeop1) - 1;
  w |= wi::shifted_mask (0, prec - 1, true, prec);
  tree c3 = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, w);
  tree c4 = *arg1;
  if (tree_int_cst_sgn (*arg1) == -1)
    c4 = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, w & wi::to_wide (*arg1));

  rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
  treeop0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (treeop0), op0);

  bool speed_p = optimize_insn_for_speed_p ();

  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  location_t loc = gimple_location (stmt);
  struct separate_ops ops;
  ops.code = TRUNC_MOD_EXPR;
  ops.location = loc;
  ops.type = TREE_TYPE (treeop0);
  ops.op0 = treeop0;
  ops.op1 = treeop1;
  ops.op2 = NULL_TREE;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  typeMode=mtcs_mode_host2device_by_tree(mtcsMode,ops.type,TYPE_MODE (ops.type));

  rtx mor = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(self,&ops, NULL_RTX, typeMode/*!TYPE_MODE (ops.type)*/,
                EXPAND_NORMAL);
  rtx_insn *moinsns =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  end_sequence ();

  unsigned mocost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,moinsns, speed_p);
  mocost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,mor, mode, EQ, 0, speed_p);
  mocost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
        mtcs_expr_expand_normal/*!expand_normal*/(self,*arg1), mode, EQ, 1, speed_p);

  ops.code = BIT_AND_EXPR;
  ops.location = loc;
  ops.type = TREE_TYPE (treeop0);
  ops.op0 = treeop0;
  ops.op1 = c3;
  ops.op2 = NULL_TREE;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  typeMode=mtcs_mode_host2device_by_tree(mtcsMode,ops.type,TYPE_MODE (ops.type));
  rtx mur = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(self,&ops, NULL_RTX,
        typeMode/*!TYPE_MODE (ops.type)*/,EXPAND_NORMAL);
  rtx_insn *muinsns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

  unsigned mucost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,muinsns, speed_p);
  mucost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,mur, mode, EQ, 0, speed_p);
  mucost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
          mtcs_expr_expand_normal/*!expand_normal*/(self,c4), mode, EQ, 1, speed_p);

  if (mocost <= mucost){
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,moinsns);
      *arg0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (*arg0), mor);
      return code;
  }

  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,muinsns);
  *arg0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (*arg0), mur);
  *arg1 = c4;
  return code;
}

/* Helper for categorize_ctor_elements.  Identical interface.  */
//原型 categorize_ctor_elements_1 expr.cc
static bool categorize_ctor_elements_1(MtcsExpr *self,const_tree ctor, HOST_WIDE_INT *p_nz_elts,
                HOST_WIDE_INT *p_unique_nz_elts,
                HOST_WIDE_INT *p_init_elts, int *p_complete)
{
  unsigned HOST_WIDE_INT idx;
  HOST_WIDE_INT nz_elts, unique_nz_elts, init_elts, num_fields;
  tree value, purpose, elt_type;

  /* Whether CTOR is a valid constant initializer, in accordance with what
     initializer_constant_valid_p does.  If inferred from the constructor
     elements, true until proven otherwise.  */
  bool const_from_elts_p = constructor_static_from_elts_p (ctor);
  bool const_p = const_from_elts_p ? true : TREE_STATIC (ctor);

  nz_elts = 0;
  unique_nz_elts = 0;
  init_elts = 0;
  num_fields = 0;
  elt_type = NULL_TREE;

  FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), idx, purpose, value){
      HOST_WIDE_INT mult = 1;
      if (purpose && TREE_CODE (purpose) == RANGE_EXPR){
          tree lo_index = TREE_OPERAND (purpose, 0);
          tree hi_index = TREE_OPERAND (purpose, 1);

          if (tree_fits_uhwi_p (lo_index) && tree_fits_uhwi_p (hi_index))
            mult = (tree_to_uhwi (hi_index) - tree_to_uhwi (lo_index) + 1);
      }
      num_fields += mult;
      elt_type = TREE_TYPE (value);

      switch (TREE_CODE (value)){
        case CONSTRUCTOR:
          {
            HOST_WIDE_INT nz = 0, unz = 0, ic = 0;
            bool const_elt_p = categorize_ctor_elements_1(self,value, &nz, &unz,&ic, p_complete);
            nz_elts += mult * nz;
            unique_nz_elts += unz;
            init_elts += mult * ic;
            if (const_from_elts_p && const_p)
              const_p = const_elt_p;
          }
          break;

        case INTEGER_CST:
        case REAL_CST:
        case FIXED_CST:
          if (!initializer_zerop (value)){
              nz_elts += mult;
              unique_nz_elts++;
          }
          init_elts += mult;
          break;

        case STRING_CST:
          nz_elts += mult * TREE_STRING_LENGTH (value);
          unique_nz_elts += TREE_STRING_LENGTH (value);
          init_elts += mult * TREE_STRING_LENGTH (value);
          break;

        case COMPLEX_CST:
          if (!initializer_zerop (TREE_REALPART (value))){
              nz_elts += mult;
              unique_nz_elts++;
          }
          if (!initializer_zerop (TREE_IMAGPART (value))){
              nz_elts += mult;
              unique_nz_elts++;
          }
          init_elts += 2 * mult;
          break;

        case VECTOR_CST:
          {
            /* We can only construct constant-length vectors using
               CONSTRUCTOR.  */
            unsigned int nunits = VECTOR_CST_NELTS (value).to_constant ();
            for (unsigned int i = 0; i < nunits; ++i){
                tree v = VECTOR_CST_ELT (value, i);
                if (!initializer_zerop (v)){
                    nz_elts += mult;
                    unique_nz_elts++;
                }
                init_elts += mult;
            }
          }
          break;

        default:
          {
            HOST_WIDE_INT tc = count_type_elements (elt_type, false);
            nz_elts += mult * tc;
            unique_nz_elts += tc;
            init_elts += mult * tc;

            if (const_from_elts_p && const_p)
              const_p = initializer_constant_valid_p (value,
                            elt_type,TYPE_REVERSE_STORAGE_ORDER(TREE_TYPE (ctor)))
              != NULL_TREE;
          }
          break;
      }
  }

  if (*p_complete && !complete_ctor_at_level_p (TREE_TYPE (ctor),num_fields, elt_type))
    *p_complete = 0;
  else if (TREE_CODE (TREE_TYPE (ctor)) == UNION_TYPE
      || TREE_CODE (TREE_TYPE (ctor)) == QUAL_UNION_TYPE)
    {
      if (*p_complete
     && CONSTRUCTOR_ZERO_PADDING_BITS (ctor)
     && (num_fields
         ? simple_cst_equal (TYPE_SIZE (TREE_TYPE (ctor)),
              TYPE_SIZE (elt_type)) != 1
         : type_has_padding_at_level_p (TREE_TYPE (ctor))))
   *p_complete = 0;
      else if (*p_complete > 0
          && (num_fields
         ? simple_cst_equal (TYPE_SIZE (TREE_TYPE (ctor)),
                   TYPE_SIZE (elt_type)) != 1
         : type_has_padding_at_level_p (TREE_TYPE (ctor))))
   *p_complete = -1;
    }
  else if (*p_complete
      && (CONSTRUCTOR_ZERO_PADDING_BITS (ctor)
          || flag_zero_init_padding_bits == ZERO_INIT_PADDING_BITS_ALL)
      && type_has_padding_at_level_p (TREE_TYPE (ctor)))
    *p_complete = 0;
  else if (*p_complete > 0
      && type_has_padding_at_level_p (TREE_TYPE (ctor)))
    *p_complete = -1;
  *p_nz_elts += nz_elts;
  *p_unique_nz_elts += unique_nz_elts;
  *p_init_elts += init_elts;

  return const_p;
}

/* Examine CTOR to discover:
   * how many scalar fields are set to nonzero values,
     and place it in *P_NZ_ELTS;
   * the same, but counting RANGE_EXPRs as multiplier of 1 instead of
     high - low + 1 (this can be useful for callers to determine ctors
     that could be cheaply initialized with - perhaps nested - loops
     compared to copied from huge read-only data),
     and place it in *P_UNIQUE_NZ_ELTS;
   * how many scalar fields in total are in CTOR,
     and place it in *P_ELT_COUNT.
   * whether the constructor is complete -- in the sense that every
     meaningful byte is explicitly given a value --
     and place it in *P_COMPLETE.

   Return whether or not CTOR is a valid static constant initializer, the same
   as "initializer_constant_valid_p (CTOR, TREE_TYPE (CTOR)) != 0".  */
//原型 categorize_ctor_elements expr.h expr.cc
bool mtcs_expr_categorize_ctor_elements (MtcsExpr *self ,const_tree ctor, HOST_WIDE_INT *p_nz_elts,
              HOST_WIDE_INT *p_unique_nz_elts, HOST_WIDE_INT *p_init_elts, int *p_complete)
{
  *p_nz_elts = 0;
  *p_unique_nz_elts = 0;
  *p_init_elts = 0;
  *p_complete = 1;
  return categorize_ctor_elements_1(self,ctor, p_nz_elts, p_unique_nz_elts,
                     p_init_elts, p_complete);
}

/* Returns true if BASE is a DECL that does not reside in memory and
   has non-BLKmode.  DECL_RTL must not be a MEM; if
   DECL_RTL was not set yet, return false.  */
//原型 non_mem_decl_p expr.h expr.cc
bool mtcs_expr_non_mem_decl_p (MtcsExpr *self,tree base)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  if (!DECL_P (base) || TREE_ADDRESSABLE (base) || DECL_MODE (base) ==mtcsMode->modes.M_BLKmode)
    return false;
  if (!DECL_RTL_SET_P (base))
    return false;
  return (!MEM_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,base)));
}

/* Returns true if REF refers to an object that does not
   reside in memory and has non-BLKmode.  */
//原型 mem_ref_refers_to_non_mem_p expr.h expr.cc
bool mtcs_expr_mem_ref_refers_to_non_mem_p (MtcsExpr *self,tree ref)
{
  tree base;
  if (TREE_CODE (ref) == MEM_REF  || TREE_CODE (ref) == TARGET_MEM_REF){
      tree addr = TREE_OPERAND (ref, 0);
      if (TREE_CODE (addr) != ADDR_EXPR)
          return false;
      base = TREE_OPERAND (addr, 0);
  }else
      base = ref;
  return mtcs_expr_non_mem_decl_p/*!non_mem_decl_p*/(self,base);
}

/* Generate code for computing expression EXP,
   and storing the value into TARGET.

   If the mode is BLKmode then we may return TARGET itself.
   It turns out that in BLKmode it doesn't cause a problem.
   because C has no operators that could combine two different
   assignments into the same BLKmode object with different values
   with no sequence point.  Will other languages need this to
   be more thorough?

   If CALL_PARAM_P is nonzero, this is a store into a call param on the
   stack, and block moves may need to be treated specially.

   If NONTEMPORAL is true, try using a nontemporal store instruction.

   If REVERSE is true, the store is to be done in reverse order.  */
//原型 store_expr expr.h expr.cc
rtx mtcs_expr_store_expr (MtcsExpr *self,tree exp, rtx target, int call_param_p, bool nontemporal, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
  MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsMachine  *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  rtx temp;
  rtx alt_rtl = NULL_RTX;
  location_t loc = curr_insn_location ();
  bool shortened_string_cst = false;
  n_debug("mtcsexpr.c mtcs_expr_store_expr 00 打印 target\n");
  mtcs_print_rtl(stderr,target);
  if (VOID_TYPE_P (TREE_TYPE (exp))){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 11 VOID_TYPE_P\n");
      /* C++ can generate ?: expressions with a throw expression in one
     branch and an rvalue in the other. Here, we resolve attempts to
     store the throw expression's nonexistent result.  */
      gcc_assert (!call_param_p);
      mtcs_expr_expand_expr/*!expand_expr*/(self,exp, const0_rtx, VOIDmode, EXPAND_NORMAL);
      return NULL_RTX;
  }
  if (TREE_CODE (exp) == COMPOUND_EXPR){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 22 COMPOUND_EXPR\n");

      /* Perform first part of compound expression, then assign from second
     part.  */
      mtcs_expr_expand_expr/*!expand_expr*/(self,TREE_OPERAND (exp, 0), const0_rtx, VOIDmode,
           call_param_p ? EXPAND_STACK_PARM : EXPAND_NORMAL);
      return mtcs_expr_store_expr/*!store_expr*/(self,TREE_OPERAND (exp, 1), target,call_param_p, nontemporal, reverse);
  }else if (TREE_CODE (exp) == COND_EXPR && GET_MODE (target) ==mtcsMode->modes.M_BLKmode){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 33 COND_EXPR\n");

      /* For conditional expression, get safe form of the target.  Then
     test the condition, doing the appropriate assignment on either
     side.  This avoids the creation of unnecessary temporaries.
     For non-BLKmode, it is more efficient not to do this.  */

      rtx_code_label *lab1 =mtcs_rtl_gen_label_rtx(mtcsRTL), *lab2 =mtcs_rtl_gen_label_rtx(mtcsRTL);

      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
      /*!NO_DEFER_POP; expr.h 定义*/
      mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop+=1;
      mtcs_dojump_jumpifnot/*!jumpifnot*/(mtcsDojump,TREE_OPERAND (exp, 0), lab1,profile_probability::uninitialized ());
      mtcs_expr_store_expr/*!store_expr*/(self,TREE_OPERAND (exp, 1), target, call_param_p,nontemporal, reverse);
      mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,lab2));
      mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab1);
       mtcs_expr_store_expr/*!store_expr*/(self,TREE_OPERAND (exp, 2), target, call_param_p,nontemporal, reverse);
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab2);
      /*!OK_DEFER_POP;expr.h 定义*/
      mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop-=1;

      return NULL_RTX;
  }else if (GET_CODE (target) == SUBREG && SUBREG_PROMOTED_VAR_P (target)){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 44 SUBREG\n");

    /* If this is a scalar in a register that is stored in a wider mode
       than the declared mode, compute the result into its declared mode
       and then convert to the wider mode.  Our value is the computed
       expression.  */

      rtx inner_target = 0;
      scalar_int_mode outer_mode = mtcs_subreg_unpromoted_mode/*!subreg_unpromoted_mode*/(mtcsMode,target);
      scalar_int_mode inner_mode = mtcs_subreg_promoted_mode/*!subreg_promoted_mode*/(mtcsMode,target);

      /* We can do the conversion inside EXP, which will often result
     in some optimizations.  Do the conversion in two steps: first
     change the signedness, if needed, then the extend.  But don't
     do this if the type of EXP is a subtype of something else
     since then the conversion might involve more than just
     converting modes.  */
      if (INTEGRAL_TYPE_P (TREE_TYPE (exp))   && TREE_TYPE (TREE_TYPE (exp)) == 0
        && mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,outer_mode)  == TYPE_PRECISION (TREE_TYPE (exp))){
          if (!SUBREG_CHECK_PROMOTED_SIGN (target,TYPE_UNSIGNED (TREE_TYPE (exp)))){
              /* Some types, e.g. Fortran's logical*4, won't have a signed
             version, so use the mode instead.  */
              tree ntype = (signed_or_unsigned_type_for(SUBREG_PROMOTED_SIGN (target), TREE_TYPE (exp)));
              if (ntype == NULL)
                  ntype = lang_hooks.types.type_for_mode(
                        mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp))),
                        SUBREG_PROMOTED_SIGN (target));
              exp = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, ntype, exp);
          }

          exp = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, lang_hooks.types.type_for_mode(inner_mode, SUBREG_PROMOTED_SIGN (target)),exp);
          inner_target = SUBREG_REG (target);
      }

      temp = mtcs_expr_expand_expr/*!expand_expr*/(self,exp, inner_target, VOIDmode,call_param_p ? EXPAND_STACK_PARM : EXPAND_NORMAL);


      /* If TEMP is a VOIDmode constant, use convert_modes to make
     sure that we properly convert it.  */
      if (CONSTANT_P (temp) && GET_MODE (temp) == VOIDmode){
          temp = mtcs_expr_convert_modes/*!convert_modes*/(self,outer_mode, TYPE_MODE (TREE_TYPE (exp)),
                    temp, SUBREG_PROMOTED_SIGN (target));
          temp = mtcs_expr_convert_modes/*!convert_modes*/(self,inner_mode, outer_mode, temp,
                    SUBREG_PROMOTED_SIGN (target));
      }else if (!mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,GET_MODE (temp)))
          temp = mtcs_expr_convert_modes/*!convert_modes*/(self,outer_mode, TYPE_MODE (TREE_TYPE (exp)),temp, SUBREG_PROMOTED_SIGN (target));

      mtcs_expr_convert_move/*!convert_move*/(self,SUBREG_REG (target), temp,SUBREG_PROMOTED_SIGN (target));
      return NULL_RTX;
  }else if ((TREE_CODE (exp) == STRING_CST
        || (TREE_CODE (exp) == MEM_REF
        && TREE_CODE (TREE_OPERAND (exp, 0)) == ADDR_EXPR
        && TREE_CODE (TREE_OPERAND (TREE_OPERAND (exp, 0), 0))== STRING_CST
        && integer_zerop (TREE_OPERAND (exp, 1))))
        && !nontemporal && !call_param_p
        && MEM_P (target)){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 55 STRING_CST MEM_REF %s\n",get_tree_code_name(TREE_CODE (exp)));

      /* Optimize initialization of an array with a STRING_CST.  */
      HOST_WIDE_INT exp_len, str_copy_len;
      rtx dest_mem;
      tree str = TREE_CODE (exp) == STRING_CST ? exp : TREE_OPERAND (TREE_OPERAND (exp, 0), 0);

      exp_len = int_expr_size (exp);
      if (exp_len <= 0)
          goto normal_expr;

      if (TREE_STRING_LENGTH (str) <= 0)
          goto normal_expr;
      StringCstReadStr userData={self,(void*)str};
      if (mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(self,exp_len,
              string_cst_read_str, (void *)&userData/*!str*/,mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target), false)){
          mtcs_expr_store_by_pieces/*store_by_pieces*/(self,target, exp_len, string_cst_read_str, (void *)&userData/*!str*/,
                  mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target), false, RETURN_BEGIN);
          return NULL_RTX;
      }

      str_copy_len = TREE_STRING_LENGTH (str);

      /* Trailing NUL bytes in EXP will be handled by the call to
     clear_storage, which is more efficient than copying them from
     the STRING_CST, so trim those from STR_COPY_LEN.  */
      while (str_copy_len){
          if (TREE_STRING_POINTER (str)[str_copy_len - 1])
            break;
          str_copy_len--;
      }
      int storeMaxPieces=mtcs_reg_get_store_max_pieces(mtcsReg);
      if ((storeMaxPieces/*!STORE_MAX_PIECES*/ & (storeMaxPieces/*!STORE_MAX_PIECES*/ - 1)) == 0){
          str_copy_len += storeMaxPieces/*!STORE_MAX_PIECES*/ - 1;
          str_copy_len &= ~(storeMaxPieces/*!STORE_MAX_PIECES*/ - 1);
      }
      if (str_copy_len >= exp_len)
          goto normal_expr;
      StringCstReadStr userData1={self,(void*)str};
      if (!mtcs_expr_can_store_by_pieces(self,str_copy_len, string_cst_read_str,
                (void *)&userData1/*!str*/, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target), false))
          goto normal_expr;

      dest_mem = mtcs_expr_store_by_pieces/*store_by_pieces*/(self,target, str_copy_len, string_cst_read_str,
              (void *)&userData1/*!str*/, mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,target), false,RETURN_END);
      mtcs_expr_clear_storage/*!clear_storage*/(self,mtcs_rtl_adjust_address_1/*!adjust_address_1*/(mtcsRTL,
              dest_mem, mtcsMode->modes.M_BLKmode, 0, 1, 1, 0,
                       exp_len - str_copy_len),mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,exp_len - str_copy_len), BLOCK_OP_NORMAL);
      return NULL_RTX;
  }else{
      rtx tmp_target;
      n_debug("mtcsexpr.c mtcs_expr_store_expr 66 \n");

normal_expr:
      /* If we want to use a nontemporal or a reverse order store, force the
     value into a register first.  */
      tmp_target = nontemporal || reverse ? NULL_RTX : target;
      tree rexp = exp;
      if (TREE_CODE (exp) == STRING_CST && tmp_target == target
         && GET_MODE (target) == mtcsMode->modes.M_BLKmode && TYPE_MODE (TREE_TYPE (exp)) == mtcsMode->modes.M_BLKmode){
         n_debug("mtcsexpr.c mtcs_expr_store_expr 66aa \n");

          rtx size = mtcs_expr_expr_size/*!expr_size*/(self,exp);
          if (CONST_INT_P (size) && size != const0_rtx
              && (UINTVAL (size) > ((unsigned HOST_WIDE_INT) TREE_STRING_LENGTH (exp) + 32))){
             n_debug("mtcsexpr.c mtcs_expr_store_expr 66bb \n");

              /* If the STRING_CST has much larger array type than
             TREE_STRING_LENGTH, only emit the TREE_STRING_LENGTH part of
             it into the rodata section as the code later on will use
             memset zero for the remainder anyway.  See PR95052.  */
              tmp_target = NULL_RTX;
              rexp = copy_node (exp);
              tree index= build_index_type (size_int (TREE_STRING_LENGTH (exp) - 1));
              TREE_TYPE (rexp) = build_array_type (TREE_TYPE (TREE_TYPE (exp)),index);
              shortened_string_cst = true;
            }
      }
      n_debug("mtcsexpr.c mtcs_expr_store_expr 77 \n");
      temp = mtcs_expr_expand_expr_real/*!expand_expr_real*/(self,rexp, tmp_target, GET_MODE (target),
                   (call_param_p ? EXPAND_STACK_PARM : EXPAND_NORMAL),&alt_rtl, false);
      if (shortened_string_cst){
          gcc_assert (MEM_P (temp));
          temp = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,temp,mtcsMode->modes.M_BLKmode, NULL_RTX);
      }
  }

  /* If TEMP is a VOIDmode constant and the mode of the type of EXP is not
     the same as that of TARGET, adjust the constant.  This is needed, for
     example, in case it is a CONST_DOUBLE or CONST_WIDE_INT and we want
     only a word-sized value.  */
  if (CONSTANT_P (temp) && GET_MODE (temp) == VOIDmode
      && TREE_CODE (exp) != ERROR_MARK  && GET_MODE (target) != TYPE_MODE (TREE_TYPE (exp))){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 88 \n");

      gcc_assert (!shortened_string_cst);
      if (mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,GET_MODE (target))
         != mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,TYPE_MODE (TREE_TYPE (exp)))
         && known_eq (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (target)),
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,TYPE_MODE (TREE_TYPE (exp))))){
          rtx t = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx,GET_MODE (target), temp,TYPE_MODE (TREE_TYPE (exp)), 0);
          if (t)
            temp = t;
      }
      if (GET_MODE (temp) == VOIDmode)
         temp = mtcs_expr_convert_modes/*!convert_modes*/(self,GET_MODE (target), TYPE_MODE (TREE_TYPE (exp)),
                  temp, TYPE_UNSIGNED (TREE_TYPE (exp)));
  }

  /* If value was not generated in the target, store it there.
     Convert the value to TARGET's type first if necessary and emit the
     pending incrementations that have been queued when expanding EXP.
     Note that we cannot emit the whole queue blindly because this will
     effectively disable the POST_INC optimization later.

     If TEMP and TARGET compare equal according to rtx_equal_p, but
     one or both of them are volatile memory refs, we have to distinguish
     two cases:
     - expand_expr has used TARGET.  In this case, we must not generate
       another copy.  This can be detected by TARGET being equal according
       to == .
     - expand_expr has not used TARGET - that means that the source just
       happens to have the same RTX form.  Since temp will have been created
       by expand_expr, it will compare unequal according to == .
       We must generate a copy in this case, to reach the correct number
       of volatile memory references.  */

  if ((! rtx_equal_p (temp, target) || (temp != target && (side_effects_p (temp)
          || side_effects_p (target) || (MEM_P (temp)
                && !mtcs_alias_mems_same_for_tbaa_p/*!mems_same_for_tbaa_p*/(mtcsAlias,temp, target)))))
      && TREE_CODE (exp) != ERROR_MARK
      /* If store_expr stores a DECL whose DECL_RTL(exp) == TARGET,
     but TARGET is not valid memory reference, TEMP will differ
     from TARGET although it is really the same location.  */
      && !(alt_rtl
       && rtx_equal_p (alt_rtl, target)
       && !side_effects_p (alt_rtl)
       && !side_effects_p (target))
      /* If there's nothing to copy, don't bother.  Don't call
     expr_size unless necessary, because some front-ends (C++)
     expr_size-hook must not be given objects that are not
     supposed to be bit-copied or bit-initialized.  */
      && mtcs_expr_expr_size/*!expr_size*/(self,exp) != const0_rtx){
     n_debug("mtcsexpr.c mtcs_expr_store_expr 99 \n");

      if (GET_MODE (temp) != GET_MODE (target) && GET_MODE (temp) != VOIDmode){
          gcc_assert (!shortened_string_cst);
          if (GET_MODE (target) == mtcsMode->modes.M_BLKmode){
              /* Handle calls that return BLKmode values in registers.  */
              if (REG_P (temp) && TREE_CODE (exp) == CALL_EXPR)
                  copy_blkmode_from_reg(self,target, temp, TREE_TYPE (exp));
              else
                  mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,target,
                     rtx_to_poly_int64 (mtcs_expr_expr_size/*!expr_size*/(self,exp)) * BITS_PER_UNIT,
                     0, 0, 0, GET_MODE (temp), temp, reverse, false);
          }else
            mtcs_expr_convert_move/*!convert_move*/(self,target, temp, TYPE_UNSIGNED (TREE_TYPE (exp)));
      }else if (GET_MODE (temp) == mtcsMode->modes.M_BLKmode && TREE_CODE (exp) == STRING_CST){
          /* Handle copying a string constant into an array.  The string
             constant may be shorter than the array.  So copy just the string's
             actual length, and clear the rest.  First get the size of the data
             type of the string, which is actually the size of the target.  */
          rtx size = mtcs_expr_expr_size/*!expr_size*/(self,exp);

          if (CONST_INT_P (size)
              && INTVAL (size) < TREE_STRING_LENGTH (exp))
              mtcs_expr_emit_block_move/*!emit_block_move*/(self,target, temp, size,
                     (call_param_p? BLOCK_OP_CALL_PARM : BLOCK_OP_NORMAL));
          else{
              machine_mode pointer_mode=target_addr_space_pointer_mode/*!targetm.addr_space.pointer_mode*/(mtcsMachine->addrSpace,
                      mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsRTL,target));
              machine_mode address_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,target);

              /* Compute the size of the data to copy from the string.  */
              tree copy_size = mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,
                    loc, MIN_EXPR, mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,sizetype, size),
                      size_int (TREE_STRING_LENGTH (exp)));
              rtx copy_size_rtx  = mtcs_expr_expand_expr/*!expand_expr*/(self,copy_size, NULL_RTX, VOIDmode,
                       (call_param_p  ? EXPAND_STACK_PARM : EXPAND_NORMAL));
              rtx_code_label *label = 0;

              /* Copy that much.  */
              copy_size_rtx = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,pointer_mode, copy_size_rtx,
                               TYPE_UNSIGNED (sizetype));
              mtcs_expr_emit_block_move/*!emit_block_move*/(self,target, temp, copy_size_rtx,
                       (call_param_p ? BLOCK_OP_CALL_PARM : BLOCK_OP_NORMAL));

              /* Figure out how much is left in TARGET that we have to clear.
              Do all calculations in pointer_mode.  */
              poly_int64 const_copy_size;
              if (poly_int_rtx_p (copy_size_rtx, &const_copy_size)){
                  size = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,address_mode, size, -const_copy_size);
                  target = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,target, mtcsMode->modes.M_BLKmode, const_copy_size);
              }else{
                  size = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,TYPE_MODE (sizetype), sub_optab, size,
                               copy_size_rtx, NULL_RTX, 0,OPTAB_LIB_WIDEN);

                  if (GET_MODE (copy_size_rtx) != address_mode)
                     copy_size_rtx = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,address_mode,
                                     copy_size_rtx,TYPE_UNSIGNED (sizetype));

                  target = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,target, copy_size_rtx,
                               highest_pow2_factor (copy_size));
                  label =mtcs_rtl_gen_label_rtx(mtcsRTL);
                  mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,size, const0_rtx, LT, NULL_RTX,
                               GET_MODE (size), 0, label);
              }

              if (size != const0_rtx)
                  mtcs_expr_clear_storage/*!clear_storage*/(self,target, size, BLOCK_OP_NORMAL);

              if (label)
                  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
          }
      }else if (shortened_string_cst)
          gcc_unreachable ();
      /* Handle calls that return values in multiple non-contiguous locations.
     The Irix 6 ABI has examples of this.  */
      else if (GET_CODE (target) == PARALLEL){
          if (GET_CODE (temp) == PARALLEL)
              mtcs_expr_emit_group_move/*!emit_group_move*/(self,target, temp);
          else
              mtcs_expr_emit_group_load/*!emit_group_load*/(self,target, temp, TREE_TYPE (exp),
                     int_size_in_bytes (TREE_TYPE (exp)));
      }else if (GET_CODE (temp) == PARALLEL)
          mtcs_expr_emit_group_store/*!emit_group_store*/(self,target, temp, TREE_TYPE (exp),
              int_size_in_bytes (TREE_TYPE (exp)));
      else if (GET_MODE (temp) == mtcsMode->modes.M_BLKmode)
          mtcs_expr_emit_block_move/*!emit_block_move*/(self,target, temp, mtcs_expr_expr_size/*!expr_size*/(self,exp),
             (call_param_p ? BLOCK_OP_CALL_PARM : BLOCK_OP_NORMAL));
      /* If we emit a nontemporal store, there is nothing else to do.  */
      else if (nontemporal && mtcs_expr_emit_storent_insn/*!emit_storent_insn*/(self,target, temp))
              ;
      else{
          if (reverse)
            temp = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,GET_MODE (target), temp);
          temp = mtcs_expr_force_operand/*!force_operand*/(self,temp, target);
          if (temp != target)
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, temp);
      }
  }else
    gcc_assert (!shortened_string_cst);

  return NULL_RTX;
}

/* Determine whether the LEN bytes generated by CONSTFUN can be
   stored to memory using several move instructions.  CONSTFUNDATA is
   a pointer which will be passed as argument in every CONSTFUN call.
   ALIGN is maximum alignment we can assume.  MEMSETP is true if this is
   a memset operation and false if it's a copy of a constant string.
   Return true if a call to store_by_pieces should succeed.  */
//原型 can_store_by_pieces expr.h expr.cc
bool mtcs_expr_can_store_by_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT len,
             by_pieces_constfn constfun,void *constfundata, unsigned int align, bool memsetp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  unsigned HOST_WIDE_INT l;
  unsigned int max_size;
  HOST_WIDE_INT offset = 0;
  enum insn_code icode;
  int reverse;
  /* cst is set but not used if LEGITIMATE_CONSTANT doesn't use it.  */
  rtx cst ATTRIBUTE_UNUSED;

  if (len == 0)
    return true;

  if (!mtcsTarget/*!targetm.use_by_pieces_infrastructure_p*/->use_by_pieces_infrastructure_p(mtcsTarget,len, align,
                           memsetp? SET_BY_PIECES: STORE_BY_PIECES,optimize_insn_for_speed_p ()))
    return false;

  align = alignment_for_piecewise_move(self,mtcs_reg_get_store_max_pieces/*!STORE_MAX_PIECES*/(mtcsReg), align);

  /* We would first store what we can in the largest integer mode, then go to
     successively smaller modes.  */

  for (reverse = 0; reverse <= (mtcs_rtl_have_pre_decrement/*!HAVE_PRE_DECREMENT*/(mtcsRTL)
          || mtcs_rtl_have_post_decrement/*!HAVE_POST_DECREMENT*/(mtcsRTL)); reverse++){
      l = len;
      max_size = mtcs_reg_get_store_max_pieces/*!STORE_MAX_PIECES*/(mtcsReg) + 1;
      while (max_size > 1 && l > 0){
          auto op = memsetp ? SET_BY_PIECES : STORE_BY_PIECES;
          auto mode = widest_fixed_size_mode_for_size(self,max_size, op);

          icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode);
          if (icode != CODE_FOR_nothing  && align >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)){
              unsigned int size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);

              while (l >= size){
                  if (reverse)
                     offset -= size;
                  cst = (*constfun) (constfundata, nullptr, offset, mode);
                  /* All CONST_VECTORs can be loaded for memset since
                     vec_duplicate_optab is a precondition to pick a
                     vector mode for the memset expander.  */
                  if (!((memsetp && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode))
                    || mtcsTarget/*!targetm.legitimate_constant_p*/->legitimate_constant_p(mtcsTarget,mode, cst)))
                     return false;
                  if (!reverse)
                     offset += size;
                  l -= size;
              }
          }
          max_size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
      }
      /* The code above should have handled everything.  */
      gcc_assert (!l);
  }
  return true;
}

/* Generate several move instructions to store LEN bytes generated by
   CONSTFUN to block TO.  (A MEM rtx with BLKmode).  CONSTFUNDATA is a
   pointer which will be passed as argument in every CONSTFUN call.
   ALIGN is maximum alignment we can assume.  MEMSETP is true if this is
   a memset operation and false if it's a copy of a constant string.
   Return value is based on RETMODE argument.  */
//原型 store_by_pieces expr.h expr.cc
rtx mtcs_expr_store_by_pieces(MtcsExpr *self,rtx to, unsigned HOST_WIDE_INT len,
         by_pieces_constfn constfun,void *constfundata, unsigned int align, bool memsetp,memop_ret retmode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  if (len == 0){
      gcc_assert (retmode != RETURN_END_MINUS_ONE);
      return to;
  }
  gcc_assert (mtcsTarget/*!targetm.use_by_pieces_infrastructure_p*/->use_by_pieces_infrastructure_p(mtcsTarget,len, align,
         memsetp ? SET_BY_PIECES : STORE_BY_PIECES,optimize_insn_for_speed_p ()));
  mtcs_store_by_pieces_d data (to, constfun, constfundata, len, align, memsetp ? SET_BY_PIECES : STORE_BY_PIECES,self);
  data.run ();
  if (retmode != RETURN_BEGIN)
    return data.finish_retmode (retmode);
  else
    return to;
}


/* Emits nontemporal store insn that moves FROM to TO.  Returns true if this
   succeeded, false otherwise.  */
//原型 emit_storent_insn expr.h expr.cc
bool mtcs_expr_emit_storent_insn (MtcsExpr *self,rtx to, rtx from)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);

  class expand_operand ops[2];
  machine_mode mode = GET_MODE (to);
  enum insn_code code = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,storent_optab, mode);
  if (code == CODE_FOR_nothing)
    return false;
  create_fixed_operand (&ops[0], to);
  create_input_operand (&ops[1], from, mode);
  return mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,code, 2, ops);
}


/* Expand an assignment that stores the value of FROM into TO.  If NONTEMPORAL
   is true, try generating a nontemporal store.  */
//原型 expand_assignment expr.h expr.cc
void mtcs_expr_expand_assignment(MtcsExpr *self,tree to, tree from, bool nontemporal)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
  MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

  rtx to_rtx = 0;
  rtx result;
  machine_mode mode;
  unsigned int align;
  enum insn_code icode;
  /* Don't crash if the lhs of the assignment was erroneous.  */
  if (TREE_CODE (to) == ERROR_MARK){
      mtcs_expr_expand_normal/*!expand_normal*/(self,from);
      return;
  }
  n_debug("mtcsexpr.c mtcs_expr_expand_assignment 00aa\n");

  /* Optimize away no-op moves without side-effects.  */
  if (operand_equal_p (to, from, 0))
    return;
  n_debug("mtcsexpr.c mtcs_expr_expand_assignment 11aa\n");
  aet_print_tree(to);
  aet_print_tree(from);

  /* Handle misaligned stores.  */
  mode = TYPE_MODE (TREE_TYPE (to));
  if ((TREE_CODE (to) == MEM_REF || TREE_CODE (to) == TARGET_MEM_REF
       || DECL_P (to)) && mode != mtcsMode->modes.M_BLKmode
      && !mem_ref_refers_to_non_mem_p (to)
      && ((align = mtcs_builtins_get_object_alignment/*!get_object_alignment*/(mtcsBuiltins,to))
            < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))
      && (((icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab, mode)) != CODE_FOR_nothing)
      || mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode, align))){
     n_debug("mtcsexpr.cmtcs_expr_expand_assignment 00\n");
      rtx reg, mem;
      reg = mtcs_expr_expand_expr/*!expand_expr*/(self,from, NULL_RTX, VOIDmode, EXPAND_NORMAL);
      /* Handle PARALLEL.  */
      reg = mtcs_expr_maybe_emit_group_store/*!maybe_emit_group_store*/(self,reg, TREE_TYPE (from));
      reg = mtcs_explow_force_not_mem/*!force_not_mem*/(mtcsExplow,reg);
      mem = mtcs_expr_expand_expr/*!expand_expr*/(self,to, NULL_RTX, VOIDmode, EXPAND_WRITE);
      if (TREE_CODE (to) == MEM_REF && REF_REVERSE_STORAGE_ORDER (to))
          reg = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,mode, reg);

      if (icode != CODE_FOR_nothing){
          class expand_operand ops[2];
          create_fixed_operand (&ops[0], mem);
          create_input_operand (&ops[1], reg, mode);
          /* The movmisalign<mode> pattern cannot fail, else the assignment
             would silently be omitted.  */
          mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 2, ops);
      }else
          mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,mem,
                  mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode), 0, 0, 0, mode, reg,false, false);
      return;
  }
  /* Assignment of a structure component needs special treatment
     if the structure component's rtx is not simply a MEM.
     Assignment of an array element at a constant index, and assignment of
     an array element in an unaligned packed structure field, has the same
     problem.  Same for (partially) storing into a non-memory object.  */
  if (handled_component_p (to)   || (TREE_CODE (to) == MEM_REF
      && (REF_REVERSE_STORAGE_ORDER (to)
            || mtcs_expr_mem_ref_refers_to_non_mem_p/*!mem_ref_refers_to_non_mem_p*/(self,to)))
            || TREE_CODE (TREE_TYPE (to)) == ARRAY_TYPE){
     n_debug("mtcsexpr.c mtcs_expr_expand_assignment 11\n");

      machine_mode mode1;
      poly_int64 bitsize, bitpos;
      poly_uint64 bitregion_start = 0;
      poly_uint64 bitregion_end = 0;
      tree offset;
      int unsignedp, reversep, volatilep = 0;
      tree tem;
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      tem = mtcs_expr_get_inner_reference/*!get_inner_reference*/(self,
            to, &bitsize, &bitpos, &offset, &mode1,&unsignedp, &reversep, &volatilep);

      /* Make sure bitpos is not negative, it can wreak havoc later.  */
      if (maybe_lt (bitpos, 0)){
          gcc_assert (offset == NULL_TREE);
          offset = size_int (bits_to_bytes_round_down (bitpos));
          bitpos = num_trailing_bits (bitpos);
      }

      if (TREE_CODE (to) == COMPONENT_REF  && DECL_BIT_FIELD_TYPE (TREE_OPERAND (to, 1)))
          get_bit_range (&bitregion_start, &bitregion_end, to, &bitpos, &offset);
      /* The C++ memory model naturally applies to byte-aligned fields.
     However, if we do not have a DECL_BIT_FIELD_TYPE but BITPOS or
     BITSIZE are not byte-aligned, there is no need to limit the range
     we can access.  This can occur with packed structures in Ada.  */
      else if (maybe_gt (bitsize, 0)  && multiple_p (bitsize, BITS_PER_UNIT)  && multiple_p (bitpos, BITS_PER_UNIT)){
          bitregion_start = bitpos;
          bitregion_end = bitpos + bitsize - 1;
      }

      to_rtx = mtcs_expr_expand_expr/*!expand_expr*/(self,tem, NULL_RTX, VOIDmode, EXPAND_WRITE);
      /* If the field has a mode, we want to access it in the
     field's mode, not the computed mode.
     If a MEM has VOIDmode (external with incomplete type),
     use BLKmode for it instead.  */
      if (MEM_P (to_rtx)){
          if (mode1 != VOIDmode)
            to_rtx = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,to_rtx, mode1, 0);
          else if (GET_MODE (to_rtx) == VOIDmode)
            to_rtx = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,to_rtx, mtcsMode->modes.M_BLKmode, 0);
      }

      rtx stemp = NULL_RTX, old_to_rtx = NULL_RTX;
      if (offset != 0){
          machine_mode address_mode;
          rtx offset_rtx;
          if (!MEM_P (to_rtx)){
              /* We can get constant negative offsets into arrays with broken
             user code.  Translate this to a trap instead of ICEing.  */
              if (TREE_CODE (offset) == INTEGER_CST){
                  expand_builtin_trap ();
                  to_rtx = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, const0_rtx);
              }
              /* Else spill for variable offset to the destination.  We expect
             to run into this only for hard registers.  */
              else{
                  gcc_assert (VAR_P (tem) && DECL_HARD_REGISTER (tem));
                  stemp = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,
                          GET_MODE (to_rtx),mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (to_rtx)));
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,stemp, to_rtx);
                  old_to_rtx = to_rtx;
                  to_rtx = stemp;
              }
          }

          offset_rtx = mtcs_expr_expand_expr/*!expand_expr*/(self,offset, NULL_RTX, VOIDmode, EXPAND_SUM);
          address_mode =mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,to_rtx);
          if (GET_MODE (offset_rtx) != address_mode){
            /* We cannot be sure that the RTL in offset_rtx is valid outside
               of a memory address context, so force it into a register
               before attempting to convert it to the desired mode.  */
              offset_rtx = mtcs_expr_force_operand/*!force_operand*/(self,offset_rtx, NULL_RTX);
              offset_rtx = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,address_mode, offset_rtx, 0);
          }

          /* If we have an expression in OFFSET_RTX and a non-zero
             byte offset in BITPOS, adding the byte offset before the
             OFFSET_RTX results in better intermediate code, which makes
             later rtl optimization passes perform better.

             We prefer intermediate code like this:

             r124:DI=r123:DI+0x18
             [r124:DI]=r121:DI

             ... instead of ...

             r124:DI=r123:DI+0x10
             [r124:DI+0x8]=r121:DI

             This is only done for aligned data values, as these can
             be expected to result in single move instructions.  */
          poly_int64 bytepos;
          if (mode1 != VOIDmode
              && maybe_ne (bitpos, 0)
              && maybe_gt (bitsize, 0)
              && multiple_p (bitpos, BITS_PER_UNIT, &bytepos)
              && multiple_p (bitpos, bitsize)
              && multiple_p (bitsize, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode1))
              && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,to_rtx) >=
                         mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode1))
          {
              to_rtx = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,to_rtx, mode1, bytepos);
              bitregion_start = 0;
              if (known_ge (bitregion_end, poly_uint64 (bitpos)))
                  bitregion_end -= bitpos;
              bitpos = 0;
          }

          to_rtx = mtcs_rtl_offset_address/*!offset_address*/(mtcsRTL,to_rtx, offset_rtx,
                       highest_pow2_factor_for_target (to,offset));
      }

      /* No action is needed if the target is not a memory and the field
     lies completely outside that target.  This can occur if the source
     code contains an out-of-bounds access to a small array.  */
      if (!MEM_P (to_rtx) && GET_MODE (to_rtx) != mtcsMode->modes.M_BLKmode
        && known_ge (bitpos, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,GET_MODE (to_rtx)))){
          mtcs_expr_expand_normal/*!expand_normal*/(self,from);
          result = NULL;
      }
      /* Handle expand_expr of a complex value returning a CONCAT.  */
      else if (GET_CODE (to_rtx) == CONCAT){
          machine_mode to_mode = GET_MODE (to_rtx);
          gcc_checking_assert (mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,to_mode));
          poly_int64 mode_bitsize = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,to_mode);
          unsigned short inner_bitsize =mtcs_mode_get_unit_bitsize/*!GET_MODE_UNIT_BITSIZE*/(mtcsMode,to_mode);
          if (TYPE_MODE (TREE_TYPE (from)) == to_mode && known_eq (bitpos, 0) && known_eq (bitsize, mode_bitsize))
            result = mtcs_expr_store_expr/*!store_expr*/(self,from, to_rtx, false, nontemporal, reversep);
          else if (TYPE_MODE (TREE_TYPE (from)) == mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,to_mode)
               && known_eq (bitsize, inner_bitsize)  && (known_eq (bitpos, 0)  || known_eq (bitpos, inner_bitsize)))
            result = mtcs_expr_store_expr/*!store_expr*/(self,from, XEXP (to_rtx, maybe_ne (bitpos, 0)),
                     false, nontemporal, reversep);
          else if (known_le (bitpos + bitsize, inner_bitsize))
            result = store_field(self,XEXP (to_rtx, 0), bitsize, bitpos,
                      bitregion_start, bitregion_end,
                      mode1, from, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to),
                      nontemporal, reversep);
          else if (known_ge (bitpos, inner_bitsize))
            result = store_field(self,XEXP (to_rtx, 1), bitsize,
                      bitpos - inner_bitsize,
                      bitregion_start, bitregion_end,
                      mode1, from, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to),
                      nontemporal, reversep);
          else if (known_eq (bitpos, 0) && known_eq (bitsize, mode_bitsize)){
              result = mtcs_expr_expand_normal/*!expand_normal*/(self,from);
              if (GET_CODE (result) == CONCAT){
                  to_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,to_mode);
                  machine_mode from_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (result));
                  rtx from_real = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,
                          to_mode, XEXP (result, 0),from_mode, 0);
                  rtx from_imag = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,
                          to_mode, XEXP (result, 1),from_mode, 0);
                  if (!from_real || !from_imag)
                    goto concat_store_slow;
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 0), from_real);
                  mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 1), from_imag);
              }else{
                  machine_mode from_mode = GET_MODE (result) == VOIDmode
                      ? TYPE_MODE (TREE_TYPE (from)) : GET_MODE (result);
                  rtx from_rtx;
                  if (MEM_P (result))
                    from_rtx = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,result, to_mode, NULL_RTX);
                  else
                    from_rtx = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,
                            to_mode, result, from_mode, 0);
                  if (from_rtx){
                      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 0),read_complex_part (from_rtx, false));
                      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 1),read_complex_part (from_rtx, true));
                  }else{
                      to_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,to_mode);
                      rtx from_real =mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,
                              to_mode, result, from_mode, 0);
                      rtx from_imag = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,
                              to_mode, result, from_mode,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,to_mode));
                      if (!from_real || !from_imag)
                          goto concat_store_slow;
                      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 0), from_real);
                      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 1), from_imag);
                  }
              }
          }else{
            concat_store_slow:;
              rtx temp = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (to_rtx),
                            mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (to_rtx)));
              mtcs_expr_write_complex_part/*!write_complex_part*/(self,temp, XEXP (to_rtx, 0), false, true);
              mtcs_expr_write_complex_part/*!write_complex_part*/(self,temp, XEXP (to_rtx, 1), true, false);
              result = store_field(self,temp, bitsize, bitpos, bitregion_start, bitregion_end,
                        mode1, from, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to),nontemporal, reversep);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 0), read_complex_part (temp, false));
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,XEXP (to_rtx, 1), read_complex_part (temp, true));
          }
      }
      /* For calls to functions returning variable length structures, if TO_RTX
     is not a MEM, go through a MEM because we must not create temporaries
     of the VLA type.  */
      else if (!MEM_P (to_rtx)  && TREE_CODE (from) == CALL_EXPR
           && COMPLETE_TYPE_P (TREE_TYPE (from))  && TREE_CODE (TYPE_SIZE (TREE_TYPE (from))) != INTEGER_CST){
         n_debug("mtcsexpr.c mtcs_expr_expand_assignment 22\n");
          rtx temp = mtcs_func_assign_stack_temp/*!assign_stack_temp*/(mtcsFunc,GET_MODE (to_rtx),
                        mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (to_rtx)));
          result = store_field(self,temp, bitsize, bitpos, bitregion_start,
                    bitregion_end, mode1, from, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to),
                    nontemporal, reversep);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,to_rtx, temp);
      }else{
         n_debug("mtcsexpr.c mtcs_expr_expand_assignment 33\n");

          if (MEM_P (to_rtx)){
              /* If the field is at offset zero, we could have been given the
             DECL_RTX of the parent struct.  Don't munge it.  */
              to_rtx = shallow_copy_rtx (to_rtx);
              mtcs_rtl_set_mem_attributes_minus_bitpos/*!set_mem_attributes_minus_bitpos*/(mtcsRTL,to_rtx, to, 0, bitpos);
              if (volatilep)
                  MEM_VOLATILE_P (to_rtx) = 1;
          }

          gcc_checking_assert (known_ge (bitpos, 0));
          if (optimize_bitfield_assignment_op(self,bitsize, bitpos,
                               bitregion_start, bitregion_end,mode1, to_rtx, to, from,reversep))
            result = NULL;
          else if (SUBREG_P (to_rtx)  && SUBREG_PROMOTED_VAR_P (to_rtx)){
              /* If to_rtx is a promoted subreg, we need to zero or sign
             extend the value afterwards.  */
              if (TREE_CODE (to) == MEM_REF
                  && TYPE_MODE (TREE_TYPE (from)) != mtcsMode->modes.M_BLKmode
                  && !REF_REVERSE_STORAGE_ORDER (to)
                  && known_eq (bitpos, 0)
                  && known_eq (bitsize, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (to_rtx))))
                  result = mtcs_expr_store_expr/*!store_expr*/(self,from, to_rtx, 0, nontemporal, false);
              /* Check if the field overlaps the MSB, requiring extension.  */
              else if (maybe_eq (bitpos + bitsize,mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (to_rtx)))){
                  scalar_int_mode imode = mtcs_subreg_unpromoted_mode/*!subreg_unpromoted_mode*/(mtcsMode,to_rtx);
                  scalar_int_mode omode = mtcs_subreg_promoted_mode/*!subreg_promoted_mode*/(mtcsMode,to_rtx);
                  rtx to_rtx1 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,
                          imode, SUBREG_REG (to_rtx),omode);
                  result = store_field(self,to_rtx1, bitsize, bitpos,
                            bitregion_start, bitregion_end,mode1, from,mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to),nontemporal, reversep);
                  /* If the target usually keeps IMODE appropriately
                     extended in OMODE it's unsafe to refer to it using
                     a SUBREG whilst this invariant doesn't hold.  */
                  if (mtcsTarget/*!targetm.mode_rep_extended*/->mode_rep_extended(mtcsTarget,imode, omode) != UNKNOWN)
                    to_rtx1 = mtcs_simplify_rtx_gen_unary/*!simplify_gen_unary*/(mtcsSimplifyRtx,TRUNCATE, imode,SUBREG_REG (to_rtx), omode);
                  mtcs_expr_convert_move/*!convert_move*/(self,SUBREG_REG (to_rtx), to_rtx1,SUBREG_PROMOTED_SIGN (to_rtx));
              }else
                result = store_field(self,to_rtx, bitsize, bitpos,
                              bitregion_start, bitregion_end,mode1, from,mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to), nontemporal, reversep);
          }else
            result = store_field(self,to_rtx, bitsize, bitpos,
                      bitregion_start, bitregion_end, mode1, from, mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,to),nontemporal, reversep);
          /* Move the temporary storage back to the non-MEM_P.  */
          if (stemp)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,old_to_rtx, stemp);
      }

      if (result)
          mtcs_func_preserve_temp_slots/*!preserve_temp_slots*/(mtcsFunc,result);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      return;
  }

  /* If the rhs is a function call and its value is not an aggregate,
     call the function before we start to compute the lhs.
     This is needed for correct code for cases such as
     val = setjmp (buf) on machines where reference to val
     requires loading up part of an address in a separate insn.

     Don't do this if TO is a VAR_DECL or PARM_DECL whose DECL_RTL is REG
     since it might be a promoted variable where the zero- or sign- extension
     needs to be done.  Handling this in the normal way is safe because no
     computation is done before the call.  The same is true for SSA names.  */
  if (TREE_CODE (from) == CALL_EXPR && !mtcs_func_aggregate_value_p/*!aggregate_value_p*/(mtcsFunc,from, from)
      && COMPLETE_TYPE_P (TREE_TYPE (from)) && TREE_CODE (TYPE_SIZE (TREE_TYPE (from))) == INTEGER_CST
      && ! (((VAR_P (to) || TREE_CODE (to) == PARM_DECL  || TREE_CODE (to) == RESULT_DECL)
         && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,to))) || TREE_CODE (to) == SSA_NAME)){
     n_debug("mtcsexpr.c mtcs_expr_expand_assignment 44\n");
      rtx value;
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      value = mtcs_expr_expand_normal/*!expand_normal*/(self,from);
      if (to_rtx == 0)
          to_rtx = mtcs_expr_expand_expr/*!expand_expr*/(self,to, NULL_RTX, VOIDmode, EXPAND_WRITE);

      /* Handle calls that return values in multiple non-contiguous locations.
     The Irix 6 ABI has examples of this.  */
      if (GET_CODE (to_rtx) == PARALLEL){
          if (GET_CODE (value) == PARALLEL)
              mtcs_expr_emit_group_move/*!emit_group_move*/(self,to_rtx, value);
          else
              mtcs_expr_emit_group_load/*!emit_group_load*/(self,to_rtx, value, TREE_TYPE (from),
                     int_size_in_bytes (TREE_TYPE (from)));
      }else if (GET_CODE (value) == PARALLEL)
          mtcs_expr_emit_group_store/*!emit_group_store*/(self,to_rtx, value, TREE_TYPE (from),
                  int_size_in_bytes (TREE_TYPE (from)));
      else if (GET_MODE (to_rtx) == mtcsMode->modes.M_BLKmode){
          /* Handle calls that return BLKmode values in registers.  */
          if (REG_P (value))
            copy_blkmode_from_reg(self,to_rtx, value, TREE_TYPE (from));
          else
              mtcs_expr_emit_block_move/*!emit_block_move*/(self,to_rtx, value,
                    mtcs_expr_expr_size/*!expr_size*/(self,from), BLOCK_OP_NORMAL);
      }else{
          if (POINTER_TYPE_P (TREE_TYPE (to)))
            value = mtcs_explow_convert_memory_address_addr_space/*!convert_memory_address_addr_space*/(mtcsExplow,
                    mtcs_mode_as_a <scalar_int_mode>(mtcsMode,GET_MODE (to_rtx)), value,TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (to))));

          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,to_rtx, value);
      }
      mtcs_func_preserve_temp_slots/*!preserve_temp_slots*/(mtcsFunc,to_rtx);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      return;
  }
  n_debug("mtcsexpr.c mtcs_expr_expand_assignment 55\n");

  /* Ordinary treatment.  Expand TO to get a REG or MEM rtx.  */
  to_rtx = mtcs_expr_expand_expr/*!expand_expr*/(self,to, NULL_RTX, VOIDmode, EXPAND_WRITE);
  n_debug("mtcsexpr.c mtcs_expr_expand_assignment 55-- 打印 rtx\n");
  mtcs_print_rtl(stderr,to_rtx);
  /* Don't move directly into a return register.  */
  if (TREE_CODE (to) == RESULT_DECL && (REG_P (to_rtx) || GET_CODE (to_rtx) == PARALLEL)){
     n_debug("mtcsexpr.c mtcs_expr_expand_assignment 66\n");

      rtx temp;
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      /* If the source is itself a return value, it still is in a pseudo at
     this point so we can move it back to the return register directly.  */
      if (REG_P (to_rtx)  && TYPE_MODE (TREE_TYPE (from)) == mtcsMode->modes.M_BLKmode  && TREE_CODE (from) != CALL_EXPR)
          temp = copy_blkmode_to_reg (GET_MODE (to_rtx), from);
      else
          temp = mtcs_expr_expand_expr/*!expand_expr*/(self,from, NULL_RTX, GET_MODE (to_rtx), EXPAND_NORMAL);

      /* Handle calls that return values in multiple non-contiguous locations.
     The Irix 6 ABI has examples of this.  */
      if (GET_CODE (to_rtx) == PARALLEL){
          if (GET_CODE (temp) == PARALLEL)
              mtcs_expr_emit_group_move/*!emit_group_move*/(self,to_rtx, temp);
          else
              mtcs_expr_emit_group_load/*!emit_group_load*/(self,to_rtx, temp, TREE_TYPE (from),
                     int_size_in_bytes (TREE_TYPE (from)));
      }else if (temp)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,to_rtx, temp);

      mtcs_func_preserve_temp_slots/*!preserve_temp_slots*/(mtcsFunc,to_rtx);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      return;
  }

  /* In case we are returning the contents of an object which overlaps
     the place the value is being stored, use a safe function when copying
     a value through a pointer into a structure value return block.  */
  if (TREE_CODE (to) == RESULT_DECL  && TREE_CODE (from) == INDIRECT_REF
      && ADDR_SPACE_GENERIC_P(TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (TREE_OPERAND (from, 0)))))
      && refs_may_alias_p (to, from) && cfun->returns_struct && !cfun->returns_pcc_struct){
     n_debug("mtcsexpr.c mtcs_expr_expand_assignment 77\n");

      rtx from_rtx, size;
      mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
      size = mtcs_expr_expr_size/*!expr_size*/(self,from);
      from_rtx = mtcs_expr_expand_normal/*!expand_normal*/(self,from);
      mtcs_expr_emit_block_move_via_libcall/*!emit_block_move_via_libcall*/(self,XEXP (to_rtx, 0), XEXP (from_rtx, 0), size);
      mtcs_func_preserve_temp_slots/*!preserve_temp_slots*/(mtcsFunc,to_rtx);
      mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
      return;
  }

  /* Compute FROM and store the value in the rtx we got.  */

  mtcs_func_push_temp_slots/*!push_temp_slots*/(mtcsFunc);
  n_debug("mtcsexpr.c mtcs_expr_expand_assignment 88 store_expr\n");
  result = mtcs_expr_store_expr/*!store_expr*/(self,from, to_rtx, 0, nontemporal, false);
  n_debug("mtcsexpr.c mtcs_expr_expand_assignment 99-- 打印 rtx\n");
  mtcs_print_rtl_single(stderr,result);
  mtcs_func_preserve_temp_slots/*!preserve_temp_slots*/(mtcsFunc,result);
  mtcs_func_pop_temp_slots/*!pop_temp_slots*/(mtcsFunc);
  return;
}

/* Return a form of X that does not use a PARALLEL.  TYPE is the type
   of the value stored in X.  */
//原型 maybe_emit_group_store expr.h expr.cc
rtx mtcs_expr_maybe_emit_group_store (MtcsExpr *self,rtx x, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   machine_mode mode = TYPE_MODE (type);
   gcc_checking_assert (GET_MODE (x) == VOIDmode || GET_MODE (x) == mode);
   if (GET_CODE (x) == PARALLEL){
      rtx result = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      mtcs_expr_emit_group_store/*!emit_group_store*/(self,result, x, type, int_size_in_bytes (type));
      return result;
   }
   return x;
}

/* Expand a setmem pattern; return true if successful.  */
//原型 set_storage_via_setmem expr.h expr.cc
bool mtcs_expr_set_storage_via_setmem (MtcsExpr *self,rtx object, rtx size, rtx val, unsigned int align,
            unsigned int expected_align, HOST_WIDE_INT expected_size,
            unsigned HOST_WIDE_INT min_size,
            unsigned HOST_WIDE_INT max_size,
            unsigned HOST_WIDE_INT probable_max_size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  /* Try the most limited insn first, because there's no point
     including more than one in the machine description unless
     the more limited one has some advantage.  */

  if (expected_align < align)
    expected_align = align;
  if (expected_size != -1){
      if ((unsigned HOST_WIDE_INT)expected_size > max_size)
          expected_size = max_size;
      if ((unsigned HOST_WIDE_INT)expected_size < min_size)
          expected_size = min_size;
  }

  opt_scalar_int_mode mode_iter;
  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode_iter, MODE_INT){
      scalar_int_mode mode = mode_iter.require ();
      enum insn_code code = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,setmem_optab, mode);

      if (code != CODE_FOR_nothing
      /* We don't need MODE to be narrower than BITS_PER_HOST_WIDE_INT
         here because if SIZE is less than the mode mask, as it is
         returned by the macro, it will definitely be less than the
         actual mode mask.  Since SIZE is within the Pmode address
         space, we limit MODE to Pmode.  */
           && ((CONST_INT_P (size)
           && ((unsigned HOST_WIDE_INT) INTVAL (size)
           <= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) >> 1)))
          || max_size <= (mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,mode) >> 1)
          || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) >=
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcs_mode_get_Pmode(mtcsMode))))
      {
          class expand_operand ops[9];
          unsigned int nops;

          nops =mtcs_output_get_n_generator_args(mtcsOutput,code)/*!insn_data[(int) code].n_generator_args*/;
          gcc_assert (nops == 4 || nops == 6 || nops == 8 || nops == 9);

          create_fixed_operand (&ops[0], object);
          /* The check above guarantees that this size conversion is valid.  */
          create_convert_operand_to (&ops[1], size, mode, true);
          create_convert_operand_from (&ops[2], val, byte_mode, true);
          mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[3], align / BITS_PER_UNIT);
          if (nops >= 6){
              mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[4], expected_align / BITS_PER_UNIT);
              mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[5], expected_size);
          }
          if (nops >= 8){
              mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[6], min_size);
              /* If we cannot represent the maximal size,
             make parameter NULL.  */
              if ((HOST_WIDE_INT) max_size != -1)
                  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[7], max_size);
              else
                  create_fixed_operand (&ops[7], NULL);
          }
          if (nops == 9){
              /* If we cannot represent the maximal size,
             make parameter NULL.  */
              if ((HOST_WIDE_INT) probable_max_size != -1)
                  mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[8], probable_max_size);
              else
                  create_fixed_operand (&ops[8], NULL);
          }
          if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,code, nops, ops))
            return true;
      }
  }

  return false;
}

/* A subroutine of emit_move_complex.  Generate a move from Y into X.
   X is known to satisfy push_operand, and MODE is known to be complex.
   Returns the last instruction emitted.  */
//原型 emit_move_complex_push expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_complex_push (MtcsExpr *self,machine_mode mode, rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  scalar_mode submode =mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
  bool imag_first;

//#ifdef PUSH_ROUNDING host=1 nvptx=0
//  poly_int64 submodesize = GET_MODE_SIZE (submode);
//
//  /* In case we output to the stack, but the size is smaller than the
//     machine can push exactly, we need to use move instructions.  */
//  if (maybe_ne (PUSH_ROUNDING (submodesize), submodesize))
//    {
//      x = emit_move_resolve_push (mode, x);
//      return emit_move_insn (x, y);
//    }
//#endif

  /* Note that the real part always precedes the imag part in memory
     regardless of machine's endianness.  */
  switch (GET_CODE (XEXP (x, 0))){
    case PRE_DEC:
    case POST_DEC:
      imag_first = true;
      break;
    case PRE_INC:
    case POST_INC:
      imag_first = false;
      break;
    default:
      gcc_unreachable ();
  }
  mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,gen_rtx_MEM (submode, XEXP (x, 0)),  read_complex_part (y, imag_first));
  return mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,gen_rtx_MEM (submode, XEXP (x, 0)), read_complex_part (y, !imag_first));
}

/* A subroutine of emit_move_complex.  Perform the move from Y to X
   via two moves of the parts.  Returns the last instruction emitted.  */
//原型 emit_move_complex_parts expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_complex_parts(MtcsExpr *self,rtx x, rtx y)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  /* Show the output dies here.  This is necessary for SUBREGs
     of pseudos since we cannot track their lifetimes correctly;
     hard regs shouldn't appear here except as return values.  */
  if (!reload_completed && !reload_in_progress  && REG_P (x) && !reg_overlap_mentioned_p (x, y))
      mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,x);

  mtcs_expr_write_complex_part/*!write_complex_part*/(self,x, read_complex_part (y, false), false, true);
  mtcs_expr_write_complex_part/*!write_complex_part*/(self,x, read_complex_part (y, true), true, false);

  return mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
}

/* Generate several move instructions to copy LEN bytes from block FROM to
   block TO.  (These are MEM rtx's with BLKmode).

   If PUSH_ROUNDING is defined and TO is NULL, emit_single_push_insn is
   used to push FROM to the stack.

   ALIGN is maximum stack alignment we can assume.

   Return value is based on RETMODE argument.  */
//原型 move_by_pieces rtl.h expr.cc
rtx mtcs_expr_move_by_pieces(MtcsExpr *self,rtx to, rtx from, unsigned HOST_WIDE_INT len,
        unsigned int align, memop_ret retmode)
{
//#ifndef PUSH_ROUNDING //host=1 nvptx=0
//  if (to == NULL)
//    gcc_unreachable ();
//#endif
  mtcs_move_by_pieces_d data (to, from, len, align,self);
  data.run ();
  if (retmode != RETURN_BEGIN)
    return data.finish_retmode (retmode);
  else
    return to;
}

/* Copy all or part of a BLKmode value X out of registers starting at REGNO.
   The number of registers to be filled is NREGS.  */
//原型 move_block_from_reg expr.h expr.cc
void mtcs_expr_move_block_from_reg (MtcsExpr *self,int regno, rtx x, int nregs)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  if (nregs == 0)
    return;
  /* See if the machine can do this with a store multiple insn.  */
  if (target_rtx_have_store_multiple/*!targetm.have_store_multiple*/(mtcsMachine->tmrtx)){
      rtx_insn *last = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      rtx first = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, regno);
      if (rtx_insn *pat = target_rtx_gen_store_multiple/*!targetm.gen_store_multiple*/
              (mtcsMachine->tmrtx,x, first,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,nregs))){
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
          return;
      }else
          mtcs_rtl_data_delete_insns_since/*!delete_insns_since*/(mtcsRtlData,last);
  }

  for (int i = 0; i < nregs; i++) {
      rtx tem = mtcs_rtl_operand_subword/*!operand_subword*/(mtcsRTL,x, i, 1, mtcsMode->modes.M_BLKmode);
      gcc_assert (tem);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,
              tem, mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mtcsMode->word_mode/*!word_mode*/, regno + i));
  }
}

/* This is run at the start of compiling a function.  */
//原型 init_expr expr.h expr.cc
void mtcs_expr_init_expr (MtcsExpr *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  /*!  memset (&crtl->expr, 0, sizeof (crtl->expr));*/
  memset (&mtcsRtlData->expr, 0, sizeof (mtcsRtlData->expr));
}

/* Attempt to optimize unsigned (X % C1) == C2 (or (X % C1) != C2).
   If C1 is odd to:
   (X - C2) * C3 <= C4 (or >), where
   C3 is modular multiplicative inverse of C1 and 1<<prec and
   C4 is ((1<<prec) - 1) / C1 or ((1<<prec) - 1) / C1 - 1 (the latter
   if C2 > ((1<<prec) - 1) % C1).
   If C1 is even, S = ctz (C1) and C2 is 0, use
   ((X * C3) r>> S) <= C4, where C3 is modular multiplicative
   inverse of C1>>S and 1<<prec and C4 is (((1<<prec) - 1) / (C1>>S)) >> S.

   For signed (X % C1) == 0 if C1 is odd to (all operations in it
   unsigned):
   (X * C3) + C4 <= 2 * C4, where
   C3 is modular multiplicative inverse of (unsigned) C1 and 1<<prec and
   C4 is ((1<<(prec - 1) - 1) / C1).
   If C1 is even, S = ctz(C1), use
   ((X * C3) + C4) r>> S <= (C4 >> (S - 1))
   where C3 is modular multiplicative inverse of (unsigned)(C1>>S) and 1<<prec
   and C4 is ((1<<(prec - 1) - 1) / (C1>>S)) & (-1<<S).

   See the Hacker's Delight book, section 10-17.  */
//原型 maybe_optimize_mod_cmp expr.h expr.cc
enum tree_code mtcs_expr_maybe_optimize_mod_cmp (MtcsExpr *self,enum tree_code code, tree *arg0, tree *arg1)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  gcc_checking_assert (code == EQ_EXPR || code == NE_EXPR);
  gcc_checking_assert (TREE_CODE (*arg1) == INTEGER_CST);

  if (opts->x_optimize < 2)
    return code;

  gimple *stmt = get_def_for_expr (*arg0, TRUNC_MOD_EXPR);
  if (stmt == NULL)
    return code;

  tree treeop0 = gimple_assign_rhs1 (stmt);
  tree treeop1 = gimple_assign_rhs2 (stmt);
  if (TREE_CODE (treeop0) != SSA_NAME || TREE_CODE (treeop1) != INTEGER_CST
      /* Don't optimize the undefined behavior case x % 0;
     x % 1 should have been optimized into zero, punt if
     it makes it here for whatever reason;
     x % -c should have been optimized into x % c.  */
      || compare_tree_int (treeop1, 2) <= 0
      /* Likewise x % c == d where d >= c should be always false.  */
      || tree_int_cst_le (treeop1, *arg1))
    return code;

  /* Unsigned x % pow2 is handled right already, for signed
     modulo handle it in maybe_optimize_pow2p_mod_cmp.  */
  if (integer_pow2p (treeop1))
    return maybe_optimize_pow2p_mod_cmp(self,code, arg0, arg1);

  tree type = TREE_TYPE (*arg0);
  scalar_int_mode mode;
  if (!mtcs_mode_is_a <scalar_int_mode> (mtcsMode,TYPE_MODE (type), &mode))
    return code;
  if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) != TYPE_PRECISION (type)
      || TYPE_PRECISION (type) <= 1)
    return code;

  signop sgn = UNSIGNED;
  /* If both operands are known to have the sign bit clear, handle
     even the signed modulo case as unsigned.  treeop1 is always
     positive >= 2, checked above.  */
  if (!TYPE_UNSIGNED (type) && get_range_pos_neg (treeop0) != 1)
    sgn = SIGNED;

  if (!TYPE_UNSIGNED (type)){
      if (tree_int_cst_sgn (*arg1) == -1)
          return code;
      type = unsigned_type_for (type);
      if (!type || TYPE_MODE (type) != TYPE_MODE (TREE_TYPE (*arg0)))
          return code;
  }

  int prec = TYPE_PRECISION (type);
  wide_int w = wi::to_wide (treeop1);
  int shift = wi::ctz (w);
  /* Unsigned (X % C1) == C2 is equivalent to (X - C2) % C1 == 0 if
     C2 <= -1U % C1, because for any Z >= 0U - C2 in that case (Z % C1) != 0.
     If C1 is odd, we can handle all cases by subtracting
     C4 below.  We could handle even the even C1 and C2 > -1U % C1 cases
     e.g. by testing for overflow on the subtraction, punt on that for now
     though.  */
  if ((sgn == SIGNED || shift) && !integer_zerop (*arg1)){
      if (sgn == SIGNED)
          return code;
      wide_int x = wi::umod_trunc (wi::mask (prec, false, prec), w);
      if (wi::gtu_p (wi::to_wide (*arg1), x))
          return code;
  }

  imm_use_iterator imm_iter;
  use_operand_p use_p;
  FOR_EACH_IMM_USE_FAST (use_p, imm_iter, treeop0){
      gimple *use_stmt = USE_STMT (use_p);
      /* Punt if treeop0 is used in the same bb in a division
     or another modulo with the same divisor.  We should expect
     the division and modulo combined together.  */
      if (use_stmt == stmt || gimple_bb (use_stmt) != gimple_bb (stmt))
          continue;
      if (!is_gimple_assign (use_stmt)
        || (gimple_assign_rhs_code (use_stmt) != TRUNC_DIV_EXPR
          && gimple_assign_rhs_code (use_stmt) != TRUNC_MOD_EXPR))
          continue;
      if (gimple_assign_rhs1 (use_stmt) != treeop0
        || !operand_equal_p (gimple_assign_rhs2 (use_stmt), treeop1, 0))
          continue;
      return code;
  }

  w = wi::lrshift (w, shift);
  wide_int a = wide_int::from (w, prec + 1, UNSIGNED);
  wide_int b = wi::shifted_mask (prec, 1, false, prec + 1);
  wide_int m = wide_int::from (wi::mod_inv (a, b), prec, UNSIGNED);
  tree c3 = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, m);
  tree c5 = NULL_TREE;
  wide_int d, e;
  if (sgn == UNSIGNED){
      d = wi::divmod_trunc (wi::mask (prec, false, prec), w, UNSIGNED, &e);
      /* Use <= floor ((1<<prec) - 1) / C1 only if C2 <= ((1<<prec) - 1) % C1,
     otherwise use < or subtract one from C4.  E.g. for
     x % 3U == 0 we transform this into x * 0xaaaaaaab <= 0x55555555, but
     x % 3U == 1 already needs to be
     (x - 1) * 0xaaaaaaabU <= 0x55555554.  */
      if (!shift && wi::gtu_p (wi::to_wide (*arg1), e))
          d -= 1;
      if (shift)
          d = wi::lrshift (d, shift);
  }else{
      e = wi::udiv_trunc (wi::mask (prec - 1, false, prec), w);
      if (!shift)
          d = wi::lshift (e, 1);
      else{
          e = wi::bit_and (e, wi::mask (shift, true, prec));
          d = wi::lrshift (e, shift - 1);
      }
      c5 = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, e);
  }
  tree c4 = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,type, d);

  rtx op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
  treeop0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (treeop0), op0);

  bool speed_p = optimize_insn_for_speed_p ();

  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  location_t loc = gimple_location (stmt);
  struct separate_ops ops;
  ops.code = TRUNC_MOD_EXPR;
  ops.location = loc;
  ops.type = TREE_TYPE (treeop0);
  ops.op0 = treeop0;
  ops.op1 = treeop1;
  ops.op2 = NULL_TREE;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  rtx mor = mtcs_expr_expand_expr_real_2/*!expand_expr_real_2*/(self,&ops, NULL_RTX,
          TYPE_MODE (ops.type),EXPAND_NORMAL);
  rtx_insn *moinsns =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);

  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

  unsigned mocost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,moinsns, speed_p);
  mocost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,mor, mode, EQ, 0, speed_p);
  mocost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
          mtcs_expr_expand_normal/*!expand_normal*/(self,*arg1), mode, EQ, 1, speed_p);

  tree t = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, treeop0);
  if (!integer_zerop (*arg1))
    t = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MINUS_EXPR, type, t, mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, *arg1));
  t = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, MULT_EXPR, type, t, c3);
  if (sgn == SIGNED)
    t = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, PLUS_EXPR, type, t, c5);
  if (shift){
      tree s = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,NULL_TREE, shift);
      t = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, RROTATE_EXPR, type, t, s);
  }

  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  rtx mur =mtcs_expr_expand_normal/*!expand_normal*/(self,t);
  rtx_insn *muinsns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

  unsigned mucost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,muinsns, speed_p);
  mucost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,mur, mode, LE, 0, speed_p);
  mucost += mtcs_rtlanal_rtx_cost/*!rtx_cost*/(mtcsRtlanal,
          mtcs_expr_expand_normal/*!expand_normal*/(self,c4), mode, LE, 1, speed_p);

  if (mocost <= mucost){
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,moinsns);
      *arg0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,TREE_TYPE (*arg0), mor);
      return code;
  }

  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,muinsns);
  *arg0 = mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,type, mur);
  *arg1 = c4;
  return code == EQ_EXPR ? LE_EXPR : GT_EXPR;
}

/* Subroutine of expand_expr: return true iff there is no way that
   EXP can reference X, which is being modified.  TOP_P is nonzero if this
   call is going to be used to determine whether we need a temporary
   for EXP, as opposed to a recursive call to this function.

   It is always safe for this routine to return false since it merely
   searches for optimization opportunities.  */
//原型 safe_from_p expr.h expr.cc
bool mtcs_expr_safe_from_p (MtcsExpr *self,const_rtx x, tree exp, int top_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

  rtx exp_rtl = 0;
  int i, nops;
  rtx virtuaOutgoingArgsRtx=  mtcs_rtl_get_virtual_outgoing_args_rtx(mtcsRTL);
  int firstPseudoRegister=mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

  if (x == 0
      /* If EXP has varying size, we MUST use a target since we currently
     have no way of allocating temporaries of variable size
     (except for arrays that have TYPE_ARRAY_MAX_SIZE set).
     So we assume here that something at a higher level has prevented a
     clash.  This is somewhat bogus, but the best we can do.  Only
     do this when X is BLKmode and when we are at the top level.  */
      || (top_p && TREE_TYPE (exp) != 0 && COMPLETE_TYPE_P (TREE_TYPE (exp))
      && TREE_CODE (TYPE_SIZE (TREE_TYPE (exp))) != INTEGER_CST
      && (TREE_CODE (TREE_TYPE (exp)) != ARRAY_TYPE
          || TYPE_ARRAY_MAX_SIZE (TREE_TYPE (exp)) == NULL_TREE
          || TREE_CODE (TYPE_ARRAY_MAX_SIZE (TREE_TYPE (exp)))
          != INTEGER_CST)
      && GET_MODE (x) == mtcsMode->modes.M_BLKmode)
      /* If X is in the outgoing argument area, it is always safe.  */
      || (MEM_P (x)
      && (XEXP (x, 0) == virtuaOutgoingArgsRtx/*!virtual_outgoing_args_rtx*/
          || (GET_CODE (XEXP (x, 0)) == PLUS
          && XEXP (XEXP (x, 0), 0) == virtuaOutgoingArgsRtx/*!virtual_outgoing_args_rtx*/))))
    return true;

  /* If this is a subreg of a hard register, declare it unsafe, otherwise,
     find the underlying pseudo.  */
  if (GET_CODE (x) == SUBREG){
      x = SUBREG_REG (x);
      if (REG_P (x) && REGNO (x) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
          return false;
  }

  /* Now look at our tree code and possibly recurse.  */
  switch (TREE_CODE_CLASS (TREE_CODE (exp))){
    case tcc_declaration:
      exp_rtl = DECL_RTL_IF_SET (exp);
      break;
    case tcc_constant:
      return true;
    case tcc_exceptional:
      if (TREE_CODE (exp) == TREE_LIST){
          while (1){
              if (TREE_VALUE (exp) && !mtcs_expr_safe_from_p(self,x, TREE_VALUE (exp), 0))
                  return false;
              exp = TREE_CHAIN (exp);
              if (!exp)
                  return true;
              if (TREE_CODE (exp) != TREE_LIST)
                  return mtcs_expr_safe_from_p(self,x, exp, 0);
          }
      }else if (TREE_CODE (exp) == CONSTRUCTOR){
          constructor_elt *ce;
          unsigned HOST_WIDE_INT idx;

          FOR_EACH_VEC_SAFE_ELT (CONSTRUCTOR_ELTS (exp), idx, ce)
            if ((ce->index != NULL_TREE && !mtcs_expr_safe_from_p(self,x, ce->index, 0))
            || !mtcs_expr_safe_from_p(self,x, ce->value, 0))
              return false;
          return true;
      }else if (TREE_CODE (exp) == ERROR_MARK)
          return true;    /* An already-visited SAVE_EXPR? */
      else
          return false;

    case tcc_statement:
      /* The only case we look at here is the DECL_INITIAL inside a
     DECL_EXPR.  */
      return (TREE_CODE (exp) != DECL_EXPR
          || TREE_CODE (DECL_EXPR_DECL (exp)) != VAR_DECL
          || !DECL_INITIAL (DECL_EXPR_DECL (exp))
          || mtcs_expr_safe_from_p(self,x, DECL_INITIAL (DECL_EXPR_DECL (exp)), 0));

    case tcc_binary:
    case tcc_comparison:
      if (!mtcs_expr_safe_from_p(self,x, TREE_OPERAND (exp, 1), 0))
          return false;
      /* Fall through.  */
    case tcc_unary:
      return mtcs_expr_safe_from_p(self,x, TREE_OPERAND (exp, 0), 0);
    case tcc_expression:
    case tcc_reference:
    case tcc_vl_exp:
      /* Now do code-specific tests.  EXP_RTL is set to any rtx we find in
     the expression.  If it is set, we conflict iff we are that rtx or
     both are in memory.  Otherwise, we check all operands of the
     expression recursively.  */
      switch (TREE_CODE (exp)){
        case ADDR_EXPR:
          /* If the operand is static or we are static, we can't conflict.
             Likewise if we don't conflict with the operand at all.  */
          if (staticp (TREE_OPERAND (exp, 0))
              || TREE_STATIC (exp) || mtcs_expr_safe_from_p(self,x, TREE_OPERAND (exp, 0), 0))
            return true;
          /* Otherwise, the only way this can conflict is if we are taking
             the address of a DECL a that address if part of X, which is
             very rare.  */
          exp = TREE_OPERAND (exp, 0);
          if (DECL_P (exp)){
              if (!DECL_RTL_SET_P (exp) || !MEM_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,exp)))
                  return false;
              else
                  exp_rtl = XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,exp), 0);
          }
          break;
        case MEM_REF:
          if (MEM_P (x)  && mtcs_alias_alias_sets_conflict_p/*!alias_sets_conflict_p*/(mtcsAlias,
                mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(mtcsRTL,x), mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,exp)))
            return false;
          break;
        case CALL_EXPR:
          /* Assume that the call will clobber all hard registers and
             all of memory.  */
          if ((REG_P (x) && REGNO (x) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/) || MEM_P (x))
            return false;
          break;
        case WITH_CLEANUP_EXPR:
        case CLEANUP_POINT_EXPR:
          /* Lowered by gimplify.cc.  */
          gcc_unreachable ();
        case SAVE_EXPR:
          return mtcs_expr_safe_from_p(self,x, TREE_OPERAND (exp, 0), 0);
        default:
          break;
      }
      /* If we have an rtx, we do not need to scan our operands.  */
      if (exp_rtl)
          break;
      nops = TREE_OPERAND_LENGTH (exp);
      for (i = 0; i < nops; i++)
          if (TREE_OPERAND (exp, i) != 0 && ! mtcs_expr_safe_from_p(self,x, TREE_OPERAND (exp, i), 0))
              return false;
      break;
    case tcc_type:
      /* Should never get a type here.  */
      gcc_unreachable ();
  }
  /* If we have an rtl, find any enclosed object.  Then see if we conflict
     with it.  */
  if (exp_rtl){
      if (GET_CODE (exp_rtl) == SUBREG){
          exp_rtl = SUBREG_REG (exp_rtl);
          if (REG_P (exp_rtl)  && REGNO (exp_rtl) <firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/)
            return false;
      }
      /* If the rtl is X, then it is not safe.  Otherwise, it is unless both
     are memory and they conflict.  */
      return ! (rtx_equal_p (x, exp_rtl)
        || (MEM_P (x) && MEM_P (exp_rtl)
            && mtcs_alias_true_dependence/*!true_dependence*/(mtcsAlias,exp_rtl, VOIDmode, x)));
  }
  /* If we reach here, it is safe.  */
  return true;
}


/* Subroutine of expand_expr.  Expand the two operands of a binary
   expression EXP0 and EXP1 placing the results in OP0 and OP1.
   The value may be stored in TARGET if TARGET is nonzero.  The
   MODIFIER argument is as documented by expand_expr.  */
//原型 expand_operands expr.h expr.cc
void mtcs_expr_expand_operands (MtcsExpr *self,tree exp0, tree exp1, rtx target, rtx *op0, rtx *op1,
         enum expand_modifier modifier)
{
  if (! mtcs_expr_safe_from_p/*!safe_from_p*/(self,target, exp1, 1))
    target = 0;
  if (operand_equal_p (exp0, exp1, 0)){
      *op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,exp0, target, VOIDmode, modifier);
      *op1 = copy_rtx (*op0);
  }else{
      *op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,exp0, target, VOIDmode, modifier);
      *op1 = mtcs_expr_expand_expr/*!expand_expr*/(self,exp1, NULL_RTX, VOIDmode, modifier);
  }
}

/* Helper function of expand_expr_2, expand a division or modulo.
   op0 and op1 should be already expanded treeop0 and treeop1, using
   expand_operands.  */
//原型 expand_expr_divmod expr.cc
static rtx expand_expr_divmod (MtcsExpr *self,tree_code code, machine_mode mode, tree treeop0,
            tree treeop1, rtx op0, rtx op1, rtx target, int unsignedp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

  bool mod_p = (code == TRUNC_MOD_EXPR || code == FLOOR_MOD_EXPR
        || code == CEIL_MOD_EXPR || code == ROUND_MOD_EXPR);
  if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)
      && opts->x_optimize >= 2
      && get_range_pos_neg (treeop0) == 1
      && get_range_pos_neg (treeop1) == 1){
      /* If both arguments are known to be positive when interpreted
     as signed, we can expand it as both signed and unsigned
     division or modulo.  Choose the cheaper sequence in that case.  */
      bool speed_p = optimize_insn_for_speed_p ();
      mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      rtx uns_ret = mtcs_expmed_expand_divmod/*!expand_divmod*/(mtcsExpmed,mod_p, code, mode, op0, op1, target, 1);
      rtx_insn *uns_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      rtx sgn_ret = mtcs_expmed_expand_divmod/*!expand_divmod*/(mtcsExpmed,mod_p, code, mode, op0, op1, target, 0);
      rtx_insn *sgn_insns =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);

      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      unsigned uns_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,uns_insns, speed_p);
      unsigned sgn_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,sgn_insns, speed_p);

      /* If costs are the same then use as tie breaker the other other
     factor.  */
      if (uns_cost == sgn_cost){
          uns_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,uns_insns, !speed_p);
          sgn_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,sgn_insns, !speed_p);
      }

      if (uns_cost < sgn_cost || (uns_cost == sgn_cost && unsignedp)){
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,uns_insns);
          return uns_ret;
      }
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,sgn_insns);
      return sgn_ret;
  }
  return mtcs_expmed_expand_divmod/*!expand_divmod*/(mtcsExpmed,mod_p, code, mode, op0, op1, target, unsignedp);
}

/* Expand CODE with arguments INNER & (1<<BITNUM) and 0 that represents
   a single bit equality/inequality test, returns where the result is located.  */
//原型 expand_single_bit_test expr.cc
static rtx expand_single_bit_test (MtcsExpr *self,location_t loc, enum tree_code code,
            tree inner, int bitnum,tree result_type, rtx target,machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  gcc_assert (code == NE_EXPR || code == EQ_EXPR);
  tree type = TREE_TYPE (inner);
  scalar_int_mode operand_mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
  int ops_unsigned;
  tree signed_type, unsigned_type, intermediate_type;
  gimple *inner_def;
  /* First, see if we can fold the single bit test into a sign-bit
     test.  */
  if (bitnum == TYPE_PRECISION (type) - 1 && mtcs_tree_type_has_mode_precision_p/*!type_has_mode_precision_p*/(mtcsTree,type)){
      tree stype = signed_type_for (type);
      tree tmp = mtcs_const_fold_build2_loc/*!fold_build2_loc*/(mtcsConst,loc, code == EQ_EXPR ? GE_EXPR : LT_EXPR,
                  result_type,
                  mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, stype, inner),
                  mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,stype, 0));
      return mtcs_expr_expand_expr/*!expand_expr*/(self,tmp, target, VOIDmode, EXPAND_NORMAL);
  }
  /* Otherwise we have (A & C) != 0 where C is a single bit,
     convert that into ((A >> C2) & 1).  Where C2 = log2(C).
     Similarly for (A & C) == 0.  */
  /* If INNER is a right shift of a constant and it plus BITNUM does
     not overflow, adjust BITNUM and INNER.  */
  if ((inner_def = get_def_for_expr (inner, RSHIFT_EXPR))
       && TREE_CODE (gimple_assign_rhs2 (inner_def)) == INTEGER_CST
       && bitnum < TYPE_PRECISION (type)
       && wi::ltu_p (wi::to_wide (gimple_assign_rhs2 (inner_def)),
             TYPE_PRECISION (type) - bitnum)){
      bitnum += tree_to_uhwi (gimple_assign_rhs2 (inner_def));
      inner = gimple_assign_rhs1 (inner_def);
  }
  /* If we are going to be able to omit the AND below, we must do our
     operations as unsigned.  If we must use the AND, we have a choice.
     Normally unsigned is faster, but for some machines signed is.  */
  ops_unsigned = (mtcs_rtl_load_extend_op/*!load_extend_op */(mtcsRTL,operand_mode) == SIGN_EXTEND
          && !opts->x_flag_syntax_only) ? 0 : 1;
  signed_type = lang_hooks.types.type_for_mode (operand_mode, 0);
  unsigned_type = lang_hooks.types.type_for_mode (operand_mode, 1);
  intermediate_type = ops_unsigned ? unsigned_type : signed_type;
  inner = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, intermediate_type, inner);
  rtx inner0 = mtcs_expr_expand_expr/*!expand_expr*/(self,inner, NULL_RTX, VOIDmode, EXPAND_NORMAL);
  if (CONST_SCALAR_INT_P (inner0)){
      wide_int t = mtcs_rtx_mode_t/*!rtx_mode_t*/(inner0, operand_mode);
      bool setp = (wi::lrshift (t, bitnum) & 1) != 0;
      return (setp ^ (code == EQ_EXPR)) ? const1_rtx : const0_rtx;
  }
  int bitpos = bitnum;
  if (BYTES_BIG_ENDIAN)
    bitpos = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,operand_mode) - 1 - bitpos;

  inner0 = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,inner0, 1, bitpos, 1, target,
                  operand_mode, mode, 0, NULL);

  if (code == EQ_EXPR)
    inner0 = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,GET_MODE (inner0), xor_optab, inner0, const1_rtx,
               NULL_RTX, 1, OPTAB_LIB_WIDEN);
  if (GET_MODE (inner0) != mode){
      rtx t = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      mtcs_expr_convert_move/*!convert_move*/(self,t, inner0, 0);
      return t;
  }
  return inner0;
}


/* Generate code to calculate OPS, and exploded expression
   using a store-flag instruction and return an rtx for the result.
   OPS reflects a comparison.

   If TARGET is nonzero, store the result there if convenient.

   Return zero if there is no suitable set-flag instruction
   available on this machine.

   Once expand_expr has been called on the arguments of the comparison,
   we are committed to doing the store flag, since it is not safe to
   re-evaluate the expression.  We emit the store-flag insn by calling
   emit_store_flag, but only expand the arguments if we have a reason
   to believe that emit_store_flag will be successful.  If we think that
   it will, but it isn't, we have to simulate the store-flag with a
   set/jump/set sequence.  */
//原型 do_store_flag expr.cc
static rtx do_store_flag (MtcsExpr *self,sepops ops, rtx target, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  enum rtx_code code;
  tree arg0, arg1, type;
  machine_mode operand_mode;
  int unsignedp;
  rtx op0, op1;
  rtx subtarget = target;
  location_t loc = ops->location;
  unsigned HOST_WIDE_INT nunits;

  arg0 = ops->op0;
  arg1 = ops->op1;
  /* Don't crash if the comparison was erroneous.  */
  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  type = TREE_TYPE (arg0);
  operand_mode = TYPE_MODE (type);
  unsignedp = TYPE_UNSIGNED (type);
  /* We won't bother with BLKmode store-flag operations because it would mean
     passing a lot of information to emit_store_flag.  */
  if (operand_mode == mtcsMode->modes.M_BLKmode)
    return 0;
  /* We won't bother with store-flag operations involving function pointers
     when function pointers must be canonicalized before comparisons.  */
  if (target_rtx_have_canonicalize_funcptr_for_compare/*!targetm.have_canonicalize_funcptr_for_compare*/(mtcsMachine->tmrtx)
      && ((POINTER_TYPE_P (TREE_TYPE (arg0))
       && FUNC_OR_METHOD_TYPE_P (TREE_TYPE (TREE_TYPE (arg0))))
      || (POINTER_TYPE_P (TREE_TYPE (arg1))
          && FUNC_OR_METHOD_TYPE_P (TREE_TYPE (TREE_TYPE (arg1))))))
    return 0;

  STRIP_NOPS (arg0);
  STRIP_NOPS (arg1);
  /* For vector typed comparisons emit code to generate the desired
     all-ones or all-zeros mask.  */
  if (VECTOR_TYPE_P (ops->type)){
      tree ifexp = build2 (ops->code, ops->type, arg0, arg1);
      if (VECTOR_BOOLEAN_TYPE_P (ops->type)  && expand_vec_cmp_expr_p (TREE_TYPE (arg0), ops->type, ops->code))
          return mtcs_optabs_expand_vec_cmp_expr/*!expand_vec_cmp_expr*/(mtcsOptabs,ops->type, ifexp, target);
      else
          gcc_unreachable ();
  }
  /* Optimize (x % C1) == C2 or (x % C1) != C2 if it is beneficial
     into (x - C2) * C3 < C4.  */
  if ((ops->code == EQ_EXPR || ops->code == NE_EXPR)
      && TREE_CODE (arg0) == SSA_NAME   && TREE_CODE (arg1) == INTEGER_CST){
      enum tree_code new_code = mtcs_expr_maybe_optimize_mod_cmp/*!maybe_optimize_mod_cmp*/(self,ops->code,&arg0, &arg1);
      if (new_code != ops->code){
          struct separate_ops nops = *ops;
          nops.code = ops->code = new_code;
          nops.op0 = arg0;
          nops.op1 = arg1;
          nops.type = TREE_TYPE (arg0);
          return do_store_flag (self,&nops, target, mode);
      }
  }
  /* Optimize (x - y) < 0 into x < y if x - y has undefined overflow.  */
  if (!unsignedp  && (ops->code == LT_EXPR || ops->code == LE_EXPR
      || ops->code == GT_EXPR || ops->code == GE_EXPR)
      && integer_zerop (arg1) && TREE_CODE (arg0) == SSA_NAME)
    maybe_optimize_sub_cmp_0 (ops->code, &arg0, &arg1);
  /* Get the rtx comparison code to use.  We know that EXP is a comparison
     operation of some type.  Some comparisons against 1 and -1 can be
     converted to comparisons with zero.  Do so here so that the tests
     below will be aware that we have a comparison with zero.   These
     tests will not catch constants in the first operand, but constants
     are rarely passed as the first operand.  */
  switch (ops->code){
    case EQ_EXPR:
      code = EQ;
      break;
    case NE_EXPR:
      code = NE;
      break;
    case LT_EXPR:
      if (integer_onep (arg1))
          arg1 = integer_zero_node, code = unsignedp ? LEU : LE;
      else
          code = unsignedp ? LTU : LT;
      break;
    case LE_EXPR:
      if (! unsignedp && integer_all_onesp (arg1))
          arg1 = integer_zero_node, code = LT;
      else
          code = unsignedp ? LEU : LE;
      break;
    case GT_EXPR:
      if (! unsignedp && integer_all_onesp (arg1))
          arg1 = integer_zero_node, code = GE;
      else
          code = unsignedp ? GTU : GT;
      break;
    case GE_EXPR:
      if (integer_onep (arg1))
          arg1 = integer_zero_node, code = unsignedp ? GTU : GT;
      else
          code = unsignedp ? GEU : GE;
      break;

    case UNORDERED_EXPR:
      code = UNORDERED;
      break;
    case ORDERED_EXPR:
      code = ORDERED;
      break;
    case UNLT_EXPR:
      code = UNLT;
      break;
    case UNLE_EXPR:
      code = UNLE;
      break;
    case UNGT_EXPR:
      code = UNGT;
      break;
    case UNGE_EXPR:
      code = UNGE;
      break;
    case UNEQ_EXPR:
      code = UNEQ;
      break;
    case LTGT_EXPR:
      code = LTGT;
      break;
    default:
      gcc_unreachable ();
  }

  /* Put a constant second.  */
  if (TREE_CODE (arg0) == REAL_CST || TREE_CODE (arg0) == INTEGER_CST
      || TREE_CODE (arg0) == FIXED_CST){
      std::swap (arg0, arg1);
      code = swap_condition (code);
  }
  /* If this is an equality or inequality test of a single bit, we can
     do this by shifting the bit being tested to the low-order bit and
     masking the result with the constant 1.  If the condition was EQ,
     we xor it with 1.  This does not require an scc insn and is faster
     than an scc insn even if we have it.  */
  if ((code == NE || code == EQ)   && (integer_zerop (arg1)
      || integer_pow2p (arg1))
      /* vector types are not handled here. */
      && TREE_CODE (TREE_TYPE (arg1)) != VECTOR_TYPE
      && (TYPE_PRECISION (ops->type) != 1 || TYPE_UNSIGNED (ops->type))){
      tree narg0 = arg0;
      wide_int nz = tree_nonzero_bits (narg0);
      gimple *srcstmt = get_def_for_expr (narg0, BIT_AND_EXPR);
      /* If the defining statement was (x & POW2), then use that instead of
      the non-zero bits.  */
      if (srcstmt && integer_pow2p (gimple_assign_rhs2 (srcstmt))){
          nz = wi::to_wide (gimple_assign_rhs2 (srcstmt));
          narg0 = gimple_assign_rhs1 (srcstmt);
      }
      if (wi::popcount (nz) == 1 && (integer_zerop (arg1) || wi::to_wide (arg1) == nz)){
          int bitnum = wi::exact_log2 (nz);
          enum tree_code tcode = EQ_EXPR;
          if ((code == NE) ^ !integer_zerop (arg1))
            tcode = NE_EXPR;

          type = lang_hooks.types.type_for_mode (mode, unsignedp);
          return expand_single_bit_test(self,loc, tcode, narg0, bitnum, type, target, mode);
      }
  }
  if (! get_subtarget(self,target) || GET_MODE (subtarget) != operand_mode)
    subtarget = 0;

  mtcs_expr_expand_operands/*!expand_operands*/(self,arg0, arg1, subtarget, &op0, &op1, EXPAND_NORMAL);
  /* For boolean vectors with less than mode precision
     make sure to fill padding with consistent values.  */
  if (VECTOR_BOOLEAN_TYPE_P (type)
      && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,operand_mode)
      && TYPE_VECTOR_SUBPARTS (type).is_constant (&nunits)
      && maybe_ne (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,operand_mode), nunits))
    {
      gcc_assert (code == EQ || code == NE);
      op0 =mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, and_optab, op0,
              mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_1U << nunits) - 1),
              NULL_RTX, true, OPTAB_WIDEN);
      op1 = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, and_optab, op1,
              mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,(HOST_WIDE_INT_1U << nunits) - 1),
              NULL_RTX, true, OPTAB_WIDEN);
    }

  if (target == 0)
    target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

  /* Try a cstore if possible.  */
  return mtcs_expmed_emit_store_flag_force/*!emit_store_flag_force*/(mtcsExpmed,target, code, op0, op1,
                operand_mode, unsignedp,(TYPE_PRECISION (ops->type) == 1  && !TYPE_UNSIGNED (ops->type)) ? -1 : 1);
}

/* Return the defining gimple statement for SSA_NAME NAME if it is an
   assigment and the class of the expresion on the RHS is CLASS.  Return
   NULL otherwise.  */
//原型 get_def_for_expr_class expr.cc
static gimple *get_def_for_expr_class (tree name, enum tree_code_class tclass)
{
  gimple *def_stmt;
  if (TREE_CODE (name) != SSA_NAME)
    return NULL;
  def_stmt = get_gimple_for_ssa_name (name);
  if (!def_stmt || TREE_CODE_CLASS (gimple_assign_rhs_code (def_stmt)) != tclass)
    return NULL;
  return def_stmt;
}

/* Convert the tree comparison code TCODE to the rtl one where the
   signedness is UNSIGNEDP.  */
//原型 convert_tree_comp_to_rtx expr.cc
static enum rtx_code convert_tree_comp_to_rtx (enum tree_code tcode, int unsignedp)
{
  enum rtx_code code;
  switch (tcode)
    {
    case EQ_EXPR:
      code = EQ;
      break;
    case NE_EXPR:
      code = NE;
      break;
    case LT_EXPR:
      code = unsignedp ? LTU : LT;
      break;
    case LE_EXPR:
      code = unsignedp ? LEU : LE;
      break;
    case GT_EXPR:
      code = unsignedp ? GTU : GT;
      break;
    case GE_EXPR:
      code = unsignedp ? GEU : GE;
      break;
    case UNORDERED_EXPR:
      code = UNORDERED;
      break;
    case ORDERED_EXPR:
      code = ORDERED;
      break;
    case UNLT_EXPR:
      code = UNLT;
      break;
    case UNLE_EXPR:
      code = UNLE;
      break;
    case UNGT_EXPR:
      code = UNGT;
      break;
    case UNGE_EXPR:
      code = UNGE;
      break;
    case UNEQ_EXPR:
      code = UNEQ;
      break;
    case LTGT_EXPR:
      code = LTGT;
      break;

    default:
      gcc_unreachable ();
    }
  return code;
}


/* Try to expand the conditional expression which is represented by
   TREEOP0 ? TREEOP1 : TREEOP2 using conditonal moves.  If it succeeds
   return the rtl reg which represents the result.  Otherwise return
   NULL_RTX.  */
//原型 expand_cond_expr_using_cmove expr.cc
static rtx expand_cond_expr_using_cmove (MtcsExpr *self,tree treeop0 ATTRIBUTE_UNUSED,
                  tree treeop1 ATTRIBUTE_UNUSED,
                  tree treeop2 ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx insn;
  rtx op00, op01, op1, op2;
  enum rtx_code comparison_code;
  machine_mode comparison_mode;
  gimple *srcstmt;
  rtx temp;
  tree type = TREE_TYPE (treeop1);
  int unsignedp = TYPE_UNSIGNED (type);
  machine_mode mode = TYPE_MODE (type);
  machine_mode orig_mode = mode;
  static bool expanding_cond_expr_using_cmove = false;
  /* Conditional move expansion can end up TERing two operands which,
     when recursively hitting conditional expressions can result in
     exponential behavior if the cmove expansion ultimatively fails.
     It's hardly profitable to TER a cmove into a cmove so avoid doing
     that by failing early if we end up recursing.  */
  if (expanding_cond_expr_using_cmove)
    return NULL_RTX;
  /* If we cannot do a conditional move on the mode, try doing it
     with the promoted mode. */
  if (!mtcs_optabs_can_conditionally_move_p/*!can_conditionally_move_p*/(mtcsOptabs,mode)){
      mode = mtcs_mode_promote_mode/*!promote_mode*/(mtcsMode,type, mode, &unsignedp);
      if (!mtcs_optabs_can_conditionally_move_p/*!can_conditionally_move_p*/(mtcsOptabs,mode))
          return NULL_RTX;
      temp = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 0, 0); /* Use promoted mode for temp.  */
  }else
    temp = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 0, 1);

  expanding_cond_expr_using_cmove = true;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
  mtcs_expr_expand_operands/*!expand_operands*/(self,treeop1, treeop2,
           mode == orig_mode ? temp : NULL_RTX, &op1, &op2,EXPAND_NORMAL);
  if (TREE_CODE (treeop0) == SSA_NAME
      && (srcstmt = get_def_for_expr_class (treeop0, tcc_comparison))){
      type = TREE_TYPE (gimple_assign_rhs1 (srcstmt));
      enum tree_code cmpcode = gimple_assign_rhs_code (srcstmt);
      op00 = mtcs_expr_expand_normal/*!expand_normal*/(self,gimple_assign_rhs1 (srcstmt));
      op01 = mtcs_expr_expand_normal/*!expand_normal*/(self,gimple_assign_rhs2 (srcstmt));
      comparison_mode = TYPE_MODE (type);
      unsignedp = TYPE_UNSIGNED (type);
      comparison_code = convert_tree_comp_to_rtx (cmpcode, unsignedp);
  }else if (COMPARISON_CLASS_P (treeop0)){
      type = TREE_TYPE (TREE_OPERAND (treeop0, 0));
      enum tree_code cmpcode = TREE_CODE (treeop0);
      op00 = mtcs_expr_expand_normal/*!expand_normal*/(self,TREE_OPERAND (treeop0, 0));
      op01 = mtcs_expr_expand_normal/*!expand_normal*/(self,TREE_OPERAND (treeop0, 1));
      unsignedp = TYPE_UNSIGNED (type);
      comparison_mode = TYPE_MODE (type);
      comparison_code = convert_tree_comp_to_rtx (cmpcode, unsignedp);
  }else{
      op00 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
      op01 = const0_rtx;
      comparison_code = NE;
      comparison_mode = GET_MODE (op00);
      if (comparison_mode == VOIDmode)
          comparison_mode = TYPE_MODE (TREE_TYPE (treeop0));
  }
  expanding_cond_expr_using_cmove = false;
  if (GET_MODE (op1) != mode)
    op1 = gen_lowpart (mode, op1);
  if (GET_MODE (op2) != mode)
    op2 = gen_lowpart (mode, op2);
  /* Try to emit the conditional move.  */
  insn =  mtcs_optabs_emit_conditional_move/*!emit_conditional_move*/(mtcsOptabs,temp,
                { comparison_code, op00, op01,comparison_mode },
                op1, op2, mode, unsignedp);
  /* If we could do the conditional move, emit the sequence,
     and return.  */
  if (insn){
      rtx_insn *seq =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);
      return mtcs_expr_convert_modes/*!convert_modes*/(self,orig_mode, mode, temp, 0);
  }
  /* Otherwise discard the sequence and fall back to code with
     branches.  */
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  return NULL_RTX;
}

//原型 expand_expr_real_2 expr.h expr.cc
rtx mtcs_expr_expand_expr_real_2 (MtcsExpr *self,sepops ops, rtx target, machine_mode tmode,enum expand_modifier modifier)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   rtx op0, op1, op2, temp;
   rtx_code_label *lab;
   tree type;
   int unsignedp;
   machine_mode mode;
   scalar_int_mode int_mode;
   enum tree_code code = ops->code;
   optab this_optab;
   rtx subtarget, original_target;
   int ignore;
   bool reduce_bit_field;
   location_t loc = ops->location;
   tree treeop0, treeop1, treeop2;
#define REDUCE_BIT_FIELD(expr)  (reduce_bit_field             \
      ? reduce_to_bit_field_precision ((self),(expr), \
            target, \
            type)   \
            : (expr))

   type = ops->type;
   mode = TYPE_MODE (type);
   unsignedp = TYPE_UNSIGNED (type);
   n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 00 mode:%d target:%p code:%s\n",mode,target,get_tree_code_name(code));
   aet_print_tree(type);
   treeop0 = ops->op0;
   treeop1 = ops->op1;
   treeop2 = ops->op2;

   /* We should be called only on simple (binary or unary) expressions,
   exactly those that are valid in gimple expressions that aren't
   GIMPLE_SINGLE_RHS (or invalid).  */
   gcc_assert (get_gimple_rhs_class (code) == GIMPLE_UNARY_RHS
      || get_gimple_rhs_class (code) == GIMPLE_BINARY_RHS
      || get_gimple_rhs_class (code) == GIMPLE_TERNARY_RHS);

   ignore = (target == const0_rtx
      || ((CONVERT_EXPR_CODE_P (code)
      || code == COND_EXPR || code == VIEW_CONVERT_EXPR)
      && TREE_CODE (type) == VOID_TYPE));

   /* We should be called only if we need the result.  */
   gcc_assert (!ignore);

   /* An operation in what may be a bit-field type needs the
   result to be reduced to the precision of the bit-field type,
   which is narrower than that of the type's mode.  */
   reduce_bit_field = (INTEGRAL_TYPE_P (type)  && !mtcs_tree_type_has_mode_precision_p/*!type_has_mode_precision_p*/(mtcsTree,type));

   if (reduce_bit_field  && (modifier == EXPAND_STACK_PARM || (target && GET_MODE (target) != mode)))
      target = 0;
   n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 11 mode:%d target:%p reduce_bit_field:%d\n",mode,target,reduce_bit_field);

   /* Use subtarget as the target for operand 0 of a binary operation.  */
   subtarget = get_subtarget(self,target);
   original_target = target;
   n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 22 mode:%d target:%p subtarget:%p\n",mode,target,subtarget);

   switch (code){
      case NON_LVALUE_EXPR:
      case PAREN_EXPR:
      CASE_CONVERT:
         if (treeop0 == error_mark_node)
            return const0_rtx;

         if (TREE_CODE (type) == UNION_TYPE){
            tree valtype = TREE_TYPE (treeop0);

            /* If both input and output are BLKmode, this conversion isn't doing
            anything except possibly changing memory attribute.  */
            if (mode == mtcsMode->modes.M_BLKmode
            && TYPE_MODE (valtype) == mtcsMode->modes.M_BLKmode){
               rtx result = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0,target, tmode,modifier);
               result = copy_rtx (result);
               mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,result, type, 0);
               return result;
            }

            if (target == 0){
               machine_mode mode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
               if (mode/*!TYPE_MODE (type)*/ != mtcsMode->modes.M_BLKmode)
                  target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode/*!TYPE_MODE (type)*/);
               else
                  target = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 1, 1);
            }

            if (MEM_P (target))
               /* Store data into beginning of memory target.  */
               mtcs_expr_store_expr/*!store_expr*/(self,treeop0,
                     mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,target, TYPE_MODE (valtype), 0),
                     modifier == EXPAND_STACK_PARM, false, TYPE_REVERSE_STORAGE_ORDER (type));
            else{
               gcc_assert (REG_P (target) && !TYPE_REVERSE_STORAGE_ORDER (type));

               /* Store this field into a union of the proper type.  */
               poly_uint64 op0_size  = tree_to_poly_uint64 (TYPE_SIZE (TREE_TYPE (treeop0)));
               poly_uint64 union_size = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode);
               store_field(self,target,
                  /* The conversion must be constructed so that
                  we know at compile time how many bits
                  to preserve.  */
                  ordered_min (op0_size, union_size), 0, 0, 0, TYPE_MODE (valtype), treeop0, 0,false, false);
            }
            /* Return the entire union.  */
            return target;
         }

         if (mode == TYPE_MODE (TREE_TYPE (treeop0))){
            op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, target, VOIDmode,modifier);
            return REDUCE_BIT_FIELD (op0);
         }
         n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 33 mode:%d modifier:%d EXPAND_SUM:%d\n",mode,modifier,EXPAND_SUM);

         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, NULL_RTX, mode, modifier == EXPAND_SUM ? EXPAND_NORMAL : modifier);
         n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 44 mode:%d %d reduce_bit_field:%d\n",mode,GET_MODE (op0),reduce_bit_field);

         if (GET_MODE (op0) == mode){
           // ;
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 55 mode:%d %d\n",mode,GET_MODE (op0));

         /* If OP0 is a constant, just convert it into the proper mode.  */
         }else if (CONSTANT_P (op0)){
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 66 mode:%d %d\n",mode,GET_MODE (op0));

            tree inner_type = TREE_TYPE (treeop0);
            machine_mode inner_mode = GET_MODE (op0);
            if (inner_mode == VOIDmode)
               inner_mode = TYPE_MODE (inner_type);

            if (modifier == EXPAND_INITIALIZER)
               op0 = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,mode, op0, inner_mode);
            else
               op0= mtcs_expr_convert_modes/*!convert_modes*/(self,mode, inner_mode, op0,TYPE_UNSIGNED (inner_type));
         }else if (modifier == EXPAND_INITIALIZER){
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 77 mode:%d %d\n",mode,GET_MODE (op0));

            op0 = gen_rtx_fmt_e (TYPE_UNSIGNED (TREE_TYPE (treeop0)) ? ZERO_EXTEND : SIGN_EXTEND, mode, op0);
         }else if (target == 0){
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 88 mode:%d %d\n",mode,GET_MODE (op0));

            op0 = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,mode, op0,TYPE_UNSIGNED (TREE_TYPE(treeop0)));
         }else{
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 99 mode:%d %d\n",mode,GET_MODE (op0));

            mtcs_expr_convert_move/*!convert_move*/(self,target, op0,TYPE_UNSIGNED (TREE_TYPE (treeop0)));
            op0 = target;
            n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 99aa mode:%d %d\n",mode,GET_MODE (op0));
             mtcs_print_rtl_single(stderr,op0);
         }
         n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 100 mode:%d %d\n",mode,GET_MODE (op0));

         return REDUCE_BIT_FIELD (op0);

      case ADDR_SPACE_CONVERT_EXPR:
      {
         tree treeop0_type = TREE_TYPE (treeop0);
         gcc_assert (POINTER_TYPE_P (type));
         gcc_assert (POINTER_TYPE_P (treeop0_type));
         addr_space_t as_to = TYPE_ADDR_SPACE (TREE_TYPE (type));
         addr_space_t as_from = TYPE_ADDR_SPACE (TREE_TYPE (treeop0_type));
         /* Conversions beween pointers to the same address space should
         have been implemented via CONVERT_EXPR / NOP_EXPR.  */
         gcc_assert (as_to != as_from);
         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, NULL_RTX, VOIDmode, modifier);

         /* Ask target code to handle conversion between pointers
         to overlapping address spaces.  */
         if (target_addr_space_subset_p/*!targetm.addr_space.subset_p*/(mtcsMachine->addrSpace,as_to, as_from)
         || target_addr_space_subset_p/*!targetm.addr_space.subset_p*/(mtcsMachine->addrSpace,as_from, as_to)){
            op0 = target_addr_space_convert/*!targetm.addr_space.convert*/(mtcsMachine->addrSpace,op0, treeop0_type, type);
         }else{
            /* For disjoint address spaces, converting anything but a null
            pointer invokes undefined behavior.  We truncate or extend the
            value as if we'd converted via integers, which handles 0 as
            required, and all others as the programmer likely expects.  */
            /*
            #ifndef POINTERS_EXTEND_UNSIGNED //host=1 nvptx=0
            const int POINTERS_EXTEND_UNSIGNED = 1;
            #endif
            */
            op0 = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, TYPE_MODE (treeop0_type),
                  op0, self->pointersExtendUnsigned/*!POINTERS_EXTEND_UNSIGNED*/);
         }
         gcc_assert (op0);
         return op0;
      }

      case POINTER_PLUS_EXPR:
         /* Even though the sizetype mode and the pointer's mode can be different
         expand is able to handle this correctly and get the correct result out
         of the PLUS_EXPR code.  */
         /* Make sure to sign-extend the sizetype offset in a POINTER_PLUS_EXPR
         if sizetype precision is smaller than pointer precision.  */
         if (TYPE_PRECISION (sizetype) < TYPE_PRECISION (type))
            treeop1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,
                  loc, type,mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, ssizetype,treeop1));
         /* If sizetype precision is larger than pointer precision, truncate the
         offset to have matching modes.  */
         else if (TYPE_PRECISION (sizetype) > TYPE_PRECISION (type))
            treeop1 = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, type, treeop1);
         /* FALLTHRU */

      case PLUS_EXPR:
         /* If we are adding a constant, a VAR_DECL that is sp, fp, or ap, and
         something else, make sure we add the register to the constant and
         then to the other thing.  This case can occur during strength
         reduction and doing it this way will produce better code if the
         frame pointer or argument pointer is eliminated.

         fold-const.cc will ensure that the constant is always in the inner
         PLUS_EXPR, so the only case we need to do anything about is if
         sp, ap, or fp is our second argument, in which case we must swap
         the innermost first argument and our second argument.  */

         if (TREE_CODE (treeop0) == PLUS_EXPR
         && TREE_CODE (TREE_OPERAND (treeop0, 1)) == INTEGER_CST
         && VAR_P (treeop1)
         && (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,treeop1) == mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)
         || mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,treeop1) == mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)
         || mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,treeop1) == mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL))){
            gcc_unreachable ();
         }

         /* If the result is to be ptr_mode and we are adding an integer to
         something, we might be forming a constant.  So try to use
         plus_constant.  If it produces a sum and we can't accept it,
         use force_operand.  This allows P = &ARR[const] to generate
         efficient code on machines where a SYMBOL_REF is not a valid
         address.

         If this is an EXPAND_SUM call, always return the sum.  */
         if (modifier == EXPAND_SUM || modifier == EXPAND_INITIALIZER
         || (mode == ptr_mode && (unsignedp || ! mtcsOptionsItem->x_flag_trapv))){
            if (modifier == EXPAND_STACK_PARM)
               target = 0;
            if (TREE_CODE (treeop0) == INTEGER_CST
            && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,mode)
            && TREE_CONSTANT (treeop1)){
               rtx constant_part;
               HOST_WIDE_INT wc;
               machine_mode wmode = TYPE_MODE (TREE_TYPE (treeop1));

               op1 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop1, subtarget, VOIDmode,EXPAND_SUM);
               /* Use wi::shwi to ensure that the constant is
               truncated according to the mode of OP1, then sign extended
               to a HOST_WIDE_INT.  Using the constant directly can result
               in non-canonical RTL in a 64x32 cross compile.  */
               wc = TREE_INT_CST_LOW (treeop0);
               constant_part = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::shwi (wc, wmode), wmode);
               op1 = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,mode, op1, INTVAL (constant_part));
               if (modifier != EXPAND_SUM && modifier != EXPAND_INITIALIZER)
                  op1 = mtcs_expr_force_operand/*!force_operand*/(self,op1, target);
               return REDUCE_BIT_FIELD (op1);
            }else if (TREE_CODE (treeop1) == INTEGER_CST
            && mtcs_mode_is_hwi_computable_p/*!HWI_COMPUTABLE_MODE_P*/(mtcsMode,mode)
            && TREE_CONSTANT (treeop0)){
               rtx constant_part;
               HOST_WIDE_INT wc;
               machine_mode wmode = TYPE_MODE (TREE_TYPE (treeop0));

               op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, subtarget, VOIDmode,
               (modifier == EXPAND_INITIALIZER ? EXPAND_INITIALIZER : EXPAND_SUM));
               if (! CONSTANT_P (op0)){
                  op1 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop1, NULL_RTX,VOIDmode, modifier);
                  /* Return a PLUS if modifier says it's OK.  */
                  if (modifier == EXPAND_SUM  || modifier == EXPAND_INITIALIZER)
                     return  mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, op0, op1);
                  goto binop2;
               }
               /* Use wi::shwi to ensure that the constant is
               truncated according to the mode of OP1, then sign extended
               to a HOST_WIDE_INT.  Using the constant directly can result
               in non-canonical RTL in a 64x32 cross compile.  */
               wc = TREE_INT_CST_LOW (treeop1);
               constant_part  = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,wi::shwi (wc, wmode), wmode);
               op0 = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,mode, op0, INTVAL (constant_part));
               if (modifier != EXPAND_SUM && modifier != EXPAND_INITIALIZER)
                  op0 = mtcs_expr_force_operand/*!force_operand*/(self,op0, target);
               return REDUCE_BIT_FIELD (op0);
            }
         }

         /* Use TER to expand pointer addition of a negated value
         as pointer subtraction.  */
         if ((POINTER_TYPE_P (TREE_TYPE (treeop0))
         || (TREE_CODE (TREE_TYPE (treeop0)) == VECTOR_TYPE
         && POINTER_TYPE_P (TREE_TYPE (TREE_TYPE (treeop0)))))
         && TREE_CODE (treeop1) == SSA_NAME
         && TYPE_MODE (TREE_TYPE (treeop0)) == TYPE_MODE (TREE_TYPE (treeop1))){
            gimple *def = get_def_for_expr (treeop1, NEGATE_EXPR);
            if (def){
               treeop1 = gimple_assign_rhs1 (def);
               code = MINUS_EXPR;
               goto do_minus;
            }
         }

         /* No sense saving up arithmetic to be done
         if it's all in the wrong mode to form part of an address.
         And force_operand won't know whether to sign-extend or
         zero-extend.  */
         if (modifier != EXPAND_INITIALIZER  && (modifier != EXPAND_SUM || mode != ptr_mode)){
            mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,
                  subtarget, &op0, &op1, modifier);
            if (op0 == const0_rtx)
               return op1;
            if (op1 == const0_rtx)
               return op0;
            goto binop2;
         }

         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,
         subtarget, &op0, &op1, modifier);
         return REDUCE_BIT_FIELD (mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, op0, op1));

      case MINUS_EXPR:
      case POINTER_DIFF_EXPR:
   do_minus:
         /* For initializers, we are allowed to return a MINUS of two
         symbolic constants.  Here we handle all cases when both operands
         are constant.  */
         /* Handle difference of two symbolic constants,
         for the sake of an initializer.  */
         if ((modifier == EXPAND_SUM || modifier == EXPAND_INITIALIZER)
         && really_constant_p (treeop0)
         && really_constant_p (treeop1)){
            mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,
            NULL_RTX, &op0, &op1, modifier);
            return mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MINUS, mode, op0, op1);
         }

         /* No sense saving up arithmetic to be done
         if it's all in the wrong mode to form part of an address.
         And force_operand won't know whether to sign-extend or
         zero-extend.  */
         if (modifier != EXPAND_INITIALIZER  && (modifier != EXPAND_SUM || mode != ptr_mode))
            goto binop;

         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,subtarget, &op0, &op1, modifier);

         /* Convert A - const to A + (-const).  */
         if (CONST_INT_P (op1)){
            op1 = mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,mode, op1);
            return REDUCE_BIT_FIELD (mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, mode, op0, op1));
         }

         goto binop2;

      case WIDEN_MULT_PLUS_EXPR:
      case WIDEN_MULT_MINUS_EXPR:
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, NULL_RTX, &op0, &op1, EXPAND_NORMAL);
         op2 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop2);
         target = mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,ops,op0, op1, op2,target, unsignedp);
         return target;

      case WIDEN_MULT_EXPR:
         /* If first operand is constant, swap them.
         Thus the following special case checks need only
         check the second operand.  */
         if (TREE_CODE (treeop0) == INTEGER_CST)
            std::swap (treeop0, treeop1);

         /* First, check if we have a multiplication of one signed and one
         unsigned operand.  */
         if (TREE_CODE (treeop1) != INTEGER_CST
         && (TYPE_UNSIGNED (TREE_TYPE (treeop0)) != TYPE_UNSIGNED (TREE_TYPE (treeop1)))){
            machine_mode innermode = TYPE_MODE (TREE_TYPE (treeop0));
            this_optab = usmul_widen_optab;
            if (mtcs_optabs_find_widening_optab_handler/*!find_widening_optab_handler*/(mtcsOptabs,
                  this_optab, mode, innermode) != CODE_FOR_nothing){
               if (TYPE_UNSIGNED (TREE_TYPE (treeop0)))
                  mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, NULL_RTX, &op0, &op1,EXPAND_NORMAL);
               else
                  mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, NULL_RTX, &op1, &op0,EXPAND_NORMAL);
               /* op0 and op1 might still be constant, despite the above
               != INTEGER_CST check.  Handle it.  */
               if (GET_MODE (op0) == VOIDmode && GET_MODE (op1) == VOIDmode){
                  op0 =  mtcs_expr_convert_modes/*!convert_modes*/(self,mode, innermode, op0, true);
                  op1 =  mtcs_expr_convert_modes/*!convert_modes*/(self,mode, innermode, op1, false);
                  return REDUCE_BIT_FIELD (mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,mode, op0, op1, target, unsignedp));
               }
               goto binop3;
            }
         }
         /* Check for a multiplication with matching signedness.  */
         else if ((TREE_CODE (treeop1) == INTEGER_CST
         && int_fits_type_p (treeop1, TREE_TYPE (treeop0)))
         || (TYPE_UNSIGNED (TREE_TYPE (treeop1)) == TYPE_UNSIGNED (TREE_TYPE (treeop0)))){
            tree op0type = TREE_TYPE (treeop0);
            machine_mode innermode = TYPE_MODE (op0type);
            bool zextend_p = TYPE_UNSIGNED (op0type);
            optab other_optab = zextend_p ? smul_widen_optab : umul_widen_optab;
            this_optab = zextend_p ? umul_widen_optab : smul_widen_optab;

            if (TREE_CODE (treeop0) != INTEGER_CST){
               if (mtcs_optabs_find_widening_optab_handler/*!find_widening_optab_handler*/(mtcsOptabs,
                     this_optab, mode, innermode)!= CODE_FOR_nothing){
                  mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, NULL_RTX, &op0, &op1,EXPAND_NORMAL);
                  /* op0 and op1 might still be constant, despite the above
                  != INTEGER_CST check.  Handle it.  */
                  if (GET_MODE (op0) == VOIDmode && GET_MODE (op1) == VOIDmode){
                     widen_mult_const:
                     op0 = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, innermode, op0, zextend_p);
                     op1 = mtcs_expr_convert_modes/*!convert_modes*/(self,mode, innermode, op1, TYPE_UNSIGNED (TREE_TYPE (treeop1)));
                     return REDUCE_BIT_FIELD (mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,mode, op0, op1,target,unsignedp));
                  }
                  temp = mtcs_expmed_expand_widening_mult/*!expand_widening_mult*/(mtcsExpmed,
                        mode, op0, op1, target, unsignedp, this_optab);
                  return REDUCE_BIT_FIELD (temp);
               }
               if (mtcs_optabs_find_widening_optab_handler/*!find_widening_optab_handler*/(mtcsOptabs,
                     other_optab, mode, innermode) != CODE_FOR_nothing  && innermode == mtcsMode->word_mode){
                  rtx htem, hipart;
                  op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
                  op1 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop1);
                  /* op0 and op1 might be constants, despite the above
                  != INTEGER_CST check.  Handle it.  */
                  if (GET_MODE (op0) == VOIDmode && GET_MODE (op1) == VOIDmode)
                     goto widen_mult_const;
                  temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode,
                        other_optab, op0, op1, target,unsignedp, OPTAB_LIB_WIDEN);
                  hipart = mtcs_rtl_gen_highpart/*!gen_highpart*/(mtcsRTL,word_mode, temp);
                  htem =mtcs_expmed_expand_mult_highpart_adjust/*!expand_mult_highpart_adjust*/(mtcsExpmed,
                        word_mode, hipart, op0, op1, hipart,zextend_p);
                  if (htem != hipart)
                     mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,hipart, htem);
                  return REDUCE_BIT_FIELD (temp);
               }
            }
         }
         treeop0 = fold_build1 (CONVERT_EXPR, type, treeop0);
         treeop1 = fold_build1 (CONVERT_EXPR, type, treeop1);
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, subtarget, &op0, &op1, EXPAND_NORMAL);
         return REDUCE_BIT_FIELD (mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,mode, op0, op1, target, unsignedp));

      case MULT_EXPR:
         /* If this is a fixed-point operation, then we cannot use the code
         below because "expand_mult" doesn't support sat/no-sat fixed-point
         multiplications.   */
         if (mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode))
            goto binop;

         /* If first operand is constant, swap them.
         Thus the following special case checks need only
         check the second operand.  */
         if (TREE_CODE (treeop0) == INTEGER_CST)
            std::swap (treeop0, treeop1);

         /* Attempt to return something suitable for generating an
         indexed address, for machines that support that.  */

         if (modifier == EXPAND_SUM && mode == ptr_mode  && tree_fits_shwi_p (treeop1)){
            tree exp1 = treeop1;
            op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, subtarget, VOIDmode,EXPAND_SUM);

            if (!REG_P (op0))
               op0 = mtcs_expr_force_operand/*!force_operand*/(self,op0, NULL_RTX);
            if (!REG_P (op0))
               op0 = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,mode, op0);

            op1 = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,tree_to_shwi (exp1),TYPE_MODE (TREE_TYPE (exp1)));
            return REDUCE_BIT_FIELD (gen_rtx_MULT (mode, op0, op1));
         }

         if (modifier == EXPAND_STACK_PARM)
            target = 0;

         if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)
               && mtcsOptionsItem->x_optimize >= 2){
            gimple *def_stmt0 = get_def_for_expr (treeop0, TRUNC_DIV_EXPR);
            gimple *def_stmt1 = get_def_for_expr (treeop1, TRUNC_DIV_EXPR);
            if (def_stmt0  && !operand_equal_p (treeop1, gimple_assign_rhs2 (def_stmt0), 0))
               def_stmt0 = NULL;
            if (def_stmt1 && !operand_equal_p (treeop0, gimple_assign_rhs2 (def_stmt1), 0))
               def_stmt1 = NULL;

            if (def_stmt0 || def_stmt1){
               /* X / Y * Y can be expanded as X - X % Y too.
               Choose the cheaper sequence of those two.  */
               if (def_stmt0)
                  treeop0 = gimple_assign_rhs1 (def_stmt0);
               else{
                  treeop1 = treeop0;
                  treeop0 = gimple_assign_rhs1 (def_stmt1);
               }
               mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, subtarget, &op0, &op1,EXPAND_NORMAL);
               bool speed_p = optimize_insn_for_speed_p ();
               mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
               mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
               rtx divmul_ret = expand_expr_divmod(self,TRUNC_DIV_EXPR, mode, treeop0, treeop1,op0, op1, NULL_RTX, unsignedp);
               divmul_ret = mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,mode, divmul_ret,op1, target,unsignedp);
               rtx_insn *divmul_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
               mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
               mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
               rtx modsub_ret = expand_expr_divmod(self,TRUNC_MOD_EXPR, mode, treeop0, treeop1,op0, op1, NULL_RTX, unsignedp);
               this_optab = optab_for_tree_code (MINUS_EXPR, type,optab_default);
               modsub_ret =mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, this_optab, op0, modsub_ret,target, unsignedp, OPTAB_LIB_WIDEN);
               rtx_insn *modsub_insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
               mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
               unsigned divmul_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,divmul_insns, speed_p);
               unsigned modsub_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,modsub_insns, speed_p);
               /* If costs are the same then use as tie breaker the other other
               factor.  */
               if (divmul_cost == modsub_cost){
                  divmul_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,divmul_insns, !speed_p);
                  modsub_cost = mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,modsub_insns, !speed_p);
               }
               if (divmul_cost <= modsub_cost){
                  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,divmul_insns);
                  return REDUCE_BIT_FIELD (divmul_ret);
               }
               mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,modsub_insns);
               return REDUCE_BIT_FIELD (modsub_ret);
            }
         }

         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, subtarget, &op0, &op1, EXPAND_NORMAL);
         /* Expand X*Y as X&-Y when Y must be zero or one.  */
         if (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)){
            bool gimple_zero_one_valued_p (tree, tree (*)(tree));
            bool bit0_p = gimple_zero_one_valued_p (treeop0, nullptr);
            bool bit1_p = gimple_zero_one_valued_p (treeop1, nullptr);

            /* Expand X*Y as X&Y when both X and Y must be zero or one.  */
            if (bit0_p && bit1_p)
               return REDUCE_BIT_FIELD (mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,mode, op0, op1, target));

            if (bit0_p || bit1_p){
               bool speed = optimize_insn_for_speed_p ();
               int cost = mtcs_expmed_add_cost/*!add_cost*/(mtcsExpmed,speed, mode) +
                     mtcs_expmed_neg_cost/*!neg_cost*/(mtcsExpmed,speed, mode);
               struct algorithm algorithm;
               enum mult_variant variant;
               if (CONST_INT_P (op1)
               ? !mtcs_expmed_choose_mult_variant/*!choose_mult_variant*/(mtcsExpmed,mode, INTVAL (op1),
               &algorithm, (int *)&variant, cost)
               : cost < mtcs_expmed_mul_cost/*!mul_cost*/(mtcsExpmed,speed, mode)){
                  temp = bit0_p ? mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,mode,
                        mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,mode, op0),op1, target)
                  : mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,mode, op0,
                        mtcs_expmed_negate_rtx/*!negate_rtx*/(mtcsExpmed,mode, op1),target);
                  return REDUCE_BIT_FIELD (temp);
               }
            }
         }

         return REDUCE_BIT_FIELD (mtcs_expmed_expand_mult/*!expand_mult*/(mtcsExpmed,mode, op0, op1, target, unsignedp));

      case TRUNC_MOD_EXPR:
      case FLOOR_MOD_EXPR:
      case CEIL_MOD_EXPR:
      case ROUND_MOD_EXPR:

      case TRUNC_DIV_EXPR:
      case FLOOR_DIV_EXPR:
      case CEIL_DIV_EXPR:
      case ROUND_DIV_EXPR:
      case EXACT_DIV_EXPR:
         /* If this is a fixed-point operation, then we cannot use the code
         below because "expand_divmod" doesn't support sat/no-sat fixed-point
         divisions.   */
         if (mtcs_mode_is_all_fixed_point_p/*! ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode))
            goto binop;

         if (modifier == EXPAND_STACK_PARM)
            target = 0;
         /* Possible optimization: compute the dividend with EXPAND_SUM
         then if the divisor is constant can optimize the case
         where some terms of the dividend have coeffs divisible by it.  */
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, subtarget, &op0, &op1, EXPAND_NORMAL);
         return expand_expr_divmod(self,code, mode, treeop0, treeop1, op0, op1,target, unsignedp);

      case RDIV_EXPR:
         goto binop;

      case MULT_HIGHPART_EXPR:
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, subtarget, &op0, &op1, EXPAND_NORMAL);
         temp = mtcs_optabs_expand_mult_highpart/*!expand_mult_highpart*/(mtcsOptabs,mode, op0, op1, target, unsignedp);
         gcc_assert (temp);
         return temp;

      case FIXED_CONVERT_EXPR:
         op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         if (target == 0 || modifier == EXPAND_STACK_PARM)
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);

         if ((TREE_CODE (TREE_TYPE (treeop0)) == INTEGER_TYPE
         && TYPE_UNSIGNED (TREE_TYPE (treeop0)))
         || (TREE_CODE (type) == INTEGER_TYPE && TYPE_UNSIGNED (type)))
            mtcs_optabs_expand_fixed_convert/*!expand_fixed_convert*/(mtcsOptabs,target, op0, 1, TYPE_SATURATING (type));
         else
            mtcs_optabs_expand_fixed_convert/*!expand_fixed_convert*/(mtcsOptabs,target, op0, 0, TYPE_SATURATING (type));
         return target;

      case FIX_TRUNC_EXPR:
         n_debug("mtcsexpr.cc expand_expr_real_2 xx FIX_TRUNC_EXPR 调用 optabs.cc中的 expand_fix 处理 target:%p modifier:%d EXPAND_STACK_PARM:%d %d\n",
               target,modifier,(target == 0 || modifier == EXPAND_STACK_PARM),EXPAND_STACK_PARM);
         op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         if (target == 0 || modifier == EXPAND_STACK_PARM)
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         mtcs_optabs_expand_fix/*!expand_fix*/(mtcsOptabs,target, op0, unsignedp);
         n_debug("mtcsexpr.cc expand_expr_real_2 yy FIX_TRUNC_EXPR\n");
         mtcs_print_rtl_single(stderr,target);
         mtcs_print_rtl_single(stderr,op0);
         return target;

      case FLOAT_EXPR:
         op0 =mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         if (target == 0 || modifier == EXPAND_STACK_PARM)
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         /* expand_float can't figure out what to do if FROM has VOIDmode.
         So give it the correct mode.  With -O, cse will optimize this.  */
         n_debug("mtcsexpr.c mtcs_expr_expand_expr_real_2 GET_MODE (op0) == VOIDmode:%d\n",GET_MODE (op0) == VOIDmode);
         if (GET_MODE (op0) == VOIDmode)
            op0 = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,TYPE_MODE (TREE_TYPE (treeop0)),op0);
         mtcs_optabs_expand_float/*!expand_float*/(mtcsOptabs,target, op0,TYPE_UNSIGNED (TREE_TYPE (treeop0)));
         return target;

      case NEGATE_EXPR:
         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, subtarget, VOIDmode, EXPAND_NORMAL);
         if (modifier == EXPAND_STACK_PARM)
            target = 0;
         temp = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode,
         optab_for_tree_code (NEGATE_EXPR, type,optab_default),op0, target, 0);
         gcc_assert (temp);
         return REDUCE_BIT_FIELD (temp);

      case ABS_EXPR:
      case ABSU_EXPR:
         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, subtarget,VOIDmode, EXPAND_NORMAL);
         if (modifier == EXPAND_STACK_PARM)
            target = 0;

         /* ABS_EXPR is not valid for complex arguments.  */
         gcc_assert (mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) != MODE_COMPLEX_INT
               && mtcs_mode_get_class/*GET_MODE_CLASS*/(mtcsMode,mode) != MODE_COMPLEX_FLOAT);

         /* Unsigned abs is simply the operand.  Testing here means we don't
         risk generating incorrect code below.  */
         if (TYPE_UNSIGNED (TREE_TYPE (treeop0)))
            return op0;

         return mtcs_optabs_expand_abs/*!expand_abs*/(mtcsOptabs,mode, op0, target, unsignedp,
               mtcs_expr_safe_from_p/*!safe_from_p*/(self,target, treeop0, 1));

      case MAX_EXPR:
      case MIN_EXPR:
         target = original_target;
         if (target == 0
         || modifier == EXPAND_STACK_PARM
         || (MEM_P (target) && MEM_VOLATILE_P (target))
         || GET_MODE (target) != mode
         || (REG_P (target)
         && REGNO (target) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)))
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,target, &op0, &op1, EXPAND_NORMAL);

         /* First try to do it with a special MIN or MAX instruction.
         If that does not win, use a conditional jump to select the proper
         value.  */
         this_optab = optab_for_tree_code (code, type, optab_default);
         temp =  mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               mode, this_optab, op0, op1, target, unsignedp,OPTAB_WIDEN);
         if (temp != 0)
            return temp;

         if (VECTOR_TYPE_P (type))
            gcc_unreachable ();
         /* At this point, a MEM target is no longer useful; we will get better
         code without it.  */
         if (! REG_P (target))
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         /* If op1 was placed in target, swap op0 and op1.  */
         if (target != op0 && target == op1)
            std::swap (op0, op1);
         /* We generate better code and avoid problems with op1 mentioning
         target by forcing op1 into a pseudo if it isn't a constant.  */
         if (! CONSTANT_P (op1))
            op1 = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, op1);

         {
            enum rtx_code comparison_code;
            rtx cmpop1 = op1;

            if (code == MAX_EXPR)
               comparison_code = unsignedp ? GEU : GE;
            else
               comparison_code = unsignedp ? LEU : LE;

            /* Canonicalize to comparisons against 0.  */
            if (op1 == const1_rtx){
               /* Converting (a >= 1 ? a : 1) into (a > 0 ? a : 1)
               or (a != 0 ? a : 1) for unsigned.
               For MIN we are safe converting (a <= 1 ? a : 1)
               into (a <= 0 ? a : 1)  */
               cmpop1 = const0_rtx;
               if (code == MAX_EXPR)
                  comparison_code = unsignedp ? NE : GT;
            }
            if (op1 == constm1_rtx && !unsignedp){
               /* Converting (a >= -1 ? a : -1) into (a >= 0 ? a : -1)
               and (a <= -1 ? a : -1) into (a < 0 ? a : -1) */
               cmpop1 = const0_rtx;
               if (code == MIN_EXPR)
                  comparison_code = LT;
            }

            /* Use a conditional move if possible.  */
            if (mtcs_optabs_can_conditionally_move_p/*!can_conditionally_move_p*/(mtcsOptabs,mode)){
               rtx insn;
               mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
               /* Try to emit the conditional move.  */
               insn = mtcs_optabs_emit_conditional_move/*!emit_conditional_move*/(mtcsOptabs,
                     target,{ comparison_code,op0, cmpop1, mode },op0, op1, mode,unsignedp);
               /* If we could do the conditional move, emit the sequence,
               and return.  */
               if (insn){
                  rtx_insn *seq =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
                  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
                  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);
                  return target;
               }
               /* Otherwise discard the sequence and fall back to code with
               branches.  */
               mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
            }

            if (target != op0)
               emit_move_insn (target, op0);

            lab = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
            mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,
                  target, cmpop1, comparison_code,unsignedp, mode, NULL_RTX, NULL, lab,profile_probability::uninitialized ());
         }
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, op1);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab);
         return target;

      case BIT_NOT_EXPR:
         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, subtarget,VOIDmode, EXPAND_NORMAL);
         if (modifier == EXPAND_STACK_PARM)
            target = 0;
         /* In case we have to reduce the result to bitfield precision
         for unsigned bitfield expand this as XOR with a proper constant
         instead.  */
         if (reduce_bit_field && TYPE_UNSIGNED (type)){
            int_mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
            wide_int mask = wi::mask (TYPE_PRECISION (type),false, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode));
            temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
                  int_mode, xor_optab, op0,mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/(mtcsRTL,
                        mask, int_mode),target, 1, OPTAB_LIB_WIDEN);
         }else
            temp = mtcs_optabs_expand_unop/*!expand_unop*/(mtcsOptabs,mode, one_cmpl_optab, op0, target, 1);
         gcc_assert (temp);
         return temp;
      /* ??? Can optimize bitwise operations with one arg constant.
      Can optimize (a bitwise1 n) bitwise2 (a bitwise3 b)
      and (a bitwise1 b) bitwise2 b (etc)
      but that is probably not worth while.  */
      case BIT_AND_EXPR:
      case BIT_IOR_EXPR:
      case BIT_XOR_EXPR:
         goto binop;
      case LROTATE_EXPR:
      case RROTATE_EXPR:
         gcc_assert (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,TYPE_MODE (type))
               || mtcs_tree_type_has_mode_precision_p/*!type_has_mode_precision_p*/(mtcsTree,type));
      /* fall through */
      case LSHIFT_EXPR:
      case RSHIFT_EXPR:
      {
         /* If this is a fixed-point operation, then we cannot use the code
         below because "expand_shift" doesn't support sat/no-sat fixed-point
         shifts.  */
         if (mtcs_mode_is_all_fixed_point_p/*! ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode))
            goto binop;

         if (! mtcs_expr_safe_from_p/*!safe_from_p*/(self,subtarget, treeop1, 1))
            subtarget = 0;
         if (modifier == EXPAND_STACK_PARM)
            target = 0;
         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, subtarget,VOIDmode, EXPAND_NORMAL);

         /* Left shift optimization when shifting across word_size boundary.

         If mode == GET_MODE_WIDER_MODE (word_mode), then normally
         there isn't native instruction to support this wide mode
         left shift.  Given below scenario:

         Type A = (Type) B  << C

         |<       T      >|
         | dest_high  |  dest_low |

         | word_size |

         If the shift amount C caused we shift B to across the word
         size boundary, i.e part of B shifted into high half of
         destination register, and part of B remains in the low
         half, then GCC will use the following left shift expand
         logic:

         1. Initialize dest_low to B.
         2. Initialize every bit of dest_high to the sign bit of B.
         3. Logic left shift dest_low by C bit to finalize dest_low.
         The value of dest_low before this shift is kept in a temp D.
         4. Logic left shift dest_high by C.
         5. Logic right shift D by (word_size - C).
         6. Or the result of 4 and 5 to finalize dest_high.

         While, by checking gimple statements, if operand B is
         coming from signed extension, then we can simplify above
         expand logic into:

         1. dest_high = src_low >> (word_size - C).
         2. dest_low = src_low << C.

         We can use one arithmetic right shift to finish all the
         purpose of steps 2, 4, 5, 6, thus we reduce the steps
         needed from 6 into 2.

         The case is similar for zero extension, except that we
         initialize dest_high to zero rather than copies of the sign
         bit from B.  Furthermore, we need to use a logical right shift
         in this case.

         The choice of sign-extension versus zero-extension is
         determined entirely by whether or not B is signed and is
         independent of the current setting of unsignedp.  */

         temp = NULL_RTX;
         if (code == LSHIFT_EXPR
         && target
         && REG_P (target)
         && mtcs_mode_get_2xwider/*!GET_MODE_2XWIDER_MODE*/(mtcsMode,mtcsMode->word_mode).exists (&int_mode)
         && mode == int_mode
         && TREE_CONSTANT (treeop1)
         && TREE_CODE (treeop0) == SSA_NAME){
            gimple *def = SSA_NAME_DEF_STMT (treeop0);
            if (is_gimple_assign (def) && gimple_assign_rhs_code (def) == NOP_EXPR){
               scalar_int_mode rmode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (gimple_assign_rhs1 (def)));

               if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,rmode) <
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode)
               && TREE_INT_CST_LOW (treeop1) < mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->word_mode)
               && ((TREE_INT_CST_LOW (treeop1) + mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,rmode))
               >= mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->word_mode))){
                  rtx_insn *seq, *seq_old;
                  poly_uint64 high_off = mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/(mtcsMode,
                        mtcsMode->word_mode,int_mode);
                  bool extend_unsigned = TYPE_UNSIGNED (TREE_TYPE (gimple_assign_rhs1 (def)));
                  rtx low = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,
                        mtcsMode->word_mode, op0, int_mode);
                  rtx dest_low = mtcs_simplify_rtx_lowpart_subreg/*!lowpart_subreg*/(mtcsSimplifyRtx,
                        mtcsMode->word_mode, target, int_mode);
                  rtx dest_high = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg */(mtcsSimplifyRtx,
                        mtcsMode->word_mode, target,int_mode, high_off);
                  HOST_WIDE_INT ramount = (BITS_PER_WORD- TREE_INT_CST_LOW (treeop1));
                  tree rshift =mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (treeop1), ramount);

                  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
                  /* dest_high = src_low >> (word_size - C).  */
                  temp = mtcs_expmed_expand_variable_shift/*!expand_variable_shift*/(mtcsExpmed,
                        RSHIFT_EXPR, mtcsMode->word_mode, low,rshift, dest_high,extend_unsigned);
                  if (temp != dest_high)
                     mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,dest_high, temp);

                  /* dest_low = src_low << C.  */
                  temp = mtcs_expmed_expand_variable_shift/*!expand_variable_shift*/(mtcsExpmed,
                        LSHIFT_EXPR, mtcsMode->word_mode, low,treeop1, dest_low, unsignedp);
                  if (temp != dest_low)
                     mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,dest_low, temp);

                  seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
                  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
                  temp = target ;

                  if (mtcs_optabs_have_insn_for/*!have_insn_for*/(mtcsOptabs,ASHIFT, int_mode)){
                     bool speed_p = optimize_insn_for_speed_p ();
                     mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
                     rtx ret_old = mtcs_expmed_expand_variable_shift/*!expand_variable_shift*/(mtcsExpmed,
                           code, int_mode,op0, treeop1,target,unsignedp);

                     seq_old = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
                     mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
                     if (mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,seq, speed_p)
                           >= mtcs_rtlanal_seq_cost/*!seq_cost*/(mtcsRtlanal,seq_old, speed_p)){
                        seq = seq_old;
                        temp = ret_old;
                     }
                  }
                  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);
               }
            }
         }

         if (temp == NULL_RTX)
            temp =mtcs_expmed_expand_variable_shift/*!expand_variable_shift*/(mtcsExpmed,
                  code, mode, op0, treeop1, target,unsignedp);
         if (code == LSHIFT_EXPR)
            temp = REDUCE_BIT_FIELD (temp);
         return temp;
      }

      /* Could determine the answer when only additive constants differ.  Also,
      the addition of one can be handled by changing the condition.  */
      case LT_EXPR:
      case LE_EXPR:
      case GT_EXPR:
      case GE_EXPR:
      case EQ_EXPR:
      case NE_EXPR:
      case UNORDERED_EXPR:
      case ORDERED_EXPR:
      case UNLT_EXPR:
      case UNLE_EXPR:
      case UNGT_EXPR:
      case UNGE_EXPR:
      case UNEQ_EXPR:
      case LTGT_EXPR:
      {
         temp = do_store_flag(self,ops, modifier != EXPAND_STACK_PARM ? target : NULL_RTX,
         tmode != VOIDmode ? tmode : mode);
         if (temp)
            return temp;
         /* Use a compare and a jump for BLKmode comparisons, or for function
         type comparisons is have_canonicalize_funcptr_for_compare.  */

         if ((target == 0
         || modifier == EXPAND_STACK_PARM
         || ! mtcs_expr_safe_from_p/*!safe_from_p*/(self,target, treeop0, 1)
         || ! mtcs_expr_safe_from_p/*!safe_from_p*/(self,target, treeop1, 1)
         /* Make sure we don't have a hard reg (such as function's return
         value) live across basic blocks, if not optimizing.  */
         || (!mtcsOptionsItem->x_optimize && REG_P (target)
         && REGNO (target) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))))
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,tmode != VOIDmode ? tmode : mode);

         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const0_rtx);
         rtx_code_label *lab1 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_dojump_jumpifnot_1/*!jumpifnot_1*/(mtcsDojump,
               code, treeop0, treeop1, lab1,profile_probability::uninitialized ());
         if (TYPE_PRECISION (type) == 1 && !TYPE_UNSIGNED (type))
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, constm1_rtx);
         else
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const1_rtx);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab1);
         return target;
      }
      case COMPLEX_EXPR:
      {
         /* Get the rtx code of the operands.  */
         op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         op1 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop1);
         machine_mode mode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));

         if (!target)
            target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode/*!TYPE_MODE (type)*/);
         else
            /* If target overlaps with op1, then either we need to force
            op1 into a pseudo (if target also overlaps with op0),
            or write the complex parts in reverse order.  */
            switch (GET_CODE (target)){
               case CONCAT:
                  if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,XEXP (target, 0), op1)){
                     if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,XEXP (target, 1), op0)){
                        complex_expr_force_op1:
                        temp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,
                        mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (target)));
                        mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,temp, op1);
                        op1 = temp;
                        break;
                     }
                     complex_expr_swap_order:
                     /* Move the imaginary (op1) and real (op0) parts to their
                     location.  */
                     mtcs_expr_write_complex_part/*!write_complex_part*/(self,target, op1, true, true);
                     mtcs_expr_write_complex_part/*!write_complex_part*/(self,target, op0, false, false);
                     return target;
                  }
                  break;
               case MEM:
                  temp = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,target,
                  mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (target)), 0);
                  if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,temp, op1)){
                     scalar_mode imode =mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (target));
                     temp =  mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,target, imode,
                     mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,imode));
                     if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,temp, op0))
                        goto complex_expr_force_op1;
                     goto complex_expr_swap_order;
                  }
                  break;
               default:
                  if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op1)){
                     if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,target, op0))
                        goto complex_expr_force_op1;
                     goto complex_expr_swap_order;
                  }
                  break;
            }
         /* Move the real (op0) and imaginary (op1) parts to their location.  */
         mtcs_expr_write_complex_part/*!write_complex_part*/(self,target, op0, false, true);
         mtcs_expr_write_complex_part/*!write_complex_part*/(self,target, op1, true, false);
         return target;
      }
      case WIDEN_SUM_EXPR:
      {
         tree oprnd0 = treeop0;
         tree oprnd1 = treeop1;
         mtcs_expr_expand_operands/*!expand_operands*/(self,oprnd0, oprnd1, NULL_RTX, &op0, &op1, EXPAND_NORMAL);
         target =mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,
               ops, op0, NULL_RTX, op1, target, unsignedp);
         return target;
      }

      case VEC_UNPACK_HI_EXPR:
      case VEC_UNPACK_LO_EXPR:
      case VEC_UNPACK_FIX_TRUNC_HI_EXPR:
      case VEC_UNPACK_FIX_TRUNC_LO_EXPR:
      {
         op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         temp = mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,
               ops, op0, NULL_RTX, NULL_RTX, target, unsignedp);
         gcc_assert (temp);
         return temp;
      }

      case VEC_UNPACK_FLOAT_HI_EXPR:
      case VEC_UNPACK_FLOAT_LO_EXPR:
      {
         op0 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         /* The signedness is determined from input operand.  */
         temp = mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,
               ops, op0, NULL_RTX, NULL_RTX, target, TYPE_UNSIGNED (TREE_TYPE (treeop0)));
         gcc_assert (temp);
         return temp;
      }

      case VEC_WIDEN_MULT_HI_EXPR:
      case VEC_WIDEN_MULT_LO_EXPR:
      case VEC_WIDEN_MULT_EVEN_EXPR:
      case VEC_WIDEN_MULT_ODD_EXPR:
      case VEC_WIDEN_LSHIFT_HI_EXPR:
      case VEC_WIDEN_LSHIFT_LO_EXPR:
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, NULL_RTX, &op0, &op1, EXPAND_NORMAL);
         target = mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,
               ops, op0, op1, NULL_RTX,target, unsignedp);
         gcc_assert (target);
         return target;

      case VEC_PACK_SAT_EXPR:
      case VEC_PACK_FIX_TRUNC_EXPR:
         mode = TYPE_MODE (TREE_TYPE (treeop0));
         mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (treeop0),mode);
         subtarget = NULL_RTX;
         goto binop;

      case VEC_PACK_TRUNC_EXPR:
         if (VECTOR_BOOLEAN_TYPE_P (type)
         && VECTOR_BOOLEAN_TYPE_P (TREE_TYPE (treeop0))
         && mode == TYPE_MODE (TREE_TYPE (treeop0))
         && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,mode)){
            class expand_operand eops[4];
            machine_mode imode = TYPE_MODE (TREE_TYPE (treeop0));
            mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,
            subtarget, &op0, &op1, EXPAND_NORMAL);
            this_optab = vec_pack_sbool_trunc_optab;
            enum insn_code icode =mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,this_optab, imode);
            create_output_operand (&eops[0], target, mode);
            create_convert_operand_from (&eops[1], op0, imode, false);
            create_convert_operand_from (&eops[2], op1, imode, false);
            temp = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,TYPE_VECTOR_SUBPARTS (type).to_constant ());
            create_input_operand (&eops[3], temp, imode);
            mtcs_optabs_expand_insn/*!expand_insn*/(mtcsOptabs,icode, 4, eops);
            return eops[0].value;
         }
         mode = TYPE_MODE (TREE_TYPE (treeop0));
         subtarget = NULL_RTX;
         goto binop;

      case VEC_PACK_FLOAT_EXPR:
         mode = TYPE_MODE (TREE_TYPE (treeop0));
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,
         subtarget, &op0, &op1, EXPAND_NORMAL);
         this_optab = optab_for_tree_code (code, TREE_TYPE (treeop0),optab_default);
         target = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
               mode, this_optab, op0, op1, target,TYPE_UNSIGNED (TREE_TYPE (treeop0)),OPTAB_LIB_WIDEN);
         gcc_assert (target);
         return target;

      case VEC_PERM_EXPR:
      {
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, target, &op0, &op1, EXPAND_NORMAL);
         vec_perm_builder sel;
         if (TREE_CODE (treeop2) == VECTOR_CST
         && tree_to_vec_perm_builder (&sel, treeop2)){
            machine_mode sel_mode = TYPE_MODE (TREE_TYPE (treeop2));
            temp = mtcs_optabs_expand_vec_perm_const/*!expand_vec_perm_const*/(mtcsOptabs,
                  mode, op0, op1, sel,sel_mode, target);
         }else{
            op2 = mtcs_expr_expand_normal/*!expand_normal*/(self,treeop2);
            temp = mtcs_optabs_expand_vec_perm_var/*!expand_vec_perm_var*/(mtcsOptabs,mode, op0, op1, op2, target);
         }
         gcc_assert (temp);
         return temp;
      }

      case DOT_PROD_EXPR:
      {
         tree oprnd0 = treeop0;
         tree oprnd1 = treeop1;
         tree oprnd2 = treeop2;

         mtcs_expr_expand_operands/*!expand_operands*/(self,oprnd0, oprnd1, NULL_RTX, &op0, &op1, EXPAND_NORMAL);
         op2 =mtcs_expr_expand_normal/*!expand_normal*/(self,oprnd2);
         target = mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,
               ops, op0, op1, op2,target, unsignedp);
         return target;
      }

      case SAD_EXPR:
      {
         tree oprnd0 = treeop0;
         tree oprnd1 = treeop1;
         tree oprnd2 = treeop2;

         mtcs_expr_expand_operands/*!expand_operands*/(self,oprnd0, oprnd1, NULL_RTX, &op0, &op1, EXPAND_NORMAL);
         op2 = mtcs_expr_expand_normal/*!expand_normal*/(self,oprnd2);
         target = mtcs_optabs_expand_widen_pattern_expr/*!expand_widen_pattern_expr*/(mtcsOptabs,
               ops, op0, op1, op2,target, unsignedp);
         return target;
      }

      case REALIGN_LOAD_EXPR:
      {
         tree oprnd0 = treeop0;
         tree oprnd1 = treeop1;
         tree oprnd2 = treeop2;

         this_optab = optab_for_tree_code (code, type, optab_default);
         mtcs_expr_expand_operands/*!expand_operands*/(self,oprnd0, oprnd1, NULL_RTX, &op0, &op1, EXPAND_NORMAL);
         op2 = mtcs_expr_expand_normal/*!expand_normal*/(self,oprnd2);
         temp = mtcs_optabs_expand_ternary_op/*!expand_ternary_op*/(mtcsOptabs,
               mode, this_optab, op0, op1, op2,target, unsignedp);
         gcc_assert (temp);
         return temp;
      }

      case COND_EXPR:
      {
         /* A COND_EXPR with its type being VOID_TYPE represents a
         conditional jump and is handled in
         expand_gimple_cond_expr.  */
         gcc_assert (!VOID_TYPE_P (type));
         /* Note that COND_EXPRs whose type is a structure or union
         are required to be constructed to contain assignments of
         a temporary variable, so that we can evaluate them here
         for side effect only.  If type is void, we must do likewise.  */
         gcc_assert (!TREE_ADDRESSABLE (type)
            && !ignore && TREE_TYPE (treeop1) != void_type_node
            && TREE_TYPE (treeop2) != void_type_node);

         temp = expand_cond_expr_using_cmove(self,treeop0, treeop1, treeop2);
         if (temp)
            return temp;

         /* If we are not to produce a result, we have no target.  Otherwise,
         if a target was specified use it; it will not be used as an
         intermediate target unless it is safe.  If no target, use a
         temporary.  */

         if (modifier != EXPAND_STACK_PARM
         && original_target  && mtcs_expr_safe_from_p/*!safe_from_p*/(self,original_target, treeop0, 1)
         && GET_MODE (original_target) == mode  && !MEM_P (original_target))
            temp = original_target;
         else
            temp = mtcs_func_assign_temp/*!assign_temp*/(mtcsFunc,type, 0, 1);

         mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
         /*!NO_DEFER_POP; expr.h 定义*/
         mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop+=1;
         rtx_code_label *lab0 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         rtx_code_label *lab1 = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_dojump_jumpifnot/*!jumpifnot*/(mtcsDojump,treeop0, lab0,profile_probability::uninitialized ());
         mtcs_expr_store_expr/*!store_expr*/(self,treeop1, temp, modifier == EXPAND_STACK_PARM,false, false);

         mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,
               target_rtx_gen_jump/*!targetm.gen_jump*/(mtcsMachine->tmrtx,lab1));
         mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab0);
         mtcs_expr_store_expr/*!store_expr*/(self,treeop2, temp,modifier == EXPAND_STACK_PARM, false, false);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,lab1);
         /*!OK_DEFER_POP;expr.h 定义*/
         mtcsRtlData/*!crtl*/->expr.x_inhibit_defer_pop-=1;
         return temp;
      }

      case VEC_DUPLICATE_EXPR:
         op0 = mtcs_expr_expand_expr/*!expand_expr*/(self,treeop0, NULL_RTX, VOIDmode, modifier);
         target = mtcs_optabs_expand_vector_broadcast/*!expand_vector_broadcast*/(mtcsOptabs,mode, op0);
         gcc_assert (target);
         return target;

      case VEC_SERIES_EXPR:
         mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1, NULL_RTX, &op0, &op1, modifier);
         return mtcs_optabs_expand_vec_series_expr/*!expand_vec_series_expr*/(mtcsOptabs,mode, op0, op1, target);

      case BIT_INSERT_EXPR:
      {
         unsigned bitpos = tree_to_uhwi (treeop2);
         unsigned bitsize;
         if (INTEGRAL_TYPE_P (TREE_TYPE (treeop1)))
            bitsize = TYPE_PRECISION (TREE_TYPE (treeop1));
         else
            bitsize = tree_to_uhwi (TYPE_SIZE (TREE_TYPE (treeop1)));
         op0 =mtcs_expr_expand_normal/*!expand_normal*/(self,treeop0);
         op1 =mtcs_expr_expand_normal/*!expand_normal*/(self,treeop1);
         rtx dst = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,dst, op0);
         machine_mode mode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (treeop1),TYPE_MODE (TREE_TYPE (treeop1)));
         mtcs_expmed_store_bit_field/*!store_bit_field*/(mtcsExpmed,dst, bitsize, bitpos, 0, 0,
         mode/*!TYPE_MODE (TREE_TYPE (treeop1))*/, op1, false, false);
         return dst;
      }

      default:
         gcc_unreachable ();
   }

   /* Here to do an ordinary binary operator.  */
binop:
   mtcs_expr_expand_operands/*!expand_operands*/(self,treeop0, treeop1,subtarget, &op0, &op1, EXPAND_NORMAL);
binop2:
   this_optab = optab_for_tree_code (code, type, optab_default);
binop3:
   if (modifier == EXPAND_STACK_PARM)
      target = 0;
   temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,
         mode, this_optab, op0, op1, target,unsignedp, OPTAB_LIB_WIDEN);
   gcc_assert (temp);
   /* Bitwise operations do not need bitfield reduction as we expect their
   operands being properly truncated.  */
   if (code == BIT_XOR_EXPR
   || code == BIT_AND_EXPR
   || code == BIT_IOR_EXPR)
      return temp;
   return REDUCE_BIT_FIELD (temp);
}
#undef REDUCE_BIT_FIELD


/* Attempt to generate a casesi instruction.  Returns true if successful,
   false otherwise (i.e. if there is no casesi instruction).

   DEFAULT_PROBABILITY is the probability of jumping to the default
   label.  */
//原型 try_casesi expr.h expr.cc
bool mtcs_expr_try_casesi (MtcsExpr *self,tree index_type, tree index_expr, tree minval, tree range,
        rtx table_label, rtx default_label, rtx fallback_label,
            profile_probability default_probability)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  class expand_operand ops[5];
  scalar_int_mode index_mode = SImode;
  rtx op1, op2, index;

  if (!target_rtx_have_casesi/*!targetm.have_casesi*/(mtcsMachine->tmrtx))
    return false;

  /* The index must be some form of integer.  Convert it to SImode.  */
  scalar_int_mode omode =mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,index_type);
  if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,omode) >
       mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,index_mode)){
      rtx rangertx =mtcs_expr_expand_normal/*!expand_normal*/(self,range);

      /* We must handle the endpoints in the original mode.  */
      index_expr = build2 (MINUS_EXPR, index_type,index_expr, minval);
      minval = integer_zero_node;
      index = mtcs_expr_expand_normal/*!expand_normal*/(self,index_expr);
      if (default_label)
          mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,rangertx, index, LTU, NULL_RTX,
                 omode, 1, default_label,default_probability);
      /* Now we can safely truncate.  */
      index = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,index_mode, index, 0);
  }else{
      if (omode != index_mode){
          index_type = lang_hooks.types.type_for_mode (index_mode, 0);
          index_expr = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, index_expr);
      }
      index = mtcs_expr_expand_normal/*!expand_normal*/(self,index_expr);
  }

  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);
  op1 = mtcs_expr_expand_normal/*!expand_normal*/(self,minval);
  op2 = mtcs_expr_expand_normal/*!expand_normal*/(self,range);

  create_input_operand (&ops[0], index, index_mode);
  create_convert_operand_from_type (&ops[1], op1, TREE_TYPE (minval));
  create_convert_operand_from_type (&ops[2], op2, TREE_TYPE (range));
  create_fixed_operand (&ops[3], table_label);
  create_fixed_operand (&ops[4], (default_label
                  ? default_label
                  : fallback_label));
  mtcs_optabs_expand_jump_insn/*!expand_jump_insn*/(mtcsOptabs,
          mtcsMachine->tmrtx->code_for_casesi/*!targetm.code_for_casesi*/, 5, ops);
  return true;
}

/* Attempt to generate a tablejump instruction; same concept.  */
/* Subroutine of the next function.

   INDEX is the value being switched on, with the lowest value
   in the table already subtracted.
   MODE is its expected mode (needed if INDEX is constant).
   RANGE is the length of the jump table.
   TABLE_LABEL is a CODE_LABEL rtx for the table itself.

   DEFAULT_LABEL is a CODE_LABEL rtx to jump to if the
   index value is out of range.
   DEFAULT_PROBABILITY is the probability of jumping to
   the default label.  */
//原型 do_tablejump expr.cc
static void do_tablejump (MtcsExpr *self,rtx index, machine_mode mode, rtx range, rtx table_label,
          rtx default_label, profile_probability default_probability)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *opts=mtcsOptions->global_options;

  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx temp, vector;
  if (INTVAL (range) > cfun->cfg->max_jumptable_ents)
    cfun->cfg->max_jumptable_ents = INTVAL (range);
  /* Do an unsigned comparison (in the proper mode) between the index
     expression and the value which represents the length of the range.
     Since we just finished subtracting the lower bound of the range
     from the index expression, this comparison allows us to simultaneously
     check that the original index expression value is both greater than
     or equal to the minimum value of the range and less than or equal to
     the maximum value of the range.  */
  if (default_label)
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,index, range, GTU, NULL_RTX, mode, 1,
                 default_label, default_probability);
  /* If index is in range, it must fit in Pmode.
     Convert to Pmode so we can index with it.  */
  if (mode != pMode){
      unsigned int width;
      /* We know the value of INDEX is between 0 and RANGE.  If we have a
     sign-extended subreg, and RANGE does not have the sign bit set, then
     we have a value that is valid for both sign and zero extension.  In
     this case, we get better code if we sign extend.  */
      if (GET_CODE (index) == SUBREG
      && SUBREG_PROMOTED_VAR_P (index)
      && SUBREG_PROMOTED_SIGNED_P (index)
      && ((width = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,
              mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode)))
          <= HOST_BITS_PER_WIDE_INT)
      && ! (UINTVAL (range) & (HOST_WIDE_INT_1U << (width - 1))))
          index = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,pMode, index, 0);
      else
          index = mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,pMode, index, 1);
  }
  /* Don't let a MEM slip through, because then INDEX that comes
     out of PIC_CASE_VECTOR_ADDRESS won't be a valid address,
     and break_out_memory_refs will go to work on it and mess it up.  */
#ifdef PIC_CASE_VECTOR_ADDRESS
  if (opts->x_flag_pic && !REG_P (index))
    index = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,pMode, index);
#endif
  /* ??? The only correct use of CASE_VECTOR_MODE is the one inside the
     GET_MODE_SIZE, because this indicates how large insns are.  The other
     uses should all be Pmode, because they are addresses.  This code
     could fail if addresses and insns are not the same size.  */
  index = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,MULT, pMode, index,
          mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,CASE_VECTOR_MODE),pMode));
  index = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, pMode, index,
                   gen_rtx_LABEL_REF (pMode, table_label));

#ifdef PIC_CASE_VECTOR_ADDRESS
  if (opts->x_flag_pic)
    index = PIC_CASE_VECTOR_ADDRESS (index);
  else
#endif
    index = mtcs_explow_memory_address/*!memory_address*/(mtcsExplow,CASE_VECTOR_MODE, index);
  temp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,CASE_VECTOR_MODE);
  vector = gen_const_mem (CASE_VECTOR_MODE, index);
  mtcs_expr_convert_move/*!convert_move*/(self,temp, vector, 0);
  mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,
          target_rtx_gen_tablejump/*!targetm.gen_tablejump*/(mtcsMachine->tmrtx,temp, table_label));
  /* If we are generating PIC code or if the table is PC-relative, the
     table and JUMP_INSN must be adjacent, so don't output a BARRIER.  */
  if (! CASE_VECTOR_PC_RELATIVE && ! opts->x_flag_pic)
      mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
}

//原型 try_tablejump expr.h expr.cc
bool mtcs_expr_try_tablejump (MtcsExpr *self,tree index_type, tree index_expr, tree minval, tree range,
           rtx table_label, rtx default_label,profile_probability default_probability)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  rtx index;
  if (! target_rtx_have_tablejump/*!targetm.have_tablejump*/(mtcsMachine->tmrtx))
    return false;

  index_expr = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, index_type,
                mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, index_expr),
                mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,index_type, minval));
  index = mtcs_expr_expand_normal/*!expand_normal*/(self,index_expr);
  mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

  do_tablejump(self,index, TYPE_MODE (index_type),
          mtcs_expr_convert_modes/*!convert_modes*/(self,TYPE_MODE (index_type),
                   TYPE_MODE (TREE_TYPE (range)),
                   mtcs_expr_expand_normal/*!expand_normal*/(self,range),
                   TYPE_UNSIGNED (TREE_TYPE (range))),
        table_label, default_label, default_probability);
  return true;
}


/* Check that store_by_pieces allows BITS + LEN (so that we don't
   expand something too unreasonably long), and every power of 2 in
   BITS.  It is assumed that LEN has already been tested by
   itself.  */
//原型 static bool can_store_by_multiple_pieces  builtins.cc
bool mtcs_expr_can_store_by_multiple_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT bits,
               by_pieces_constfn constfun,void *constfundata, unsigned int align,
               bool memsetp, unsigned HOST_WIDE_INT len)
{
   if (bits && !mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(self,bits + len,
                       constfun, constfundata, align, memsetp))
      return false;

   /* BITS set are expected to be generally in the low range and
   contiguous.  We do NOT want to repeat the test above in case BITS
   has a single bit set, so we terminate the loop when BITS == BIT.
   In the unlikely case that BITS has the MSB set, also terminate in
   case BIT gets shifted out.  */
   for (unsigned HOST_WIDE_INT bit = 1; bit < bits && bit; bit <<= 1){
      if ((bits & bit) == 0)
         continue;

      if (!mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(self,bit, constfun,
                     constfundata, align, memsetp))
         return false;
   }
   return true;
}

/* Try to store VAL (or, if NULL_RTX, VALC) in LEN bytes starting at TO.
   Return TRUE if successful, FALSE otherwise.  TO is assumed to be
   aligned at an ALIGN-bits boundary.  LEN must be a multiple of
   1<<CTZ_LEN between MIN_LEN and MAX_LEN.

   The strategy is to issue one store_by_pieces for each power of two,
   from most to least significant, guarded by a test on whether there
   are at least that many bytes left to copy in LEN.

   ??? Should we skip some powers of two in favor of loops?  Maybe start
   at the max of TO/LEN/word alignment, at least when optimizing for
   size, instead of ensuring O(log len) dynamic compares?  */
//原型 try_store_by_multiple_pieces expr.h builtins.cc
bool mtcs_expr_try_store_by_multiple_pieces (MtcsExpr *self,rtx to, rtx len,
               unsigned int ctz_len, unsigned HOST_WIDE_INT min_len,
               unsigned HOST_WIDE_INT max_len, rtx val, char valc, unsigned int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int max_bits = floor_log2 (max_len);
   int min_bits = floor_log2 (min_len);
   int sctz_len = ctz_len;
   gcc_checking_assert (sctz_len >= 0);
   if (val)
      valc = 1;
   /* Bits more significant than TST_BITS are part of the shared prefix
   in the binary representation of both min_len and max_len.  Since
   they're identical, we don't need to test them in the loop.  */
   int tst_bits = (max_bits != min_bits ? max_bits : floor_log2 (max_len ^ min_len));
   /* Save the pre-blksize values.  */
   int orig_max_bits = max_bits;
   int orig_tst_bits = tst_bits;
   /* Check whether it's profitable to start by storing a fixed BLKSIZE
   bytes, to lower max_bits.  In the unlikely case of a constant LEN
   (implied by identical MAX_LEN and MIN_LEN), we want to issue a
   single store_by_pieces, but otherwise, select the minimum multiple
   of the ALIGN (in bytes) and of the MCD of the possible LENs, that
   brings MAX_LEN below TST_BITS, if that's lower than min_len.  */
   unsigned HOST_WIDE_INT blksize;
   if (max_len > min_len){
      unsigned HOST_WIDE_INT alrng = MAX (HOST_WIDE_INT_1U << ctz_len,align / BITS_PER_UNIT);
      blksize = max_len - (HOST_WIDE_INT_1U << tst_bits) + alrng;
      blksize &= ~(alrng - 1);
   }else if (max_len == min_len)
      blksize = max_len;
   else
      /* Huh, max_len < min_len?  Punt.  See pr100843.c.  */
      return false;

   BuiltinReadStrData userData={mtcsBuiltins,&valc};
   if (min_len >= blksize
   /* ??? Maybe try smaller fixed-prefix blksizes before punting?  */
   && mtcs_expr_can_store_by_pieces/*!can_store_by_pieces*/(self, blksize,
         mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/,&userData/*!&valc*/, align, true)){
      min_len -= blksize;
      min_bits = floor_log2 (min_len);
      max_len -= blksize;
      max_bits = floor_log2 (max_len);

      tst_bits = (max_bits != min_bits ? max_bits  : floor_log2 (max_len ^ min_len));
   }else
      blksize = 0;

   /* Check that we can use store by pieces for the maximum store count
   we may issue (initial fixed-size block, plus conditional
   power-of-two-sized from max_bits to ctz_len.  */
   unsigned HOST_WIDE_INT xlenest = blksize;
   if (max_bits >= 0)
      xlenest += ((HOST_WIDE_INT_1U << max_bits) * 2  - (HOST_WIDE_INT_1U << ctz_len));
   bool max_loop = false;
   bool use_store_by_pieces = true;
   /* Skip the test in case of overflow in xlenest.  It shouldn't
   happen because of the way max_bits and blksize are related, but
   it doesn't hurt to test.  */
   if (blksize > xlenest
   || !mtcs_expr_can_store_by_multiple_pieces/*!can_store_by_multiple_pieces*/(self,xlenest - blksize,
       mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/,&userData/*!&valc*/, align, true, blksize)){
      if (!(mtcsOptionsItem->x_flag_inline_stringops & ILSOP_MEMSET))
         return false;

      for (max_bits = orig_max_bits; max_bits >= sctz_len; --max_bits){
         xlenest = ((HOST_WIDE_INT_1U << max_bits) * 2 - (HOST_WIDE_INT_1U << ctz_len));
         /* Check that blksize plus the bits to be stored as blocks
         sized at powers of two can be stored by pieces.  This is
         like the test above, but with smaller max_bits.  Skip
         orig_max_bits (it would be redundant).  Also skip in case
         of overflow.  */
         if (max_bits < orig_max_bits
         && xlenest + blksize >= xlenest
         && mtcs_expr_can_store_by_multiple_pieces/*!can_store_by_multiple_pieces*/(self,xlenest,
               mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/, &userData/*!&valc*/, align, true, blksize)){
            max_loop = true;
            break;
         }
         if (blksize  && mtcs_expr_can_store_by_multiple_pieces/*!can_store_by_multiple_pieces*/(self,xlenest,
               mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/, &userData/*!&valc*/, align, true, 0)){
            max_len += blksize;
            min_len += blksize;
            tst_bits = orig_tst_bits;
            blksize = 0;
            max_loop = true;
            break;
         }
         if (max_bits == sctz_len){
            /* We'll get here if can_store_by_pieces refuses to
            store even a single QImode.  We'll fall back to
            QImode stores then.  */
            if (!sctz_len){
               blksize = 0;
               max_loop = true;
               use_store_by_pieces = false;
               break;
            }
            --sctz_len;
            --ctz_len;
         }
      }
      if (!max_loop)
         return false;
      /* If the boundaries are such that min and max may run a
      different number of trips in the initial loop, the remainder
      needs not be between the moduli, so set tst_bits to cover all
      bits.  Otherwise, if the trip counts are the same, max_len
      has the common prefix, and the previously-computed tst_bits
      is usable.  */
      if (max_len >> max_bits > min_len >> max_bits)
         tst_bits = max_bits;
   }

   by_pieces_constfn constfun;
   if (val){
      machine_mode typeMode=TYPE_MODE (unsigned_char_type_node);
      typeMode= mtcs_mode_host2device_by_tree(mtcsMode,unsigned_char_type_node,typeMode);
      constfun = mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/;
      userData.data = val = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,typeMode/*!TYPE_MODE (unsigned_char_type_node)*/,val);
   }else {
      constfun = mtcs_builtins_builtin_memset_read_str/*!builtin_memset_read_str*/;
      userData.data = &valc;
   }

   rtx ptr = mtcs_explow_copy_addr_to_reg/*!copy_addr_to_reg*/(mtcsExplow,XEXP (to, 0));
   rtx rem = mtcs_explow_copy_to_mode_reg/*!copy_to_mode_reg*/(mtcsExplow,ptr_mode,
   mtcs_expr_convert_to_mode/*!convert_to_mode*/(self,ptr_mode, len, 0));
   to = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,to, ptr);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,to, align);

   if (blksize){
      to = mtcs_expr_store_by_pieces/*!store_by_pieces*/(self,to, blksize,
      constfun, &userData/*!constfundata*/,align, true, max_len != 0 ? RETURN_END : RETURN_BEGIN);
      if (max_len == 0)
         return true;

      /* Adjust PTR, TO and REM.  Since TO's address is likely
      PTR+offset, we have to replace it.  */
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,ptr,
            mtcs_expr_force_operand/*!force_operand*/(self,XEXP (to, 0), NULL_RTX));
      to = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,to, ptr);
      rtx rem_minus_blksize =mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,ptr_mode, rem, -blksize);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,rem,
            mtcs_expr_force_operand/*!force_operand*/(self,rem_minus_blksize, NULL_RTX));
   }

   /* Iterate over power-of-two block sizes from the maximum length to
   the least significant bit possibly set in the length.  */
   for (int i = max_bits; i >= sctz_len; i--){
      rtx_code_label *loop_label = NULL;
      rtx_code_label *label = NULL;

      blksize = HOST_WIDE_INT_1U << i;

      /* If we're past the bits shared between min_ and max_len, expand
      a test on the dynamic length, comparing it with the
      BLKSIZE.  */
      if (i <= tst_bits){
         label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,rem,
               mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,blksize), LT, NULL,
               ptr_mode, 1, label, profile_probability::even ());
      }
      /* If we are at a bit that is in the prefix shared by min_ and
      max_len, skip the current BLKSIZE if the bit is clear, but do
      not skip the loop, even if it doesn't require
      prechecking.  */
      else if ((max_len & blksize) == 0  && !(max_loop && i == max_bits))
         continue;

      if (max_loop && i == max_bits){
         loop_label =mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,loop_label);
         /* Since we may run this multiple times, don't assume we
         know anything about the offset.  */
         mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,to);
      }

      bool update_needed = i != sctz_len || loop_label;
      rtx next_ptr = NULL_RTX;
      if (!use_store_by_pieces){
         gcc_checking_assert (blksize == 1);
         if (!val)
            val = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,valc, mtcsMode->modes.M_QImode);
         to = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,to, mtcsMode->modes.M_QImode, 0);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,to, val);
         if (update_needed)
            next_ptr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,GET_MODE (ptr), ptr, blksize);
      }else{
         /* Issue a store of BLKSIZE bytes.  */
         to = mtcs_expr_store_by_pieces/*!store_by_pieces*/(self,to, blksize,
         constfun, &userData/*!constfundata*/,
         align, true,
         update_needed ? RETURN_END : RETURN_BEGIN);
         next_ptr = XEXP (to, 0);
      }
      /* Adjust REM and PTR, unless this is the last iteration.  */
      if (update_needed){
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,ptr,
               mtcs_expr_force_operand/*!force_operand*/(self,next_ptr, NULL_RTX));
         to = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,to, ptr);
         rtx rem_minus_blksize = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,ptr_mode, rem, -blksize);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,rem,
               mtcs_expr_force_operand/*!force_operand*/(self,rem_minus_blksize, NULL_RTX));
      }

      if (loop_label)
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
               rem,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,blksize), GE, NULL,
               ptr_mode, 1, loop_label,profile_probability::likely ());

      if (label){
         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
         /* Given conditional stores, the offset can no longer be
         known, so clear it.  */
         mtcs_rtl_clear_mem_offset/*!clear_mem_offset*/(mtcsRTL,to);
      }
   }
   return true;
}


/* Context used by compare_by_pieces_genfn.  It stores the fail label
   to jump to in case of miscomparison, and for branch ratios greater than 1,
   it stores an accumulator and the current and maximum counts before
   emitting another branch.  */

class mtcs_compare_by_pieces_d : public op_by_pieces_d
{
  rtx_code_label *m_fail_label;
  rtx m_accumulator;
  int m_count, m_batch;

  void generate (rtx, rtx, machine_mode) final override;
  bool prepare_mode (machine_mode, unsigned int) final override;
  void finish_mode (machine_mode) final override;

 public:
  mtcs_compare_by_pieces_d (rtx op0, rtx op1, by_pieces_constfn op1_cfn,
             void *op1_cfn_data, HOST_WIDE_INT len, int align,
             rtx_code_label *fail_label,MtcsExpr *mtcsExpr)
    : op_by_pieces_d (
          mtcs_reg_get_compare_max_pieces/*!COMPARE_MAX_PIECES*/(
                    (mtcs_target_get_reg(((MtcsTarget*)((MtcsMode *) MTCS_GET_MODE_OBJECT(mtcsExpr))->target)))),
            op0, true, op1, true, op1_cfn,
            op1_cfn_data, len, align, false, COMPARE_BY_PIECES,mtcsExpr)
  {
    m_fail_label = fail_label;
  }
};

/* A callback used when iterating for a compare_by_pieces_operation.
   OP0 and OP1 are the values that have been loaded and should be
   compared in MODE.  DATA holds a pointer to the compare_by_pieces_data
   context structure.  */

void mtcs_compare_by_pieces_d::generate (rtx op0, rtx op1, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

   if (m_batch > 1){
      rtx temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode,
            sub_optab, op0, op1, NULL_RTX,true, OPTAB_LIB_WIDEN);
      if (m_count != 0)
         temp = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, ior_optab,
               m_accumulator, temp, temp,true, OPTAB_LIB_WIDEN);
      m_accumulator = temp;

      if (++m_count < m_batch)
         return;

      m_count = 0;
      op0 = m_accumulator;
      op1 = const0_rtx;
      m_accumulator = NULL_RTX;
   }
   mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,op0, op1, NE, true, mode, NULL_RTX, NULL,
                             m_fail_label, profile_probability::uninitialized ());
}

/* Return true if MODE can be used for a set of moves and comparisons,
   given an alignment ALIGN.  Prepare whatever data is necessary for
   later calls to generate.  */

bool mtcs_compare_by_pieces_d::prepare_mode (machine_mode mode, unsigned int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   insn_code icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,mov_optab, mode);
   if (icode == CODE_FOR_nothing
   || align < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)
   || !mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,EQ, mode, ccp_jump))
      return false;
   m_batch = mtcsTarget/*!targetm.compare_by_pieces_branch_ratio*/->compare_by_pieces_branch_ratio(mtcsTarget,mode);
   if (m_batch < 0)
      return false;
   m_accumulator = NULL_RTX;
   m_count = 0;
   return true;
}

/* Called after expanding a series of comparisons in MODE.  If we have
   accumulated results for which we haven't emitted a branch yet, do
   so now.  */

void mtcs_compare_by_pieces_d::finish_mode (machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);

  if (m_accumulator != NULL_RTX)
     mtcs_dojump_do_compare_rtx_and_jump/*!do_compare_rtx_and_jump*/(mtcsDojump,m_accumulator, const0_rtx, NE, true, mode,
              NULL_RTX, NULL, m_fail_label,profile_probability::uninitialized ());
}


/* Generate several move instructions to compare LEN bytes from blocks
   ARG0 and ARG1.  (These are MEM rtx's with BLKmode).

   If PUSH_ROUNDING is defined and TO is NULL, emit_single_push_insn is
   used to push FROM to the stack.

   ALIGN is maximum stack alignment we can assume.

   Optionally, the caller can pass a constfn and associated data in A1_CFN
   and A1_CFN_DATA. describing that the second operand being compared is a
   known constant and how to obtain its data.  */
//原型 compare_by_pieces expr.cc
static rtx compare_by_pieces (MtcsExpr *self,rtx arg0, rtx arg1, unsigned HOST_WIDE_INT len,
         rtx target, unsigned int align,    by_pieces_constfn a1_cfn, void *a1_cfn_data)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx_code_label *fail_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   rtx_code_label *end_label = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   machine_mode integerTypeNodMode=TYPE_MODE (integer_type_node);
   integerTypeNodMode=mtcs_mode_host2device_by_tree(mtcsMode,integer_type_node,integerTypeNodMode);

   if (target == NULL_RTX || !REG_P (target)
         || REGNO (target) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,integerTypeNodMode/*!TYPE_MODE (integer_type_node)*/);

   mtcs_compare_by_pieces_d data (arg0, arg1, a1_cfn, a1_cfn_data, len, align,fail_label,self);

   data.run ();

   mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const0_rtx);
   mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,end_label);
   mtcs_emit_emit_barrier/*!emit_barrier*/(mtcsEmit);
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,fail_label);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const1_rtx);
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,end_label);

   return target;
}

/* Expand a block compare between X and Y with length LEN using the
   cmpmem optab, placing the result in TARGET.  LEN_TYPE is the type
   of the expression that was used to calculate the length.  ALIGN
   gives the known minimum common alignment.  */
//原型 emit_block_cmp_via_cmpmem expr.cc
static rtx emit_block_cmp_via_cmpmem (MtcsExpr *self,rtx x, rtx y, rtx len, tree len_type, rtx target,
            unsigned align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

   /* Note: The cmpstrnsi pattern, if it exists, is not suitable for
   implementing memcmp because it will stop if it encounters two
   zero bytes.  */
   insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
     cmpmem_optab, mtcsMode->modes.M_SImode);

   if (icode == CODE_FOR_nothing)
      return NULL_RTX;

   return mtcs_expr_expand_cmpstrn_or_cmpmem/*!expand_cmpstrn_or_cmpmem*/(self,
            icode, target, x, y, len_type, len, align);
}



/* Like emit_block_cmp_hints, but with known alignment and no support
   for constats.  Always expand to a loop with iterations that compare
   blocks of the largest compare-by-pieces size that divides both len
   and align, and then, if !EQUALITY_ONLY, identify the word and then
   the unit that first differs to return the result.  */
//原型 emit_block_cmp_via_loop expr.cc
static rtx emit_block_cmp_via_loop (MtcsExpr *self,rtx x, rtx y, rtx len, tree len_type, rtx target,
          bool equality_only, unsigned align, unsigned ctz_len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsDojump  *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   unsigned incr = align / BITS_PER_UNIT;

   if (CONST_INT_P (len))
      ctz_len = MAX (ctz_len, (unsigned) wi::ctz (UINTVAL (len)));

   if (HOST_WIDE_INT_1U << ctz_len < (unsigned HOST_WIDE_INT) incr)
      incr = HOST_WIDE_INT_1U << ctz_len;

   while (incr > 1 && !can_do_by_pieces(self,incr, align, COMPARE_BY_PIECES))
      incr >>= 1;

   rtx_code_label *cmp_label, *top_label, *ne_label, *res_label;
   rtx iter, x_addr, y_addr, tmp;
   machine_mode x_addr_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,x);
   machine_mode y_addr_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(mtcsRTL,y);
   machine_mode iter_mode;

   iter_mode = GET_MODE (len);
   if (iter_mode == VOIDmode)
      iter_mode = word_mode;

   rtx iter_init = const0_rtx;
   rtx_code iter_cond = LTU;
   rtx_code entry_cond = GEU;
   rtx iter_limit = len;
   rtx iter_incr = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,incr);
   machine_mode cmp_mode;

   /* We can drop the loop back edge if we know there's exactly one
   iteration.  */
   top_label = (!rtx_equal_p (len, iter_incr) ? mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL)  : NULL);
   /* We need not test before entering the loop if len is known
   nonzero.  ??? This could be even stricter, testing whether a
   nonconstant LEN could possibly be zero.  */
   cmp_label = (!CONSTANT_P (len) || rtx_equal_p (len, iter_init) ? mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL) : NULL);
   ne_label =  mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
   res_label =  mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

   iter = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,iter_mode);
   mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,iter, iter_init);

   opt_scalar_int_mode int_cmp_mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,incr * BITS_PER_UNIT, 1);
   if (!int_cmp_mode.exists (&cmp_mode)
   || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_cmp_mode.require ()) != incr * BITS_PER_UNIT
   || !mtcs_optabs_can_compare_p/*!can_compare_p*/(mtcsOptabs,NE, cmp_mode, ccp_jump)){
      cmp_mode = mtcsMode->modes.M_BLKmode;
      gcc_checking_assert (incr != 1);
   }

   /* Save the base addresses.  */
   x_addr = mtcs_expr_force_operand/*!force_operand*/(self,XEXP (x, 0), NULL_RTX);
   y_addr = mtcs_expr_force_operand/*!force_operand*/(self,XEXP (y, 0), NULL_RTX);
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

   if (cmp_label) {
      if (top_label)
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,cmp_label);
      else
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,iter, iter_limit, entry_cond,
             NULL_RTX, iter_mode, true, cmp_label, profile_probability::guessed_always ().apply_scale (1, 10));
   }
   if (top_label)
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,top_label);

   /* Offset the base addresses by ITER.  */
   tmp = mtcs_expr_convert_modes/*!convert_modes*/(self,x_addr_mode, iter_mode, iter, true);
   x_addr = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, x_addr_mode, x_addr, tmp);

   if (x_addr_mode != y_addr_mode)
      tmp = mtcs_expr_convert_modes/*!convert_modes*/(self,y_addr_mode, iter_mode, iter, true);
   y_addr = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, y_addr_mode, y_addr, tmp);

   x = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,x, cmp_mode, x_addr);
   y = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,y, cmp_mode, y_addr);

   /* Compare one block.  */
   rtx part_res;
   if (cmp_mode == mtcsMode->modes.M_BLKmode)
      part_res = compare_by_pieces(self,x, y, incr, target, align, 0, 0);
   else
      part_res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,cmp_mode,
            sub_optab, x, y, NULL_RTX,true, OPTAB_LIB_WIDEN);

   /* Stop if we found a difference.  */
   mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,
                     part_res,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,0), NE, NULL_RTX, GET_MODE (part_res),
                     true, ne_label,profile_probability::guessed_always().apply_scale (1, 10));

   /* Increment ITER.  */
   tmp = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,iter_mode,
             PLUS, iter, iter_incr, iter, true, OPTAB_LIB_WIDEN);
   if (tmp != iter)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,iter, tmp);

   if (cmp_label)
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,cmp_label);
   /* Loop until we reach the limit.  */

   if (top_label)
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,iter, iter_limit, iter_cond,
            NULL_RTX, iter_mode,true, top_label, profile_probability::guessed_always ().apply_scale (9, 10));

   /* We got to the end without differences, so the result is zero.  */
   machine_mode integerTypeNodMode=TYPE_MODE (integer_type_node);
   integerTypeNodMode=mtcs_mode_host2device_by_tree(mtcsMode,integer_type_node,integerTypeNodMode);
   if (target == NULL_RTX  || !REG_P (target)
         || REGNO (target) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg))
      target = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,integerTypeNodMode/*!TYPE_MODE (integer_type_node)*/);

   mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const0_rtx);
   mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,res_label);

   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,ne_label);

   /* Return nonzero, or pinpoint the difference to return the expected
   result for non-equality tests.  */
   if (equality_only)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const1_rtx);
   else{
      if (incr > UNITS_PER_WORD)
         /* ??? Re-compare the block found to be different one word at a
         time.  */
         part_res = emit_block_cmp_via_loop(self,x, y, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,incr), len_type,
                        target, equality_only, BITS_PER_WORD, 0);
      else if (incr > 1)
         /* ??? Re-compare the block found to be different one byte at a
         time.  We could do better using part_res, and being careful
         about endianness.  */
         part_res = emit_block_cmp_via_loop(self,x, y, mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,incr), len_type,
                        target, equality_only, BITS_PER_UNIT, 0);
      else if (known_gt (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (target)),
                                         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,cmp_mode)))
         part_res = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,GET_MODE (target), sub_optab, x, y, target,
                           true, OPTAB_LIB_WIDEN);
      else{
         /* In the odd chance target is QImode, we can't count on
         widening subtract to capture the result of the unsigned
         compares.  */
         rtx_code_label *ltu_label;
         ltu_label =  mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);
         mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,x, y, LTU, NULL_RTX,
         cmp_mode, true, ltu_label,profile_probability::guessed_always ().apply_scale (5, 10));

         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, const1_rtx);
         mtcs_emit_emit_jump/*!emit_jump*/(mtcsEmit,res_label);

         mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,ltu_label);
         mtcs_expr_emit_move_insn/*!emit_move_insn*/(self,target, constm1_rtx);
         part_res = target;
      }

      if (target != part_res)
         mtcs_expr_convert_move/*!convert_move*/(self,target, part_res, false);
   }

   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,res_label);

   return target;
}



/* Emit code to compare a block Y to a block X.  This may be done with
   string-compare instructions, with multiple scalar instructions,
   or with a library call.

   Both X and Y must be MEM rtx's.  LEN is an rtx that says how long
   they are.  LEN_TYPE is the type of the expression that was used to
   calculate it, and CTZ_LEN is the known trailing-zeros count of LEN,
   so LEN must be a multiple of 1<<CTZ_LEN even if it's not constant.

   If EQUALITY_ONLY is true, it means we don't have to return the tri-state
   value of a normal memcmp call, instead we can just compare for equality.
   If FORCE_LIBCALL is true, we should emit a call to memcmp rather than
   returning NULL_RTX.

   Optionally, the caller can pass a constfn and associated data in Y_CFN
   and Y_CFN_DATA. describing that the second operand being compared is a
   known constant and how to obtain its data.
   Return the result of the comparison, or NULL_RTX if we failed to
   perform the operation.  */
//原型 emit_block_cmp_hints expr.h expr.cc
rtx mtcs_expr_emit_block_cmp_hints (MtcsExpr *self,rtx x, rtx y, rtx len, tree len_type, rtx target,
            bool equality_only, by_pieces_constfn y_cfn, void *y_cfndata, unsigned ctz_len)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   rtx result = 0;

   if (CONST_INT_P (len) && INTVAL (len) == 0)
      return const0_rtx;

   gcc_assert (MEM_P (x) && MEM_P (y));
   unsigned int align = MIN (mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,x),
   mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,y));
   gcc_assert (align >= BITS_PER_UNIT);

   x = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,x, mtcsMode->modes.M_BLKmode, 0);
   y = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,y, mtcsMode->modes.M_BLKmode, 0);

   if (equality_only   && CONST_INT_P (len)
         && can_do_by_pieces(self,INTVAL (len), align, COMPARE_BY_PIECES))
      result = compare_by_pieces(self,x, y, INTVAL (len), target, align,y_cfn, y_cfndata);
   else
      result = emit_block_cmp_via_cmpmem(self,x, y, len, len_type, target, align);

   if (!result && (mtcsOptionsItem->x_flag_inline_stringops & ILSOP_MEMCMP))
      result = emit_block_cmp_via_loop(self,x, y, len, len_type,
                             target, equality_only, align, ctz_len);

   return result;
}

/* Try to expand cmpstrn or cmpmem operation ICODE with the given operands.
   ARG3_TYPE is the type of ARG3_RTX.  Return the result rtx on success,
   otherwise return null.  */
//原型 expand_cmpstrn_or_cmpmem expr.h expr.cc
rtx mtcs_expr_expand_cmpstrn_or_cmpmem (MtcsExpr *self,insn_code icode, rtx target, rtx arg1_rtx,
           rtx arg2_rtx, tree arg3_type, rtx arg3_rtx,HOST_WIDE_INT align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   machine_mode insn_mode = mtcsOutput->insn_data[icode].operand[0].mode;
   machine_mode arg3TypeMode=TYPE_MODE (arg3_type);
   arg3TypeMode=mtcs_mode_host2device_by_tree(mtcsMode,arg3_type,arg3TypeMode);

   if (target && (!REG_P (target) || mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,target)))
      target = NULL_RTX;

   class expand_operand ops[5];
   create_output_operand (&ops[0], target, insn_mode);
   create_fixed_operand (&ops[1], arg1_rtx);
   create_fixed_operand (&ops[2], arg2_rtx);
   create_convert_operand_from (&ops[3], arg3_rtx, arg3TypeMode/*!TYPE_MODE (arg3_type)*/,
   TYPE_UNSIGNED (arg3_type));
   mtcs_optabs_create_integer_operand/*!create_integer_operand*/(mtcsOptabs,&ops[4], align);
   if (mtcs_optabs_maybe_expand_insn/*!maybe_expand_insn*/(mtcsOptabs,icode, 5, ops))
      return ops[0].value;
   return NULL_RTX;
}

/* Given an expression EXP that may be a COMPONENT_REF, a BIT_FIELD_REF,
   an ARRAY_REF, or an ARRAY_RANGE_REF, look for nested operations of these
   codes and find the ultimate containing object, which we return.

   We set *PBITSIZE to the size in bits that we want, *PBITPOS to the
   bit position, *PUNSIGNEDP to the signedness and *PREVERSEP to the
   storage order of the field.
   If the position of the field is variable, we store a tree
   giving the variable offset (in units) in *POFFSET.
   This offset is in addition to the bit position.
   If the position is not variable, we store 0 in *POFFSET.

   If any of the extraction expressions is volatile,
   we store 1 in *PVOLATILEP.  Otherwise we don't change that.

   If the field is a non-BLKmode bit-field, *PMODE is set to VOIDmode.
   Otherwise, it is a mode that can be used to access the field.

   If the field describes a variable-sized object, *PMODE is set to
   BLKmode and *PBITSIZE is set to -1.  An access cannot be made in
   this case, but the address of the object can be found.  */
//原型 get_inner_reference tree.h expr.cc
tree mtcs_expr_get_inner_reference (MtcsExpr *self,tree exp, poly_int64 *pbitsize,
           poly_int64 *pbitpos, tree *poffset,
           machine_mode *pmode, int *punsignedp,
           int *preversep, int *pvolatilep)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   tree size_tree = 0;
   machine_mode mode = VOIDmode;
   bool blkmode_bitfield = false;
   tree offset = size_zero_node;
   poly_offset_int bit_offset = 0;

   /* First get the mode, signedness, storage order and size.  We do this from
   just the outermost expression.  */
   *pbitsize = -1;
   if (TREE_CODE (exp) == COMPONENT_REF){
      tree field = TREE_OPERAND (exp, 1);
      size_tree = DECL_SIZE (field);
      if (mtcsOptionsItem->x_flag_strict_volatile_bitfields > 0
      && TREE_THIS_VOLATILE (exp)
      && DECL_BIT_FIELD_TYPE (field)
      && DECL_MODE (field) != mtcsMode->modes.M_BLKmode)
         /* Volatile bitfields should be accessed in the mode of the
         field's type, not the mode computed based on the bit
         size.  */
         mode = TYPE_MODE (DECL_BIT_FIELD_TYPE (field));
      else if (!DECL_BIT_FIELD (field)){
         mode = DECL_MODE (field);
         /* For vector fields re-check the target flags, as DECL_MODE
         could have been set with different target flags than
         the current function has.  */
         if (VECTOR_TYPE_P (TREE_TYPE (field))
               && mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,TYPE_MODE_RAW (TREE_TYPE (field))))
            mode = TYPE_MODE (TREE_TYPE (field));
      }else if (DECL_MODE (field) == BLKmode)
         blkmode_bitfield = true;
      *punsignedp = DECL_UNSIGNED (field);
   }else if (TREE_CODE (exp) == BIT_FIELD_REF){
      size_tree = TREE_OPERAND (exp, 1);
      *punsignedp = (! INTEGRAL_TYPE_P (TREE_TYPE (exp)) || TYPE_UNSIGNED (TREE_TYPE (exp)));

      /* For vector element types with the correct size of access or for
      vector typed accesses use the mode of the access type.  */
      if ((TREE_CODE (TREE_TYPE (TREE_OPERAND (exp, 0))) == VECTOR_TYPE
      && TREE_TYPE (exp) == TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0)))
      && tree_int_cst_equal (size_tree, TYPE_SIZE (TREE_TYPE (exp))))
      || VECTOR_TYPE_P (TREE_TYPE (exp)))
         mode = TYPE_MODE (TREE_TYPE (exp));
   }else{
      mode = TYPE_MODE (TREE_TYPE (exp));
      *punsignedp = TYPE_UNSIGNED (TREE_TYPE (exp));

      if (mode ==mtcsMode->modes.M_BLKmode)
         size_tree = TYPE_SIZE (TREE_TYPE (exp));
      else
         *pbitsize = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode);
   }

   if (size_tree != 0){
      if (! tree_fits_uhwi_p (size_tree))
         mode = mtcsMode->modes.M_BLKmode, *pbitsize = -1;
      else
         *pbitsize = tree_to_uhwi (size_tree);
   }

   *preversep = reverse_storage_order_for_component_p (exp);

   /* Compute cumulative bit-offset for nested component-refs and array-refs,
   and find the ultimate containing object.  */
   while (1){
      switch (TREE_CODE (exp)){
         case BIT_FIELD_REF:
            bit_offset += wi::to_poly_offset (TREE_OPERAND (exp, 2));
            break;

         case COMPONENT_REF:
         {
            tree field = TREE_OPERAND (exp, 1);
            tree this_offset =mtcs_tree_component_ref_field_offset/*!component_ref_field_offset*/(mtcsTree,exp);
            aet_print_tree(exp);
            aet_print_tree(this_offset);
            aet_print_tree(field);

            /* If this field hasn't been filled in yet, don't go past it.
            This should only happen when folding expressions made during
            type construction.  */
            if (this_offset == 0)
               break;

            offset = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, offset, this_offset);
            bit_offset += wi::to_poly_offset (DECL_FIELD_BIT_OFFSET (field));

            /* ??? Right now we don't do anything with DECL_OFFSET_ALIGN.  */
         }
            break;

         case ARRAY_REF:
         case ARRAY_RANGE_REF:
         {
            tree index = TREE_OPERAND (exp, 1);
            tree low_bound = mtcs_tree_array_ref_low_bound/*!array_ref_low_bound*/(mtcsTree,exp);
            tree unit_size = mtcs_tree_array_ref_element_size/*!array_ref_element_size*/(mtcsTree,exp);
            tree elmt_type = TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0)));
            tree elemt0 = TREE_OPERAND (exp, 0);
            tree elemt0Type = TREE_TYPE (elemt0);
            tree elemt1Type = TREE_TYPE (elemt0Type);
            tree treesizeunit = TYPE_SIZE_UNIT (elemt1Type);
            tree treesizeunittype = TREE_TYPE(treesizeunit);

            n_debug("mtcsexpr.c get_inner_refere xx 00 elemt0 %p %s\n",elemt0,get_tree_code_name(TREE_CODE(elemt0)));
            n_debug("mtcsexpr.c get_inner_refere xx 11 elemt0Type %p %s %d\n",
                  elemt0Type,get_tree_code_name(TREE_CODE(elemt0Type)),TYPE_MODE(elemt0Type));
            n_debug("mtcsexpr.c get_inner_refere xx 22 elemt1Type %p %s %d\n",
                  elemt1Type,get_tree_code_name(TREE_CODE(elemt1Type)),TYPE_MODE(elemt1Type));
            n_debug("mtcsexpr.c get_inner_refere xx 33 treesizeunit %p %s\n",
                  treesizeunit,get_tree_code_name(TREE_CODE(treesizeunit)));
            n_debug("mtcsexpr.c get_inner_refere xx 44 treesizeunittype %p %s %d\n",
                  treesizeunittype,get_tree_code_name(TREE_CODE(treesizeunittype)),TYPE_MODE(treesizeunittype));



            /* We assume all arrays have sizes that are a multiple of a byte.
            First subtract the lower bound, if any, in the type of the
            index, then convert to sizetype and multiply by the size of
            the array element.  */
            if (! integer_zerop (low_bound)){
               index = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MINUS_EXPR, TREE_TYPE (index),index, low_bound);
               n_debug("mtcsexpr.cc get_inner_reference 11\n");
            }
            tree t0= mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,mtcs_sizetype, index);
            tree unitsizeType=TREE_TYPE (unit_size);
            n_debug("mtcsexpr.c get_inner_reference 22 %d\n",TYPE_MODE(unitsizeType));
            //gcc_assert (int_binop_types_match_p (code, TREE_TYPE (arg0),TREE_TYPE (arg1)));
            tree t1=mtcs_const_size_binop/*!size_binop*/(mtcsConst,MULT_EXPR,t0,unit_size);
            tree t2=mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, offset,t1);
            offset = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, offset,
                  mtcs_const_size_binop/*!size_binop*/(mtcsConst,MULT_EXPR,
                        mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,mtcs_sizetype, index),unit_size));
         }
            break;

         case REALPART_EXPR:
            break;

         case IMAGPART_EXPR:
            bit_offset += *pbitsize;
            break;

         case VIEW_CONVERT_EXPR:
            break;

         case MEM_REF:
            /* Hand back the decl for MEM[&decl, off].  */
            if (TREE_CODE (TREE_OPERAND (exp, 0)) == ADDR_EXPR){
               tree off = TREE_OPERAND (exp, 1);
               if (!integer_zerop (off)){
                  poly_offset_int boff = mem_ref_offset (exp);
                  boff <<= LOG2_BITS_PER_UNIT;
                  bit_offset += boff;
               }
               exp = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
            }
            goto done;

         default:
            goto done;
      }

      /* If any reference in the chain is volatile, the effect is volatile.  */
      if (TREE_THIS_VOLATILE (exp))
         *pvolatilep = 1;

      exp = TREE_OPERAND (exp, 0);
   }
done:

   /* If OFFSET is constant, see if we can return the whole thing as a
   constant bit position.  Make sure to handle overflow during
   this conversion.  */
   if (poly_int_tree_p (offset)){
      poly_offset_int tem = wi::sext (wi::to_poly_offset (offset),TYPE_PRECISION (sizetype));
      tem <<= LOG2_BITS_PER_UNIT;
      tem += bit_offset;
      if (tem.to_shwi (pbitpos))
         *poffset = offset = NULL_TREE;
   }

   /* Otherwise, split it up.  */
   if (offset){
      /* Avoid returning a negative bitpos as this may wreak havoc later.  */
      if (!bit_offset.to_shwi (pbitpos) || maybe_lt (*pbitpos, 0)){
         *pbitpos = num_trailing_bits (bit_offset.force_shwi ());
         poly_offset_int bytes = bits_to_bytes_round_down (bit_offset);
         offset = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, offset,
               mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,sizetype, bytes.force_shwi ()));
      }

      *poffset = offset;
   }

   /* We can use BLKmode for a byte-aligned BLKmode bitfield.  */
   if (mode == VOIDmode  && blkmode_bitfield
   && multiple_p (*pbitpos, BITS_PER_UNIT)
   && multiple_p (*pbitsize, BITS_PER_UNIT))
      *pmode = mtcsMode->modes.M_BLKmode;
   else
      *pmode = mode;

   return exp;
}

/* Calculate CRC for the initial CRC and given POLYNOMIAL.
   CRC_BITS is CRC size.  */
static unsigned HOST_WIDE_INT calculate_crc (unsigned HOST_WIDE_INT crc,
          unsigned HOST_WIDE_INT polynomial,
          unsigned short crc_bits)
{
   unsigned HOST_WIDE_INT msb = HOST_WIDE_INT_1U << (crc_bits - 1);
   crc = crc << (crc_bits - 8);
   for (short i = 8; i > 0; --i){
      if (crc & msb)
         crc = (crc << 1) ^ polynomial;
      else
         crc <<= 1;
   }
   /* Zero out bits in crc beyond the specified number of crc_bits.  */
   if (crc_bits < sizeof (crc) * CHAR_BIT)
      crc &= (HOST_WIDE_INT_1U << crc_bits) - 1;
   return crc;
}

/* Assemble CRC table with 256 elements for the given POLYNOM and CRC_BITS.
   POLYNOM is the polynomial used to calculate the CRC table's elements.
   CRC_BITS is the size of CRC, may be 8, 16, ... . */
static rtx assemble_crc_table (MtcsExpr *self,unsigned HOST_WIDE_INT polynom, unsigned short crc_bits)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   unsigned table_el_n = 0x100;
   tree ar = build_array_type (mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,
         crc_bits),build_index_type (mtcs_tree_get_size_int/*!size_int*/(mtcsTree,table_el_n - 1)));

   /* Initialize the table.  */
   vec<tree, va_gc> *initial_values;
   vec_alloc (initial_values, table_el_n);
   for (size_t i = 0; i < table_el_n; ++i) {
      unsigned HOST_WIDE_INT crc = calculate_crc (i, polynom, crc_bits);
      tree element = build_int_cstu (make_unsigned_type (crc_bits), crc);
      vec_safe_push (initial_values, element);
   }
   tree ctor = build_constructor_from_vec (ar, initial_values);
   rtx mem = output_constant_def (ctor, 1);
   gcc_assert (MEM_P (mem));
   if (dump_file && (dump_flags & TDF_DETAILS)){
      fprintf (dump_file, ";; emitting crc table crc_%u_polynomial_"HOST_WIDE_INT_PRINT_HEX " ", crc_bits, polynom);
      print_rtl_single (dump_file, XEXP (mem, 0));
      fprintf (dump_file, "\n");
   }

   return XEXP (mem, 0);
}

/* Generate CRC lookup table by calculating CRC for all possible
   8-bit data values.  The table is stored with a specific name in the read-only
   static data section.
   POLYNOM is the polynomial used to calculate the CRC table's elements.
   CRC_BITS is the size of CRC, may be 8, 16, ... .  */
static rtx generate_crc_table (MtcsExpr *self,unsigned HOST_WIDE_INT polynom, unsigned short crc_bits)
{
   gcc_assert (crc_bits <= 64);
   return assemble_crc_table (self,polynom, crc_bits);
}

/* Generate table-based CRC code for the given CRC, INPUT_DATA and the
   POLYNOMIAL (without leading 1).

   First, using POLYNOMIAL's value generates CRC table of 256 elements,
   then generates the assembly for the following code,
   where crc_bit_size and data_bit_size may be 8, 16, 32, 64, depending on CRC:

     for (int i = 0; i < data_bit_size / 8; i++)
       crc = (crc << 8) ^ crc_table[(crc >> (crc_bit_size - 8))
                ^ (data >> (data_bit_size - (i + 1) * 8)
                & 0xFF))];

   So to take values from the table, we need 8-bit data.
   If input data size is not 8, then first we extract upper 8 bits,
   then the other 8 bits, and so on.  */

static void calculate_table_based_CRC (MtcsExpr *self,rtx *crc, const rtx &input_data,
            const rtx &polynomial,
            machine_mode data_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   machine_mode mode = GET_MODE (*crc);
   unsigned short crc_bit_size = mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,mode).to_constant ();
   unsigned short data_size = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,data_mode).to_constant ();
   rtx tab = generate_crc_table(self,UINTVAL (polynomial), crc_bit_size);

   for (unsigned short i = 0; i < data_size; i++){
      /* crc >> (crc_bit_size - 8).  */
      *crc = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mode, *crc);
      rtx op1 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, *crc, crc_bit_size - 8,NULL_RTX, 1);

      /* data >> (8 * (GET_MODE_SIZE (data_mode).to_constant () - i - 1)).  */
      unsigned range_8 = 8 * (data_size - i - 1);
      /* CRC's mode is always at least as wide as INPUT_DATA.  Convert
      INPUT_DATA into CRC's mode.  */
      rtx data = gen_reg_rtx (mode);
      mtcs_expr_convert_move/*!convert_move*/(self,data, input_data, 1);
      data = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, mode, data, range_8, NULL_RTX, 1);

      /* data >> (8 * (GET_MODE_SIZE (mode)
      .to_constant () - i - 1)) & 0xFF.  */
      rtx data_final =mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,
            mode, data,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,255, mode), NULL_RTX);

      /* (crc >> (crc_bit_size - 8)) ^ data_8bit.  */
      rtx in = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, xor_optab, op1, data_final,
      NULL_RTX, 1, OPTAB_WIDEN);

      /* ((crc >> (crc_bit_size - 8)) ^ data_8bit) & 0xFF.  */
      rtx index = mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,
            mode, in,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,255, mode),NULL_RTX);
      int log_crc_size = exact_log2 (mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode).to_constant ());
      index = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, index,log_crc_size, NULL_RTX, 0);

      rtx addr = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcs_mode_get_Pmode(mtcsMode));
      mtcs_expr_convert_move/*!convert_move*/(self,addr, index, 1);
      addr = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mtcs_mode_get_Pmode(mtcsMode), add_optab, addr, tab, NULL_RTX,
      0, OPTAB_DIRECT);

      /* crc_table[(crc >> (crc_bit_size - 8)) ^ data_8bit]  */
      rtx tab_el = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,gen_rtx_MEM (mode, addr));

      /* (crc << 8) if CRC is larger than 8, otherwise crc = 0.  */
      rtx high = NULL_RTX;
      if (crc_bit_size != 8)
         high = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, mode, *crc, 8, NULL_RTX, 0);
      else
         high = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,0, mode);

      /* crc = (crc << 8)
      ^ crc_table[(crc >> (crc_bit_size - 8)) ^ data_8bit];  */
      *crc = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,mode, xor_optab, tab_el, high, NULL_RTX, 1,OPTAB_WIDEN);
   }
}


/* Generate table-based CRC code for the given CRC, INPUT_DATA and the
   POLYNOMIAL (without leading 1).

   CRC is OP1, data is OP2 and the polynomial is OP3.
   This must generate a CRC table and an assembly for the following code,
   where crc_bit_size and data_bit_size may be 8, 16, 32, 64:
   uint_crc_bit_size_t
   crc_crc_bit_size (uint_crc_bit_size_t crc_init,
           uint_data_bit_size_t data, size_t size)
   {
     uint_crc_bit_size_t crc = crc_init;
     for (int i = 0; i < data_bit_size / 8; i++)
       crc = (crc << 8) ^ crc_table[(crc >> (crc_bit_size - 8))
                ^ (data >> (data_bit_size - (i + 1) * 8)
                & 0xFF))];
     return crc;
   }  */
//原型 expand_crc_table_based expr.h expr.cc
void mtcs_expr_expand_crc_table_based (MtcsExpr *self,rtx op0, rtx op1, rtx op2, rtx op3,
         machine_mode data_mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   gcc_assert (!CONST_INT_P (op0));
   gcc_assert (CONST_INT_P (op3));
   machine_mode crc_mode = GET_MODE (op0);
   rtx crc = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,crc_mode);
   mtcs_expr_convert_move/*!convert_move*/(self,crc, op1, 0);
   calculate_table_based_CRC(self,&crc, op2, op3, data_mode);
   mtcs_expr_convert_move/*!convert_move*/(self,op0, crc, 0);
}


/* Generate table-based reversed CRC code for the given CRC, INPUT_DATA and
   the POLYNOMIAL (without leading 1).

   CRC is OP1, data is OP2 and the polynomial is OP3.
   This must generate CRC table and assembly for the following code,
   where crc_bit_size and data_bit_size may be 8, 16, 32, 64:
   uint_crc_bit_size_t
   crc_crc_bit_size (uint_crc_bit_size_t crc_init,
            uint_data_bit_size_t data, size_t size)
   {
     reflect (crc_init)
     uint_crc_bit_size_t crc = crc_init;
     reflect (data);
     for (int i = 0; i < data_bit_size / 8; i++)
       crc = (crc << 8) ^ crc_table[(crc >> (crc_bit_size - 8))
           ^ (data >> (data_bit_size - (i + 1) * 8) & 0xFF))];
     reflect (crc);
     return crc;
   }  */
//原型 expand_reversed_crc_table_based expr.h expr.cc
void mtcs_expr_expand_reversed_crc_table_based (MtcsExpr *self,rtx op0, rtx op1, rtx op2, rtx op3,
             machine_mode data_mode, void (*gen_reflecting_code) (rtx *op,void *userData),void *userData)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   gcc_assert (!CONST_INT_P (op0));
   gcc_assert (CONST_INT_P (op3));
   machine_mode crc_mode = GET_MODE (op0);

   rtx crc = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,crc_mode);
   mtcs_expr_convert_move/*!convert_move*/(self,crc, op1, 0);
   gen_reflecting_code (&crc,userData);

   rtx data = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,data_mode);
   mtcs_expr_convert_move/*!convert_move*/(self,data, op2, 0);
   gen_reflecting_code (&data,userData);

   calculate_table_based_CRC(self,&crc, data, op3, data_mode);

   gen_reflecting_code (&crc,userData);
   mtcs_expr_convert_move/*!convert_move*/(self,op0, crc, 0);
}

/* Generate the common operation for reflecting values:
   *OP = (*OP & AND1_VALUE) << SHIFT_VAL | (*OP & AND2_VALUE) >> SHIFT_VAL;  */

static void gen_common_operation_to_reflect (MtcsExpr *self,rtx *op,
             unsigned HOST_WIDE_INT and1_value,
             unsigned HOST_WIDE_INT and2_value,
             unsigned shift_val)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);

   rtx op1 = mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,
         GET_MODE (*op), *op,mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,and1_value, GET_MODE (*op)), NULL_RTX);
   op1 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR, GET_MODE (*op), op1, shift_val, op1, 0);
   rtx op2 = mtcs_expmed_expand_and/*!expand_and*/(mtcsExpmed,
         GET_MODE (*op), *op, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,and2_value, GET_MODE (*op)), NULL_RTX);
   op2 = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,RSHIFT_EXPR, GET_MODE (*op), op2, shift_val, op2, 1);
   *op = mtcs_optabs_expand_binop/*!expand_binop*/(mtcsOptabs,GET_MODE (*op), ior_optab, op1, op2, *op, 0, OPTAB_LIB_WIDEN);
}


/* Reflect 64-bit value for the 64-bit target.  */
static void reflect_64_bit_value (MtcsExpr *self,rtx *op)
{
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x00000000FFFFFFFF),HOST_WIDE_INT_C (0xFFFFFFFF00000000), 32);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x0000FFFF0000FFFF),HOST_WIDE_INT_C (0xFFFF0000FFFF0000), 16);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x00FF00FF00FF00FF),HOST_WIDE_INT_C (0xFF00FF00FF00FF00), 8);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x0F0F0F0F0F0F0F0F),HOST_WIDE_INT_C (0xF0F0F0F0F0F0F0F0), 4);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x3333333333333333),HOST_WIDE_INT_C (0xCCCCCCCCCCCCCCCC), 2);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x5555555555555555),HOST_WIDE_INT_C (0xAAAAAAAAAAAAAAAA), 1);
}

/* Reflect 32-bit value for the 32-bit target.  */

static void reflect_32_bit_value (MtcsExpr *self,rtx *op)
{
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x0000FFFF),HOST_WIDE_INT_C (0xFFFF0000), 16);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x00FF00FF),HOST_WIDE_INT_C (0xFF00FF00), 8);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x0F0F0F0F), HOST_WIDE_INT_C (0xF0F0F0F0), 4);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x33333333),HOST_WIDE_INT_C (0xCCCCCCCC), 2);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x55555555), HOST_WIDE_INT_C (0xAAAAAAAA), 1);
}

/* Reflect 16-bit value for the 16-bit target.  */

static void reflect_16_bit_value (MtcsExpr *self,rtx *op)
{
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x00FF),HOST_WIDE_INT_C (0xFF00), 8);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x0F0F),HOST_WIDE_INT_C (0xF0F0), 4);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x3333),HOST_WIDE_INT_C (0xCCCC), 2);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x5555),HOST_WIDE_INT_C (0xAAAA), 1);
}

/* Reflect 8-bit value for the 8-bit target.  */

static void reflect_8_bit_value (MtcsExpr *self,rtx *op)
{
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x0F),HOST_WIDE_INT_C (0xF0), 4);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x33),HOST_WIDE_INT_C (0xCC), 2);
  gen_common_operation_to_reflect(self,op, HOST_WIDE_INT_C (0x55),HOST_WIDE_INT_C (0xAA), 1);
}

/* Generate instruction sequence which reflects the value of the OP
   using shift, and, or operations.  OP's mode may be less than word_mode.  */
//原型 generate_reflecting_code_standard expr.h expr.cc
void mtcs_expr_generate_reflecting_code_standard (MtcsExpr *self,rtx *op)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   gcc_assert (mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (*op)).to_constant ()  >= 8
   && mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (*op)).to_constant () <= 64);

   if (mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (*op)).to_constant () == 64)
      reflect_64_bit_value(self,op);
   else if (mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (*op)).to_constant () == 32)
      reflect_32_bit_value(self,op);
   else if (mtcs_mode_get_bitsize_poly/*!GET_MODE_BITSIZE*/(mtcsMode,GET_MODE (*op)).to_constant () == 16)
      reflect_16_bit_value(self,op);
   else
      reflect_8_bit_value(self,op);
}

/* Extracts the personality function of DECL and returns the corresponding
   libfunc.  */
//原型 get_personality_function expr.h expr.cc
rtx mtcs_expr_get_personality_function (MtcsExpr *self,tree decl)
{
   tree personality = DECL_FUNCTION_PERSONALITY (decl);
   enum eh_personality_kind pk;

   pk = function_needs_eh_personality (DECL_STRUCT_FUNCTION (decl));
   if (pk == eh_personality_none)
      return NULL;

   if (!personality && pk == eh_personality_any)
      personality = lang_hooks.eh_personality ();

   if (pk == eh_personality_lang)
      gcc_assert (personality != NULL_TREE);

   return XEXP (DECL_RTL (personality), 0);
}


MtcsExpr *mtcs_expr_new(MtcsMode *mtcsMode)
{
     MtcsExpr *self = n_slice_alloc0 (sizeof(MtcsExpr));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsExprInit(self);
     return self;
}
