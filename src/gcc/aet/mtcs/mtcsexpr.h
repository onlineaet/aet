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


#ifndef __GCC_MTCS_EXPR__
#define __GCC_MTCS_EXPR__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "target.h"
#include "expr.h"


typedef struct _MtcsExpr MtcsExpr;


struct _MtcsExpr
{
    MtcsComponent parent;
    //原型 POINTERS_EXTEND_UNSIGNED emit_block_move_via_oriented_loop 中定义 或 i386.h中定义 POINTERS_EXTEND_UNSIGNED=1
    int pointersExtendUnsigned;
};

MtcsExpr *mtcs_expr_new(MtcsMode *mtcsMode);
//原型 force_operand rtl.h expr.cc
rtx       mtcs_expr_force_operand (MtcsExpr *self,rtx value, rtx target);
//原型 emit_move_insn_1 expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_insn_1 (MtcsExpr *self,rtx x, rtx y);
//原型 emit_move_insn expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_insn (MtcsExpr *self,rtx x, rtx y);
//原型 convert_modes expr.h expr.cc
rtx mtcs_expr_convert_modes (MtcsExpr *self,machine_mode mode, machine_mode oldmode, rtx x, int unsignedp);
//原型 convert_to_mode expr.h expr.cc
rtx mtcs_expr_convert_to_mode (MtcsExpr *self,machine_mode mode, rtx x, int unsignedp);
//原型 convert_move expr.h expr.cc
void mtcs_expr_convert_move (MtcsExpr *self,rtx to, rtx from, int unsignedp);
//原型 gen_move_insn expr.h expr.cc
rtx_insn * mtcs_expr_gen_move_insn (MtcsExpr *self,rtx x, rtx y);
//原型 emit_move_resolve_push expr.h expr.cc
rtx mtcs_expr_emit_move_resolve_push (MtcsExpr *self,machine_mode mode, rtx x);
//原型 emit_block_move expr.h expr.cc
rtx mtcs_expr_emit_block_move (MtcsExpr *self,rtx x, rtx y, rtx size, enum block_op_methods method, unsigned int ctz_size=0);
//原型 emit_block_move_hints expr.h expr.cc
rtx mtcs_expr_emit_block_move_hints (MtcsExpr *self,rtx x, rtx y, rtx size, enum block_op_methods method,
               unsigned int expected_align, HOST_WIDE_INT expected_size,
               unsigned HOST_WIDE_INT min_size,
               unsigned HOST_WIDE_INT max_size,
               unsigned HOST_WIDE_INT probable_max_size,
               bool bail_out_libcall, bool *is_move_done,
               bool might_overlap, unsigned ctz_size = 0);
//原型 can_move_by_pieces expr.h expr.cc
bool mtcs_expr_can_move_by_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT len, unsigned int align);
//原型 by_pieces_ninsns target.h expr.cc
unsigned HOST_WIDE_INT mtcs_expr_by_pieces_ninsns(MtcsExpr *self,unsigned HOST_WIDE_INT l, unsigned int align,
          unsigned int max_size, by_pieces_operation op);
//原型 emit_block_copy_via_libcall expr.h
rtx mtcs_expr_emit_block_copy_via_libcall (MtcsExpr *self,rtx dst, rtx src, rtx size, bool tailcall = false);
//原型 emit_block_op_via_libcall expr.h expr.cc
rtx mtcs_expr_emit_block_op_via_libcall (MtcsExpr *self,enum built_in_function fncode, rtx dst, rtx src,
               rtx size, bool tailcall);
//原型 emit_block_copy_via_libcall expr.h
inline rtx mtcs_expr_emit_block_copy_via_libcall (MtcsExpr *self,rtx dst, rtx src, rtx size, bool tailcall = false)
{
  return mtcs_expr_emit_block_op_via_libcall(self,BUILT_IN_MEMCPY, dst, src, size, tailcall);
}
//原型 emit_block_move_via_libcall expr.h
inline rtx mtcs_expr_emit_block_move_via_libcall (MtcsExpr *self,rtx dst, rtx src, rtx size, bool tailcall = false)
{
  return mtcs_expr_emit_block_op_via_libcall(self,BUILT_IN_MEMMOVE, dst, src, size, tailcall);
}
//原型 emit_block_comp_via_libcall expr.h
inline rtx mtcs_expr_emit_block_comp_via_libcall (MtcsExpr *self,rtx dst, rtx src, rtx size, bool tailcall = false)
{
  return mtcs_expr_emit_block_op_via_libcall(self,BUILT_IN_MEMCMP, dst, src, size, tailcall);
}


