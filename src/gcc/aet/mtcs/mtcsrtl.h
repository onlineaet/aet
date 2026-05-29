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

#ifndef __GCC_MTCS_RTL__
#define __GCC_MTCS_RTL__

#include "../nlib.h"
#include "mtcsreg.h"
#include "mtcsmode.h"
#include "mtcsinterface.h"
#include "mtcsmicro.h"

//原型 struct GTY(()) target_rtl rtl.h
typedef struct _MtcsRTL MtcsRTL;
struct _MtcsRTL
{
        MtcsComponent parent;
        MtcsBackupRestore mtcsBackupRestore;//备份恢复接口
    /* All references to the hard registers in global_rtl_index go through
         these unique rtl objects.  On machines where the frame-pointer and
         arg-pointer are the same register, they use the same unique object.

         After register allocation, other rtl objects which used to be pseudo-regs
         may be clobbered to refer to the frame-pointer register.
         But references that were originally to the frame-pointer can be
         distinguished from the others because they contain frame_pointer_rtx.

         When to use frame_pointer_rtx and hard_frame_pointer_rtx is a little
         tricky: until register elimination has taken place hard_frame_pointer_rtx
         should be used if it is being set, and frame_pointer_rtx otherwise.  After
         register elimination hard_frame_pointer_rtx should always be used.
         On machines where the two registers are same (most) then these are the
         same.  */
      rtx x_global_rtl[20/*!GR_MAX*/];

      /* A unique representation of (REG:Pmode PIC_OFFSET_TABLE_REGNUM).  */
      rtx x_pic_offset_table_rtx;

      /* A unique representation of (REG:Pmode RETURN_ADDRESS_POINTER_REGNUM).
         This is used to implement __builtin_return_address for some machines;
         see for instance the MIPS port.  */
      rtx x_return_address_pointer_rtx;