//原型 emit_group_load expr.h expr.cc
void mtcs_expr_emit_group_load (MtcsExpr *self,rtx dst, rtx src, tree type, poly_int64 ssize);
//原型 emit_push_insn expr.h expr.cc
bool mtcs_expr_emit_push_insn(MtcsExpr *self,rtx x, machine_mode mode, tree type, rtx size, unsigned int align,
        int partial, rtx reg, poly_int64 extra, rtx args_addr, rtx args_so_far, int reg_parm_stack_space,rtx alignment_pad, bool sibcall_p);
//原型 push_block expr.h expr.cc
rtx mtcs_expr_push_block (MtcsExpr *self,rtx size, poly_int64 extra, int below);
//原型 use_group_regs expr.h expr.cc
void mtcs_expr_use_group_regs (MtcsExpr *self,rtx *call_fusage, rtx regs);
//原型 use_regs expr.h expr.cc
void mtcs_expr_use_regs (MtcsExpr *self,rtx *call_fusage, int regno, int nregs);
//原型 move_block_to_reg expr.h expr.cc
void mtcs_expr_move_block_to_reg (MtcsExpr *self ,int regno, rtx x, int nregs, machine_mode mode);
//原型 emit_group_store expr.h expr.cc
void mtcs_expr_emit_group_store (MtcsExpr *self,rtx orig_dst, rtx src, tree type ATTRIBUTE_UNUSED,poly_int64 ssize);

//原型 expand_expr_real_1 expr.h expr.cc
rtx mtcs_expr_expand_expr_real_1 (MtcsExpr *self,tree exp, rtx target, machine_mode tmode,
            enum expand_modifier modifier, rtx *alt_rtl,bool inner_reference_p);
//原型 expand_expr_real expr.h expr.cc
rtx mtcs_expr_expand_expr_real (MtcsExpr *self,tree exp, rtx target, machine_mode tmode,
          enum expand_modifier modifier, rtx *alt_rtl, bool inner_reference_p);
/* Generate code for computing expression EXP.
   An rtx for the computed value is returned.  The value is never null.
   In the case of a void EXP, const0_rtx is returned.  */
//原型 expand_expr expr.h
inline rtx mtcs_expr_expand_expr (MtcsExpr *self,tree exp, rtx target, machine_mode mode,enum expand_modifier modifier)
{
  return mtcs_expr_expand_expr_real (self,exp, target, mode, modifier, NULL, false);
}
//原型 expand_normal expr.h
inline rtx mtcs_expr_expand_normal(MtcsExpr *self,tree exp)
{
  return mtcs_expr_expand_expr_real (self,exp, NULL_RTX, VOIDmode, EXPAND_NORMAL, NULL, false);
}


//原型 store_constructor expr.h expr.cc
void mtcs_expr_store_constructor(MtcsExpr *self,tree exp, rtx target, int cleared, poly_int64 size, bool reverse);
//原型 clear_storage expr.h expr.cc
rtx mtcs_expr_clear_storage(MtcsExpr *self,rtx object, rtx size, enum block_op_methods method);

//原型 clear_storage_hints expr.h expr.cc
rtx mtcs_expr_clear_storage_hints (MtcsExpr *self,rtx object, rtx size, enum block_op_methods method,
             unsigned int expected_align, HOST_WIDE_INT expected_size,
             unsigned HOST_WIDE_INT min_size,
             unsigned HOST_WIDE_INT max_size,
             unsigned HOST_WIDE_INT probable_max_size,
             unsigned ctz_size);

//原型 use_reg_mode expr.h expr.cc
void mtcs_expr_use_reg_mode (MtcsExpr *self,rtx *call_fusage, rtx reg, machine_mode mode);
/* Mark REG as holding a parameter for the next CALL_INSN.  */
//原型 use_reg expr.h
inline void mtcs_expr_use_reg (MtcsExpr *self,rtx *fusage, rtx reg)
{
    mtcs_expr_use_reg_mode/*!use_reg_mode*/(self,fusage, reg, VOIDmode);
}