      /* Commonly used RTL for hard registers.  These objects are not
         necessarily unique, so we allocate them separately from global_rtl.
         They are initialized once per compilation unit, then copied into
         regno_reg_rtx at the beginning of each function.  */
      rtx x_initial_regno_reg_rtx[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

      /* A sample (mem:M stack_pointer_rtx) rtx for each mode M.  */
      rtx x_top_of_stack[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

      /* Static hunks of RTL used by the aliasing code; these are treated
         as persistent to avoid unnecessary RTL allocations.  */
      rtx x_static_reg_base_value[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];

      /* The default memory attributes for each mode.  */
      class mem_attrs *x_mode_mem_attrs[(int)MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

      /* Track if RTL has been initialized.  */
      bool target_specific_initialized;

     rtx const_int_rtx[MAX_SAVED_CONST_INT * 2 + 1];

     /*原型 const0_rtx const1_rtx const2_rtx constm1_rtx rtl.h
#define const0_rtx (const_int_rtx[MAX_SAVED_CONST_INT])
#define const1_rtx  (const_int_rtx[MAX_SAVED_CONST_INT+1])
#define const2_rtx  (const_int_rtx[MAX_SAVED_CONST_INT+2])
#define constm1_rtx (const_int_rtx[MAX_SAVED_CONST_INT-1])
     */
     rtx mtcs_const0_rtx;
     rtx mtcs_const1_rtx;
     rtx mtcs_const2_rtx;
     rtx mtcs_constm1_rtx;

     GTY(()) rtx pc_rtx;  //原型 pc_rtx rtl.h
     GTY(()) rtx ret_rtx;  //原型 ret_rtx rtl.h
     GTY(()) rtx simple_return_rtx; //原型 simple_return_rtx rtl.h
     GTY(()) rtx_insn *invalid_insn_rtx; //原型 invalid_insn_rtx rtl.h

     //原型 const_true_rtx rtl.h
     //rtl.h 中const_true_rtx引用的地方太多,进入备份
     rtx const_true_rtx;

     //原型  const_tiny_rtx rtl.h
     GTY(()) rtx const_tiny_rtx[4][MAX_MAX_MACHINE_MODE/*!(int) MAX_MACHINE_MODE*/];
     //原型 fconst0 fconst1 fixed-value.h
     FIXED_VALUE_TYPE fconst0[MAX_FCONST0];
     FIXED_VALUE_TYPE fconst1[MAX_FCONST1];

     int rtl_initialized;//原型 toplev.cc
     //原型 generating_concat_p rtl.h rtl.cc
     int generating_concat_p;
     //原型 label_num emit-rtl.cc 缺省是1
     int label_num ;
     //原型 stack_limit_rtx rtl.h toplev.cc定义
     GTY(()) rtx stack_limit_rtx;


     NHashTable *const_int_htab;     //原型 static GTY ((cache)) hash_table<const_int_hasher> *const_int_htab; emit-rtl.cc
     NHashTable *const_wide_int_htab;//原型 static GTY ((cache)) hash_table<const_wide_int_hasher> *const_wide_int_htab;emit-rtl.cc
     NHashTable *const_poly_int_htab;//原型 static GTY ((cache)) hash_table<const_poly_int_hasher> *const_poly_int_htab;emit-rtl.cc
     NHashTable *reg_attrs_htab;//原型 static GTY ((cache)) hash_table<reg_attr_hasher> *reg_attrs_htab;emit-rtl.cc
     NHashTable *const_double_htab;//原型 static GTY ((cache)) hash_table<const_double_hasher> *const_double_htab;emit-rtl.cc
     NHashTable *const_fixed_htab;//原型 static GTY ((cache)) hash_table<const_fixed_hasher> *const_fixed_htab; emit-rtl.cc
    //原型 #define CONSTANT_ADDRESS_P(X)   (CONSTANT_P (X) && GET_CODE (X) != CONST_DOUBLE) defaults.h host nvptx实现不一样
     bool (*constant_address_p)(MtcsRTL *self,rtx x);
    //原型 #define SELECT_CC_MODE(OP, X, Y) ix86_cc_mode ((OP), (X), (Y))
     machine_mode (* select_cc_mode)(MtcsRTL *self,enum rtx_code code, rtx op0, rtx op1);
     /* Before the prologue, RA is at 0(%esp).  */
    //原型 #define INCOMING_RETURN_ADDR_RTX    gen_rtx_MEM (Pmode, stack_pointer_rtx) host=1 nvptx=0
     rtx (*incoming_return_addr_rtx)(MtcsRTL *self);
     //原型 regstack_completed rtl.h reg-stack.cc
     int regstack_completed;

     void *backup;//备份主机的rtl
};

/* The address space that the memory reference uses.  */
void mtcs_rtl_init(MtcsRTL *self);
void mtcs_rtl_init_emit_regs(MtcsRTL *self);
//原型 set_reg_attrs_for_decl_rtl emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_reg_attrs_for_decl_rtl (MtcsRTL *self,tree t, rtx x);
//原型 gen_int_mode emit-rtl.cc
rtx mtcs_rtl_gen_int_mode (MtcsRTL *self,poly_int64 c, machine_mode mode);
//来自rtl.h plus_constant
rtx mtcs_rtl_plus_constant (MtcsRTL *self,mtcs_mode mmode, rtx x, poly_int64 c, bool inplace=false);
//原型 gen_raw_REG rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_raw_REG (MtcsRTL *self,mtcs_mode mode, unsigned int regno);
/**
 * 下面10个方法是获取global_rtl中的rtx
 */
//原型 #define stack_pointer_rtx       (global_rtl[GR_STACK_POINTER]) rtl.h
rtx  mtcs_rtl_get_stack_pointer_rtx(MtcsRTL *self);
//原型 frame_pointer_rtx     (global_rtl[GR_FRAME_POINTER]) rtl.h
rtx  mtcs_rtl_get_frame_pointer_rtx(MtcsRTL *self);
//原型 #define arg_pointer_rtx     (global_rtl[GR_ARG_POINTER])
rtx  mtcs_rtl_get_arg_pointer_rtx(MtcsRTL *self);
//原型  #define hard_frame_pointer_rtx  (global_rtl[GR_HARD_FRAME_POINTER])
rtx  mtcs_rtl_get_hard_frame_pointer_rtx(MtcsRTL *self);
//原型 #define virtual_incoming_args_rtx       (global_rtl[GR_VIRTUAL_INCOMING_ARGS])
rtx  mtcs_rtl_get_virtual_incoming_args_rtx(MtcsRTL *self);
//原型 #define virtual_stack_vars_rtx          (global_rtl[GR_VIRTUAL_STACK_ARGS])
rtx  mtcs_rtl_get_virtual_stack_args_rtx(MtcsRTL *self);
//原型 #define virtual_stack_dynamic_rtx   (global_rtl[GR_VIRTUAL_STACK_DYNAMIC])
rtx  mtcs_rtl_get_virtual_stack_dynamic_rtx(MtcsRTL *self);
//原型 #define virtual_outgoing_args_rtx   (global_rtl[GR_VIRTUAL_OUTGOING_ARGS])
rtx  mtcs_rtl_get_virtual_outgoing_args_rtx(MtcsRTL *self);
//原型 #define virtual_cfa_rtx         (global_rtl[GR_VIRTUAL_CFA])
rtx  mtcs_rtl_get_virtual_cfa_rtx(MtcsRTL *self);
//原型 #define virtual_preferred_stack_boundary_rtx   (global_rtl[GR_VIRTUAL_PREFERRED_STACK_BOUNDARY])
rtx  mtcs_rtl_get_virtual_preferred_stack_boundary_rtx(MtcsRTL *self);
//原型 #define pic_offset_table_rtx   (this_target_rtl->x_pic_offset_table_rtx)
rtx  mtcs_rtl_get_pic_offset_table_rtx(MtcsRTL *self);

//原型 rtl.h get_mem_attrs
const class mem_attrs *mtcs_rtl_get_mem_attrs (MtcsRTL *self,const_rtx x);
//原型 immed_wide_int_const rtl.h emit-rtl.cc
rtx mtcs_rtl_immed_wide_int_const (MtcsRTL *self,const poly_wide_int_ref c, mtcs_mode mode);
rtx mtcs_rtl_gen_rtx_CONST_INT (MtcsRTL *self,mtcs_mode mode ATTRIBUTE_UNUSED, HOST_WIDE_INT arg);
//原型 GEN_INT rtl.h
rtx mtcs_rtl_GEN_INT(MtcsRTL *self,HOST_WIDE_INT m);
//原型 rtl.h rtlanal.cc  get_address_mode
scalar_int_mode mtcs_rtl_get_address_mode (MtcsRTL *self,rtx mem);
//原型 rtl.h emit-rtl.cc
rtx mtcs_rtl_adjust_address_1 (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset,
          int validate, int adjust_address, int adjust_object, poly_int64 size);
//原型 adjust_address_nv emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_adjust_address_nv (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset);
//原型 adjust_address emit-rtl.h emit-rtl.cc  adjust_address_1 (MEMREF, MODE, OFFSET, 1, 1, 0, 0)
rtx mtcs_rtl_adjust_address (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset);
//原型 const_double_from_real_value rtl.h emit-rtl.cc
rtx mtcs_rtl_const_double_from_real_value (MtcsRTL *self,REAL_VALUE_TYPE value, machine_mode mode);
//原型 const_fixed_from_fixed_value fixed-value.h
//原型 #define CONST_FIXED_FROM_FIXED_VALUE(r, m)  const_fixed_from_fixed_value (r, m)
rtx mtcs_rtl_const_fixed_from_fixed_value (MtcsRTL *self,FIXED_VALUE_TYPE value, machine_mode mode);
//原型subreg_get_info rtl.h rtlanal.cc
void mtcs_rtl_subreg_get_info (MtcsRTL *self,unsigned int xregno, machine_mode xmode,poly_uint64 offset, machine_mode ymode,struct subreg_info *info);
/* Generate a vector like gen_rtx_raw_CONST_VEC, but use the zero vector when
   all elements are zero, and the one vector when all elements are one.  */
//原型 gen_rtx_CONST_VECTOR emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_rtx_CONST_VECTOR (MtcsRTL *self,machine_mode mode, rtvec v);
//原型 MEM_ALIGN rtl.h
nuint   mtcs_rtl_get_mem_align(MtcsRTL *self,rtx x);
//原型 MEM_SIZE rtl.h
poly_int64  mtcs_rtl_get_mem_size(MtcsRTL *self,rtx x);
//原型 MEM_SIZE_KNOWN_P rtl.h
nboolean  mtcs_rtl_is_mem_size_known_p(MtcsRTL *self,rtx x);
//原型 MEM_ADDR_SPACE rtl.h
nuchar  mtcs_rtl_get_mem_addr_space(MtcsRTL *self,rtx x);
//原型 MEM_OFFSET rtl.h
poly_int64  mtcs_rtl_get_mem_offset(MtcsRTL *self,rtx x);
//原型 MEM_OFFSET_KNOWN_P rtl.h
nboolean  mtcs_rtl_is_mem_offset_known_p(MtcsRTL *self,rtx x);
//原型 MEM_ALIAS_SET rtl.h
alias_set_type mtcs_rtl_get_mem_alias(MtcsRTL *self,rtx x);
//原型 clear_mem_size emit-rtl.h emit-rtl.cc
void mtcs_rtl_clear_mem_size (MtcsRTL *self,rtx mem);
//原型 MEM_EXPR rtl.h
tree mtcs_rtl_get_mem_expr(MtcsRTL *self,rtx x);
//原型 subreg_memory_offset rtl.h emit-rtl.cc
poly_int64 mtcs_rtl_subreg_memory_offset (MtcsRTL *self,machine_mode outer_mode, machine_mode inner_mode,poly_uint64 offset);
//原型 rtl.h extern poly_int64 subreg_memory_offset (const_rtx); emit-rtl.cc 重载方法subreg_memory_offset
poly_int64 mtcs_rtl_subreg_memory_offset_with_rtx (MtcsRTL *self,const_rtx x);
//原型 validate_subreg rtl.h emit-rtl.cc
bool mtcs_rtl_validate_subreg (MtcsRTL *self,machine_mode omode, machine_mode imode,const_rtx reg, poly_uint64 offset);
//原型 gen_rtx_SUBREG rtl.h emit-rtl.cc
rtx  mtcs_rtl_gen_rtx_SUBREG (MtcsRTL *self,machine_mode mode, rtx reg, poly_uint64 offset);
//原型 gen_lowpart_SUBREG rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_lowpart_SUBREG (MtcsRTL *self,machine_mode mode, rtx reg);
//原型 const_vector_elt rtl.h emit-rtl.cc #define CONST_VECTOR_ELT(RTX, N) const_vector_elt (RTX, N)
rtx mtcs_rtl_const_vector_elt (MtcsRTL *self,const_rtx x, unsigned int i);
//原型 simplify_subreg_regno rtl.h rtlanal.cc
int mtcs_rtl_simplify_subreg_regno (MtcsRTL *self,unsigned int xregno, machine_mode xmode, poly_uint64 offset, machine_mode ymode);
//原型 gen_const_vec_duplicate emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_const_vec_duplicate (MtcsRTL *self,machine_mode mode, rtx elt);
//原型 get_mode_bounds rtl.h stor-layout.cc
void mtc_rtl_get_mode_bounds (MtcsRTL *self,scalar_int_mode mode, int sign, scalar_int_mode target_mode,rtx *mmin, rtx *mmax);
/* Return true if X is a paradoxical subreg, false otherwise.  */
//原型 paradoxical_subreg_p rtl.h 还有一个重载paradoxical_subreg_p(machine_mode m1)
nboolean mtcs_rtl_paradoxical_subreg_p (MtcsRTL *self,const_rtx x);
//原型 gen_int_shift_amount emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_int_shift_amount (MtcsRTL *self,machine_mode, poly_int64 value);
//原型 gen_highpart rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_highpart (MtcsRTL *self,machine_mode mode, rtx x);
//原型 gen_lowpart_if_possible rtl.h rtlhooks.cc
rtx mtcs_rtl_gen_lowpart_if_possible (MtcsRTL *self,machine_mode mode, rtx x);
//原型 operand_subword_force rtl.h emit-rtl.cc
rtx mtcs_rtl_operand_subword_force (MtcsRTL *self,rtx op, poly_uint64 offset, machine_mode mode);
//原型 operand_subword rtl.h emit-rtl.cc
rtx mtcs_rtl_operand_subword (MtcsRTL *self,rtx op, poly_uint64 offset, int validate_address,machine_mode mode);
//原型 replace_equiv_address_nv emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_replace_equiv_address_nv (MtcsRTL *self,rtx memref, rtx addr, bool inplace=false);
//原型 replace_equiv_address emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_replace_equiv_address (MtcsRTL *self,rtx memref, rtx addr, bool inplace=false);
//原型 virtual_stack_vars_rtx #define virtual_stack_vars_rtx  (global_rtl[GR_VIRTUAL_STACK_ARGS]) rtl.h
rtx mtcs_rtl_get_virtaul_stack_var_rtx (MtcsRTL *self);
//原型 change_address emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_change_address (MtcsRTL *self,rtx memref, machine_mode mode, rtx addr);

//原型 init_fake_stack_mems rtl.h reginfo.cc
void mtcs_rtl_init_fake_stack_mems (MtcsRTL *self);
//原型 rtl_initialized toplev.cc
void mtcs_rtl_initialize_rtl (MtcsRTL *self);
//原型 PUT_MODE rtl.h
void mtcs_rtl_put_mode(MtcsRTL *self,rtx x, machine_mode mode);
//原型 set_mem_size emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_size (MtcsRTL *self,rtx mem, poly_int64 size);
//原型 set_mem_alias_set emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_alias_set (MtcsRTL *self,rtx mem, alias_set_type set);
//原型set_mem_align emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_align (MtcsRTL *self,rtx mem, unsigned int align);
//原型 set_mem_expr emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_expr (MtcsRTL *self,rtx mem, tree expr);
//原型 set_mem_attributes emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_attributes(MtcsRTL *self,rtx ref, tree t, int objectp);
//原型 set_mem_attributes_minus_bitpos emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_attributes_minus_bitpos(MtcsRTL *self,rtx ref, tree t, int objectp,poly_int64 bitpos);

/* Return a memory reference like MEMREF, but with its mode changed
   to MODE and its address offset by OFFSET bytes.  Assume that it's
   for a bitfield and conservatively drop the underlying object if we
   cannot be sure to stay within its bounds.  */
//原型 #define adjust_bitfield_address(MEMREF, MODE, OFFSET)  adjust_address_1 (MEMREF, MODE, OFFSET, 1, 1, 1, 0) emit-rtl.h
inline rtx mtcs_rtl_adjust_bitfield_address (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset)
{
    return mtcs_rtl_adjust_address_1(self,memref,mode,offset,1,1,1,0);
}

/* As for adjust_bitfield_address, but specify that the width of
   BLKmode accesses is SIZE bytes.  */
//原型 #define adjust_bitfield_address_size(MEMREF, MODE, OFFSET, SIZE)  adjust_address_1 (MEMREF, MODE, OFFSET, 1, 1, 1, SIZE) emit-rtl.h
inline rtx mtcs_rtl_adjust_bitfield_address_size(MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset,poly_int64 size)
{
    return mtcs_rtl_adjust_address_1(self,memref,mode,offset,1,1,1,size);
}

//原型 mark_reg_pointer rtl.h emit-rtl.cc
void  mtcs_rtl_mark_reg_pointer (MtcsRTL *self,rtx reg, int align);
//原型 set_mem_addr_space emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_addr_space (MtcsRTL *self,rtx mem, addr_space_t addrspace);
//原型 HAVE_PRE_DECREMENT rtl.h
bool mtcs_rtl_have_pre_decrement(MtcsRTL *self);
//原型 HAVE_POST_DECREMENT rtl.h
bool mtcs_rtl_have_post_decrement(MtcsRTL *self);
//原型 offset_address emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_offset_address (MtcsRTL *self,rtx memref, rtx offset, unsigned HOST_WIDE_INT pow2);

//原型 adjust_automodify_address_1 emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_adjust_automodify_address_1 (MtcsRTL *self,rtx memref, machine_mode mode, rtx addr,
                 poly_int64 offset, int validate);


/* Return a memory reference like MEMREF, but with its mode changed
   to MODE and its address changed to ADDR, which is assumed to be
   increased by OFFSET bytes from MEMREF.  */
//原型 #define adjust_automodify_address(MEMREF, MODE, ADDR, OFFSET) adjust_automodify_address_1 (MEMREF, MODE, ADDR, OFFSET, 1) emit-rtl.h
inline rtx mtcs_rtl_adjust_automodify_address(MtcsRTL *self,rtx memref, machine_mode mode, rtx addr,
                 poly_int64 offset)
{
    return mtcs_rtl_adjust_automodify_address_1(self,memref,mode,addr,offset,1);
}

/* Likewise, but the reference is not required to be valid.  */
//原型 #define adjust_automodify_address_nv(MEMREF, MODE, ADDR, OFFSET) adjust_automodify_address_1 (MEMREF, MODE, ADDR, OFFSET, 0) emit-rtl.h
inline rtx mtcs_rtl_adjust_automodify_address_nv(MtcsRTL *self,rtx memref, machine_mode mode, rtx addr,
                 poly_int64 offset)
{
    return mtcs_rtl_adjust_automodify_address_1(self,memref,mode,addr,offset,0);
}

//原型 init_emit_once rtl.h emit-rtl.cc
void mtcs_rtl_init_emit_once (MtcsRTL *self);

//原型 max_label_num rtl.h emit-rtl.cc label_num原型定义在emit-rtl.cc
int mtcs_rtl_get_label_num(MtcsRTL *self);
void mtcs_rtl_set_label_num(MtcsRTL *self,int value);
//原型 max_label_num rtl.h emit-rtl.cc
int mtcs_rtl_max_label_num (MtcsRTL *self);
//原型 set_mode_and_regno rtl.h emit-rtl.cc
void mtcs_rtl_set_mode_and_regno (MtcsRTL *self,rtx x, mtcs_mode mode, unsigned int regno);
//原型 stack_limit_rtx rtl.h toplev.cc
rtx mtcs_rtl_get_stack_limit_rtx(MtcsRTL *self);
//原型 set_mem_offset emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_offset (MtcsRTL *self,rtx mem, poly_int64 offset);
//原型 set_reg_attrs_from_value emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_reg_attrs_from_value (MtcsRTL *self,rtx reg, rtx x);
//原型 set_reg_attrs_for_parm emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_reg_attrs_for_parm (MtcsRTL *self,rtx parm_rtx, rtx mem);
//原型 set_decl_incoming_rtl emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_decl_incoming_rtl (MtcsRTL *self,tree t, rtx x, bool by_reference_p);
//原型 subreg_lowpart_p rtl.h emit-rtl.cc
bool mtcs_rtl_subreg_lowpart_p (MtcsRTL *self,const_rtx x);
//原型 partial_subreg_p rtl.h rtl.h
bool mtcs_rtl_partial_subreg_p (MtcsRTL *self,const_rtx x);
//原型 gen_label_rtx rtl.h emit-rtl.cc 由于label_num是全局的
rtx_code_label *mtcs_rtl_gen_label_rtx (MtcsRTL *self);
//原型 rtx_code load_extend_op rtl.h
rtx_code mtcs_rtl_load_extend_op (MtcsRTL *self,machine_mode mode);
//原型 clear_mem_offset emit-rtl.h emit-rtl.cc
void mtcs_rtl_clear_mem_offset (MtcsRTL *self,rtx mem);
//原型 set_decl_rtl tree.h emit-rtl.cc
//#define SET_DECL_RTL(NODE, RTL) set_decl_rtl (NODE, RTL)
void mtcs_rtl_set_decl_rtl (MtcsRTL *self,tree t, rtx x);
//原型 gen_rtx_REG rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_rtx_REG (MtcsRTL *self,machine_mode mode, unsigned int regno);
//原型 reg_scan rtl.h reginfo.cc
void mtcs_rtl_reg_scan (MtcsRTL *self,rtx_insn *f, unsigned int nregs ATTRIBUTE_UNUSED);
//原型 #define CONSTANT_ADDRESS_P(X)   (CONSTANT_P (X) && GET_CODE (X) != CONST_DOUBLE) defaults.h host nvptx实现不一样
bool mtcs_rtl_constant_address_p(MtcsRTL *self,rtx x);
//原型 set_dst_reg_note rtl.h emit-rtl.cc
rtx mtcs_rtl_set_dst_reg_note (MtcsRTL *self,rtx insn, enum reg_note kind, rtx datum, rtx dst);
//原型 set_unique_reg_note rtl.h emit-rtl.cc
rtx mtcs_rtl_set_unique_reg_note (MtcsRTL *self,rtx insn, enum reg_note kind, rtx datum);
//原型 get_spill_slot_decl emit-rtl.h emit-rtl.cc
tree mtcs_rtl_get_spill_slot_decl (MtcsRTL *self,bool force_build_p);
//原型 set_insn_deleted rtl.h emit-rtl.cc SET_INSN_DELETED 宏指向 set_insn_deleted
void mtcs_rtl_set_insn_deleted (MtcsRTL *self,rtx_insn *insn);
//原型 unshare_all_rtl_in_chain rtl.h emit-rtl.cc
void mtcs_rtl_unshare_all_rtl_in_chain (MtcsRTL *self,rtx_insn *insn);
//原型 copy_rtx_if_shared rtl.h emit-rtl.cc
rtx mtcs_rtl_copy_rtx_if_shared (MtcsRTL *self,rtx orig);
//原型 reorder_insns rtl.h emit-rtl.cc
void mtcs_rtl_reorder_insns (MtcsRTL *self,rtx_insn *from, rtx_insn *to, rtx_insn *after);
//原型 reorder_insns_nobb rtl.h emit-rtl.cc
void mtcs_rtl_reorder_insns_nobb (MtcsRTL *self,rtx_insn *from, rtx_insn *to, rtx_insn *after);
//原型 #define SELECT_CC_MODE(OP, X, Y) ix86_cc_mode ((OP), (X), (Y))
 machine_mode mtcs_rtl_select_cc_mode (MtcsRTL *self,enum rtx_code code, rtx op0, rtx op1);
 //原型 adjust_reg_mode emit-rtl.h emit-rtl.cc
void mtcs_rtl_adjust_reg_mode (MtcsRTL *self,rtx reg, machine_mode mode);
//原型 #define INCOMING_RETURN_ADDR_RTX    gen_rtx_MEM (Pmode, stack_pointer_rtx) host=1 nvptx=0
rtx mtcs_rtl_incoming_return_addr_rtx (MtcsRTL *self);
//原型 regstack_completed rtl.h reg-stack.cc
int mtcs_rtl_get_regstack_completed(MtcsRTL *self);
void mtcs_rtl_set_regstack_completed (MtcsRTL *self,int value);
//原型 gen_reg_rtx_and_attrs rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_reg_rtx_and_attrs (MtcsRTL *self,rtx x);
//原型 gen_lowpart_common rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_lowpart_common (MtcsRTL *self,machine_mode mode, rtx x);


#endif