//原型 expr_size expr.h expr.cc
rtx mtcs_expr_expr_size (MtcsExpr *self,tree exp);
//原型 convert_float_to_wider_int expr.h expr.cc
rtx mtcs_expr_convert_float_to_wider_int(MtcsExpr *self,machine_mode mode, machine_mode fmode, rtx x);
//原型 emit_group_load_into_temps expr.h expr.cc
rtx mtcs_expr_emit_group_load_into_temps (MtcsExpr *self,rtx parallel, rtx src, tree type, poly_int64 ssize);
//原型 emit_group_move expr.h expr.cc
void mtcs_expr_emit_group_move (MtcsExpr *self,rtx dst, rtx src);
//原型 emit_group_move_into_temps expr.h expr.cc
rtx mtcs_expr_emit_group_move_into_temps(MtcsExpr *self,rtx src);
//原型 convert_wider_int_to_float expr.h expr.cc
rtx mtcs_expr_convert_wider_int_to_float (MtcsExpr *self,machine_mode mode, machine_mode imode, rtx x);
//原型 fixup_args_size_notes rtl.h expr.cc
poly_int64 mtcs_expr_fixup_args_size_notes (MtcsExpr *self,rtx_insn *prev, rtx_insn *last,poly_int64 end_args_size);
//原型 find_args_size_adjust rtl.h expr.cc
poly_int64 mtcs_expr_find_args_size_adjust (MtcsExpr *self,rtx_insn *insn);
//原型 expand_expr_real_gassign expr.h expr.cc
rtx mtcs_expr_expand_expr_real_gassign (MtcsExpr *self,gassign *g, rtx target, machine_mode tmode,
              enum expand_modifier modifier, rtx *alt_rtl=nullptr,bool inner_reference_p=false);
//原型 categorize_ctor_elements expr.h expr.cc
bool mtcs_expr_categorize_ctor_elements (MtcsExpr *self ,const_tree ctor, HOST_WIDE_INT *p_nz_elts,
              HOST_WIDE_INT *p_unique_nz_elts, HOST_WIDE_INT *p_init_elts, int *p_complete);

//原型 non_mem_decl_p expr.h expr.cc
bool mtcs_expr_non_mem_decl_p (MtcsExpr *self,tree base);
//原型 mem_ref_refers_to_non_mem_p expr.h expr.cc
bool mtcs_expr_mem_ref_refers_to_non_mem_p (MtcsExpr *self,tree ref);
//原型 can_store_by_pieces expr.h expr.cc
bool mtcs_expr_can_store_by_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT len,
             by_pieces_constfn constfun,void *constfundata, unsigned int align, bool memsetp);
//原型 store_expr expr.h expr.cc
rtx mtcs_expr_store_expr (MtcsExpr *self,tree exp, rtx target, int call_param_p, bool nontemporal, bool reverse);
//原型 store_by_pieces expr.h expr.cc
rtx mtcs_expr_store_by_pieces(MtcsExpr *self,rtx to, unsigned HOST_WIDE_INT len,
         by_pieces_constfn constfun,void *constfundata, unsigned int align, bool memsetp,memop_ret retmode);

//原型 emit_storent_insn expr.h expr.cc
bool mtcs_expr_emit_storent_insn (MtcsExpr *self,rtx to, rtx from);
//原型 maybe_emit_group_store expr.h expr.cc
rtx mtcs_expr_maybe_emit_group_store (MtcsExpr *self,rtx x, tree type);
//原型 expand_assignment expr.h expr.cc
void mtcs_expr_expand_assignment(MtcsExpr *self,tree to, tree from, bool nontemporal);

//原型 set_storage_via_setmem expr.h expr.cc
bool mtcs_expr_set_storage_via_setmem (MtcsExpr *self,rtx object, rtx size, rtx val, unsigned int align,
            unsigned int expected_align, HOST_WIDE_INT expected_size,
            unsigned HOST_WIDE_INT min_size,
            unsigned HOST_WIDE_INT max_size,
            unsigned HOST_WIDE_INT probable_max_size);

//原型 emit_move_complex_push expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_complex_push (MtcsExpr *self,machine_mode mode, rtx x, rtx y);
//原型 emit_move_complex_parts expr.h expr.cc
rtx_insn *mtcs_expr_emit_move_complex_parts(MtcsExpr *self,rtx x, rtx y);
//原型 move_by_pieces rtl.h expr.cc
rtx mtcs_expr_move_by_pieces(MtcsExpr *self,rtx to, rtx from, unsigned HOST_WIDE_INT len,
        unsigned int align, memop_ret retmode);
//原型 init_expr expr.h expr.cc
void mtcs_expr_init_expr (MtcsExpr *self);
//原型 init_expr_target expr.h expr.cc
void mtcs_expr_init_expr_target (MtcsExpr *self);
//原型 move_block_from_reg expr.h expr.cc
void mtcs_expr_move_block_from_reg (MtcsExpr *self,int regno, rtx x, int nregs);
//原型 maybe_optimize_mod_cmp expr.h expr.cc
enum tree_code mtcs_expr_maybe_optimize_mod_cmp (MtcsExpr *self,enum tree_code code, tree *arg0, tree *arg1);
//原型 safe_from_p expr.h expr.cc
bool mtcs_expr_safe_from_p (MtcsExpr *self,const_rtx x, tree exp, int top_p);
//原型 expand_operands expr.h expr.cc
void mtcs_expr_expand_operands (MtcsExpr *self,tree exp0, tree exp1, rtx target, rtx *op0, rtx *op1,
         enum expand_modifier modifier);
//原型 expand_expr_real_2 expr.h expr.cc
rtx mtcs_expr_expand_expr_real_2 (MtcsExpr *self,sepops ops, rtx target, machine_mode tmode,
        enum expand_modifier modifier);
//原型 write_complex_part expr.h expr.cc
void mtcs_expr_write_complex_part (MtcsExpr *self,rtx cplx, rtx val, bool imag_p, bool undefined_p);
//原型 try_casesi expr.h expr.cc
bool mtcs_expr_try_casesi (MtcsExpr *self,tree index_type, tree index_expr, tree minval, tree range,
        rtx table_label, rtx default_label, rtx fallback_label,
            profile_probability default_probability);

//原型 try_tablejump expr.h expr.cc
bool mtcs_expr_try_tablejump (MtcsExpr *self,tree index_type, tree index_expr, tree minval, tree range,
           rtx table_label, rtx default_label,profile_probability default_probability);
//原型 try_store_by_multiple_pieces expr.h builtins.cc
bool mtcs_expr_try_store_by_multiple_pieces (MtcsExpr *self,rtx to, rtx len, unsigned int ctz_len,
               unsigned HOST_WIDE_INT min_len, unsigned HOST_WIDE_INT max_len, rtx val, char valc, unsigned int align);
//原型 static bool can_store_by_multiple_pieces  builtins.cc
bool mtcs_expr_can_store_by_multiple_pieces (MtcsExpr *self,unsigned HOST_WIDE_INT bits,
               by_pieces_constfn constfun,void *constfundata, unsigned int align,
               bool memsetp, unsigned HOST_WIDE_INT len);
//原型 emit_block_cmp_hints expr.h expr.cc
rtx mtcs_expr_emit_block_cmp_hints (MtcsExpr *self,rtx x, rtx y, rtx len, tree len_type, rtx target,
            bool equality_only, by_pieces_constfn y_cfn, void *y_cfndata, unsigned ctz_len);
//原型 expand_cmpstrn_or_cmpmem expr.h expr.cc
rtx mtcs_expr_expand_cmpstrn_or_cmpmem (MtcsExpr *self,insn_code icode, rtx target, rtx arg1_rtx,
           rtx arg2_rtx, tree arg3_type, rtx arg3_rtx,HOST_WIDE_INT align);
//原型 get_inner_reference tree.h expr.cc
tree mtcs_expr_get_inner_reference (MtcsExpr *self,tree exp, poly_int64 *pbitsize,
           poly_int64 *pbitpos, tree *poffset,
           machine_mode *pmode, int *punsignedp,
           int *preversep, int *pvolatilep);
//原型 expand_crc_table_based expr.h expr.cc
void mtcs_expr_expand_crc_table_based (MtcsExpr *self,rtx op0, rtx op1, rtx op2, rtx op3,
         machine_mode data_mode);
//原型 expand_crc_table_based expr.h expr.cc
void mtcs_expr_expand_reversed_crc_table_based (MtcsExpr *self,rtx op0, rtx op1, rtx op2, rtx op3,
             machine_mode data_mode, void (*gen_reflecting_code) (rtx *op,void *userData),void *userData);
//原型 generate_reflecting_code_standard expr.h expr.cc
void mtcs_expr_generate_reflecting_code_standard (MtcsExpr *self,rtx *op);
//原型 get_personality_function expr.h expr.cc
rtx mtcs_expr_get_personality_function (MtcsExpr *self,tree decl);

#endif
