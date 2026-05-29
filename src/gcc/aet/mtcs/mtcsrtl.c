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
 * base on rtl.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "memmodel.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "df.h"
#include "tm_p.h"
#include "stringpool.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "alias.h"
#include "fold-const.h"
#include "varasm.h"
#include "cfgrtl.h"
#include "tree-eh.h"
#include "explow.h"
#include "expr.h"
#include "builtins.h"
#include "rtl-iter.h"
#include "stor-layout.h"
#include "opts.h"
#include "optabs.h"
#include "predict.h"
#include "rtx-vector-builder.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "gimplify.h"
#include "expmed.h"
#include "rtlhooks-def.h"
#include "function-abi.h"

#include "aet/aetprinttree.h"
#include "mtcsrtl.h"
#include "mtcstarget.h"
#include "mtcsmicro.h"
#include "mtcsreg.h"
#include "mtcsvectorbuilder.h"
#include "mtcsasm.h"
#include "mtcscompile.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"
#include "rtl/mtcsira.h"
#include "mtcsprintrtl.h"


typedef struct _MtcsRTLBackup
{
       //备份的内容
    rtx b_const_int_rtx[MAX_SAVED_CONST_INT * 2 + 1];
    rtx b_const_tiny_rtx[4][(int) MAX_MACHINE_MODE];
    rtx b_pc_rtx;
    rtx b_ret_rtx;
    rtx b_simple_return_rtx;
    rtx_insn *b_invalid_insn_rtx;
    rtx b_x_global_rtl[GR_MAX]; //原型 x_global_rtl #define global_rtl  (this_target_rtl->x_global_rtl)rtl.h
    rtx b_const_true_rtx ;//原型  rtl.h  const_true_rtx;
}MtcsRTLBackup;

/* Index labels for global_rtl.  */
//原型 global_rtl_index rtl.h
typedef enum mtcs_global_rtl_index
{
  MTCS_GR_STACK_POINTER,
  MTCS_GR_FRAME_POINTER,
  MTCS_GR_ARG_POINTER,// = GR_FRAME_POINTER,
  MTCS_GR_HARD_FRAME_POINTER,// = GR_FRAME_POINTER,
  MTCS_GR_VIRTUAL_INCOMING_ARGS,
  MTCS_GR_VIRTUAL_STACK_ARGS,
  MTCS_GR_VIRTUAL_STACK_DYNAMIC,
  MTCS_GR_VIRTUAL_OUTGOING_ARGS,
  MTCS_GR_VIRTUAL_CFA,
  MTCS_GR_VIRTUAL_PREFERRED_STACK_BOUNDARY,

  MTCS_GR_MAX
};

static reg_attrs *get_reg_attrs (MtcsRTL *self,tree decl, poly_int64 offset);

static   void backup_cb(MtcsBackupRestore *iface);
static   void restore_cb(MtcsBackupRestore *iface);

static nuint constIntHash_cb(nconstpointer v)
{
  rtx x=(rtx)v;
  //printf("testHash hash nconstpointer %p %p %d\n",x,v,INTVAL (x));
  return (hashval_t) INTVAL (x);
}

static nboolean constIntHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    rtx x=(rtx)v1;
    HOST_WIDE_INT y=*((HOST_WIDE_INT *)v2);
//    n_debug("mtcsrtl.c rtx_equal testEqual nconstpointer %p %p %p %p "
//          HOST_WIDE_INT_PRINT_UNSIGNED" v2:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",v1,v2,x,y,INTVAL (x),y);
    return (INTVAL (x) ==y);
}

static void createTestHash()
{
   NHashTable *testHash = n_hash_table_new_full(constIntHash_cb, constIntHashEqual_cb,NULL, NULL);
   rtx x = gen_rtx_ASM_INPUT (VOIDmode, "");
   MEM_VOLATILE_P (x) = true;
   printf("testHash is -----%p %d\n",x,INTVAL (x));
   n_hash_table_insert(testHash,x,x);
   rtx ret=(rtx)n_hash_table_lookup(testHash,x);
   printf("查找key对应的value: key:%p value:%p\n",x,ret);
   nuint hash=INTVAL (x);

   ret=(rtx)n_hash_table_lookup_by_hash(testHash,&hash,hash);
   printf("用hash值查找 hash:%d value:%p\n",hash,ret);
}

//原型 hashval_t const_wide_int_hasher::hash (rtx x) emit-rtl.cc
static nuint constWideIntHash_cb(nconstpointer v)
{
  rtx x=(rtx)v;
  int i;
  unsigned HOST_WIDE_INT hash = 0;
  const_rtx xr = x;
  for (i = 0; i < CONST_WIDE_INT_NUNITS (xr); i++)
    hash += CONST_WIDE_INT_ELT (xr, i);
  return (nuint) hash;
}

/* Returns true if the value represented by X (which is really a
   CONST_WIDE_INT) is the same as that given by Y (which is really a
   CONST_WIDE_INT).  */
//原型 bool const_wide_int_hasher::equal (rtx x, rtx y) emit-rtl.cc
static nboolean constWideIntHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
  rtx x=(rtx)v1;
  rtx y=(rtx)v2;
  int i;
  const_rtx xr = x;
  const_rtx yr = y;
  if (CONST_WIDE_INT_NUNITS (xr) != CONST_WIDE_INT_NUNITS (yr))
    return FALSE;

  for (i = 0; i < CONST_WIDE_INT_NUNITS (xr); i++)
    if (CONST_WIDE_INT_ELT (xr, i) != CONST_WIDE_INT_ELT (yr, i))
      return FALSE;
  return TRUE;
}


static nuint constPolyIntHash_cb(nconstpointer v)
{
  rtx x=(rtx)v;
  inchash::hash h;
  h.add_int (GET_MODE (x));
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
    h.add_wide_int (CONST_POLY_INT_COEFFS (x)[i]);
  n_debug("mtcsrtl.c constPolyIntHash_cb rtx %p hash %d\n",x,h.end());
  return h.end ();
}

/* Returns true if CONST_POLY_INT X is an rtx representation of Y.  */
typedef struct _PolyIntCompareType
{
    machine_mode mode;
    poly_wide_int_ref ref;
}PolyIntCompareType;

static nboolean constPolyIntHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
  rtx x=(rtx)v1;
  PolyIntCompareType *y=(PolyIntCompareType *)v2;
  n_debug("mtcsrtl.c constPolyIntHashEqual_cb 00 rtx %p y:%p\n",x,y);
  if (GET_MODE (x) != y->mode)
    return false;
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
    if (CONST_POLY_INT_COEFFS (x)[i] != y->ref.coeffs[i])
      return FALSE;
  n_debug("mtcsrtl.c constPolyIntHashEqual_cb 11 rtx %p y:%p\n",x,y);

  return TRUE;
}

static void createPolyHash(MtcsRTL *self)
{
   NHashTable *testHash = n_hash_table_new_full(constPolyIntHash_cb, constPolyIntHashEqual_cb,NULL, NULL);
   unsigned int prec = 1;
   machine_mode mode=2;
   poly_int64 c=2;
   poly_wide_int newc = poly_wide_int::from (c, prec, SIGNED);

   typedef trailing_wide_ints<NUM_POLY_INT_COEFFS> twi;
   size_t extra_size = twi::extra_size (prec);
   rtx  x = rtx_alloc_v (CONST_POLY_INT, sizeof (struct const_poly_int_def) + extra_size);
   mtcs_rtl_put_mode/*!PUT_MODE*/(self,x, mode);
   CONST_POLY_INT_COEFFS (x).set_precision (prec);
   for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
       CONST_POLY_INT_COEFFS (x)[i] = newc.coeffs[i];

   n_debug("mtcsrtl.c createPolyHash is 00 x:%p INTVAL (x):%d\n",x,INTVAL (x));
   n_hash_table_insert(testHash,x,x);
   rtx ret=(rtx)n_hash_table_lookup(testHash,x);
   n_debug("mtcsrtl.c createPolyHash is 11 查找key对应的value: key:%p value:%p\n",x,ret);

   inchash::hash h;
   h.add_int (mode);
   for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
     h.add_wide_int (newc.coeffs[i]);
   PolyIntCompareType key={mode,newc};
   ret=(rtx)n_hash_table_lookup_by_hash(testHash,&key,h.end ());
   n_debug("mtcsrtl.c createPolyHash 22 用hash值查找 hash:%d value:%p\n",h.end (),ret);
}

/* Returns a hash code for X (which is a really a reg_attrs *).  */
//原型 hashval_t reg_attr_hasher::hash (reg_attrs *x) emit-rtl.cc
static nuint regAttrHash_cb(nconstpointer v1)
{
  reg_attrs *x=(reg_attrs *)v1;
  const reg_attrs *const p = x;
  inchash::hash h;
  h.add_ptr (p->decl);
  h.add_poly_hwi (p->offset);
  return h.end ();
}

/* Update NEW with the same attributes as REG, but with OFFSET added
   to the REG_OFFSET.  */
//原型 update_reg_offset emit-rtl.cc
static void update_reg_offset (MtcsRTL *self,rtx new_rtx, rtx reg, poly_int64 offset)
{
  REG_ATTRS (new_rtx) = get_reg_attrs (self,REG_EXPR (reg), REG_OFFSET (reg) + offset);
}

/* Returns true if the value represented by X  is the same as that given by
   Y.  */
//原型 bool reg_attr_hasher::equal (reg_attrs *x, reg_attrs *y) emit-rtl.cc
static nboolean regAttrHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
  reg_attrs *x=(reg_attrs *)v1;
  reg_attrs *y=(reg_attrs *)v2;
  const reg_attrs *const p = x;
  const reg_attrs *const q = y;
  return (p->decl == q->decl && known_eq (p->offset, q->offset));
}

/* Returns a hash code for X (which is really a CONST_DOUBLE).  */
//原型 hashval_t const_double_hasher::hash (rtx x) emit-rtl.cc
static nuint constDoubleHash_cb(nconstpointer v)
{
  rtx x=(rtx)v;
  const_rtx const value = x;
  hashval_t h;
  if (TARGET_SUPPORTS_WIDE_INT == 0 && GET_MODE (value) == VOIDmode)
    h = CONST_DOUBLE_LOW (value) ^ CONST_DOUBLE_HIGH (value);
  else{
      h = real_hash (CONST_DOUBLE_REAL_VALUE (value));
      /* MODE is used in the comparison, so it should be in the hash.  */
      h ^= GET_MODE (value);
  }
  return (nuint)h;
}

/* Returns true if the value represented by X (really a ...)
   is the same as that represented by Y (really a ...) */
//原型 bool  const_double_hasher::equal (rtx x, rtx y) emit-rtl.cc

static nboolean constDoubleHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
  rtx x=(rtx)v1;
  rtx y=(rtx)v2;
  const_rtx const a = x, b = y;
  if (GET_MODE (a) != GET_MODE (b))
    return false;
  if (TARGET_SUPPORTS_WIDE_INT == 0 && GET_MODE (a) == VOIDmode)
    return (CONST_DOUBLE_LOW (a) == CONST_DOUBLE_LOW (b) && CONST_DOUBLE_HIGH (a) == CONST_DOUBLE_HIGH (b));
  else
    return real_identical (CONST_DOUBLE_REAL_VALUE (a), CONST_DOUBLE_REAL_VALUE (b));
}

/* Returns a hash code for X (which is really a CONST_FIXED).  */
//原型 hashval_t const_fixed_hasher::hash (rtx x) emit-rtl.cc
static nuint constFixedHash_cb(nconstpointer v)
{
  rtx x=(rtx)v;
  const_rtx const value = x;
  hashval_t h;
  h = fixed_hash (CONST_FIXED_VALUE (value));
  /* MODE is used in the comparison, so it should be in the hash.  */
  h ^= GET_MODE (value);
  return h;
}

/* Returns true if the value represented by X is the same as that
   represented by Y.  */
//原型 bool const_fixed_hasher::equal (rtx x, rtx y) emit-rtl.cc
static nboolean constFixedHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
  rtx x=(rtx)v1;
  rtx y=(rtx)v2;
  const_rtx const a = x, b = y;
  if (GET_MODE (a) != GET_MODE (b))
    return false;
  return fixed_identical (CONST_FIXED_VALUE (a), CONST_FIXED_VALUE (b));
}


static void mtcsRTLInit(MtcsRTL *self)
{
    self->mtcsBackupRestore.backup=backup_cb;
    self->mtcsBackupRestore.restore=restore_cb;
    self->mtcsBackupRestore.impl=(npointer)self;
    self->backup=(void*) n_slice_alloc0 (sizeof(MtcsRTLBackup));
    ////原型 static GTY ((cache)) hash_table<const_int_hasher> *const_int_htab; emit-rtl.cc
    self->const_int_htab = n_hash_table_new_full(constIntHash_cb, constIntHashEqual_cb,NULL, NULL);
    //原型  static GTY ((cache)) hash_table<const_wide_int_hasher> *const_wide_int_htab;
    self->const_wide_int_htab = n_hash_table_new_full(constWideIntHash_cb, constWideIntHashEqual_cb,NULL, NULL);
    //原型 static GTY ((cache)) hash_table<const_poly_int_hasher> *const_poly_int_htab;emit-rtl.cc
    self->const_poly_int_htab = n_hash_table_new_full(constPolyIntHash_cb, constPolyIntHashEqual_cb,NULL, NULL);
    //原型 static GTY ((cache)) hash_table<reg_attr_hasher> *reg_attrs_htab;emit-rtl.cc
    self->reg_attrs_htab = n_hash_table_new_full(regAttrHash_cb, regAttrHashEqual_cb,NULL, NULL);
    //原型 static GTY ((cache)) hash_table<const_double_hasher> *const_double_htab;emit-rtl.cc
    self->const_double_htab = n_hash_table_new_full(constDoubleHash_cb, constDoubleHashEqual_cb,NULL, NULL);
    //原型 static GTY ((cache)) hash_table<const_fixed_hasher> *const_fixed_htab; emit-rtl.cc
    self->const_fixed_htab = n_hash_table_new_full(constFixedHash_cb, constFixedHashEqual_cb,NULL, NULL);
    //原型 generating_concat_p rtl.h rtl.cc
    self->generating_concat_p=0;
    //原型 label_num emit-rtl.cc 缺省是1
    self->label_num = 1;
    //原型 regstack_completed rtl.h reg-stack.cc
    self->regstack_completed= 0;
    //createTestHash();
    createPolyHash(self);
}

/* Generate a new vector constant for mode MODE and constant value
   CONSTANT.  */
//原型 gen_const_vector emit-rtl.cc
static rtx gen_const_vector (MtcsRTL *self,machine_mode mode, int constant)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  machine_mode inner = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
  gcc_assert (!mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,inner));
  rtx el = self->const_tiny_rtx[constant][(int) inner];
  gcc_assert (el);
  return mtcs_rtl_gen_const_vec_duplicate/*!gen_const_vec_duplicate*/(self,mode, el);
}

/* Set the mode and register number of X to MODE and REGNO.  */
//原型 set_mode_and_regno rtl.h emit-rtl.cc
void mtcs_rtl_set_mode_and_regno (MtcsRTL *self,rtx x, mtcs_mode mode, unsigned int regno)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  /*!溶合在mtcs_reg_get_reg_nums
  unsigned int nregs = (HARD_REGISTER_NUM_P (regno)
            ? hard_regno_nregs (regno, mode)
            : 1);
  */
  nuint nregs = mtcs_reg_get_reg_nums(mtcsReg,mode,regno);/*hard_regno_nregs*/
  PUT_MODE_RAW (x, mode);
  set_regno_raw (x, regno, nregs);
}

/* Initialize a fresh REG rtx with mode MODE and register REGNO.  */

static rtx mtcs_rtl_init_raw_REG (MtcsRTL *self,rtx x, mtcs_mode mode, unsigned int regno)
{
  mtcs_rtl_set_mode_and_regno (self,x, mode, regno);
  REG_ATTRS (x) = NULL;
  ORIGINAL_REGNO (x) = regno;
  return x;
}

//原型 gen_raw_REG rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_raw_REG (MtcsRTL *self,mtcs_mode mode, unsigned int regno)
{
  rtx x = rtx_alloc (REG MEM_STAT_INFO);
  mtcs_rtl_init_raw_REG (self,x, mode, regno);
  return x;
}

//原型 PUT_MODE rtl.h
void mtcs_rtl_put_mode(MtcsRTL *self,rtx x, machine_mode mode)
{
  if (REG_P (x))
      mtcs_rtl_set_mode_and_regno/*!set_mode_and_regno*/(self,x, mode, REGNO (x));
  else
     PUT_MODE_RAW (x, mode);
}

/**
 * pmode的每个平台不一样，这里的pmode是主机，所以需要用平台的Pmode M_Pmode
 * 原型   init_emit_regs (void)  rtl.h emit-rtl.cc
 */
void mtcs_rtl_init_emit_regs(MtcsRTL *self)
{
     MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
     MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
     MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
     MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
     MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);

     mtcs_mode pmode=mtcs_mode_get_Pmode(mtcsMode);
     mem_attrs *attrs;
     n_hash_table_remove_all/*!reg_attrs_htab->empty ()*/(self->reg_attrs_htab);
     //原型 init_reg_modes_target rtl.h reginfo.cc
     mtcs_reg_init_reg_modes_target(mtcsReg);
    /* Assign register numbers to the globally defined register rtx.  */
     self->x_global_rtl/* stack_pointer_rtx*/[MTCS_GR_STACK_POINTER] = mtcs_rtl_gen_raw_REG (self,pmode,
             mtcsReg->normalHardRegsNum.stack_pointer_regnum/*STACK_POINTER_REGNUM*/);
     self->x_global_rtl/* frame_pointer_rtx*/[MTCS_GR_FRAME_POINTER] = mtcs_rtl_gen_raw_REG (self,pmode,
                mtcsReg->normalHardRegsNum.frame_pointer_regnum/*FRAME_POINTER_REGNUM*/);
     self->x_global_rtl/* hard_frame_pointer_rtx*/[MTCS_GR_HARD_FRAME_POINTER] = mtcs_rtl_gen_raw_REG (self,pmode,
                mtcsReg->normalHardRegsNum.hard_frame_pointer_regnum/*HARD_FRAME_POINTER_REGNUM*/);
     self->x_global_rtl/* arg_pointer_rtx*/[MTCS_GR_ARG_POINTER] = mtcs_rtl_gen_raw_REG (self,pmode,
                mtcsReg->normalHardRegsNum.arg_pointer_regnum/*ARG_POINTER_REGNUM*/);
     self->x_global_rtl/* virtual_incoming_args_rtx*/[MTCS_GR_VIRTUAL_INCOMING_ARGS] = mtcs_rtl_gen_raw_REG (self,pmode,
                mtcsReg->normalHardRegsNum.virtual_incoming_args_regnum/*VIRTUAL_INCOMING_ARGS_REGNUM*/);
     self->x_global_rtl/* virtual_stack_vars_rtx*/[MTCS_GR_VIRTUAL_STACK_ARGS] = mtcs_rtl_gen_raw_REG (self,pmode,
                 mtcsReg->normalHardRegsNum.virtual_stack_vars_regnum/*VIRTUAL_STACK_VARS_REGNUM*/);
     self->x_global_rtl/* virtual_stack_dynamic_rtx*/[MTCS_GR_VIRTUAL_STACK_DYNAMIC] = mtcs_rtl_gen_raw_REG (self,pmode,
                 mtcsReg->normalHardRegsNum.virtual_stack_dynamic_regnum/*VIRTUAL_STACK_DYNAMIC_REGNUM*/);
     self->x_global_rtl/* virtual_outgoing_args_rtx*/[MTCS_GR_VIRTUAL_OUTGOING_ARGS] = mtcs_rtl_gen_raw_REG (self,pmode,
                 mtcsReg->normalHardRegsNum.virtual_outgoing_args_regnum/*VIRTUAL_OUTGOING_ARGS_REGNUM*/);
     self->x_global_rtl/* virtual_cfa_rtx*/[MTCS_GR_VIRTUAL_CFA] = mtcs_rtl_gen_raw_REG (self,pmode,
                 mtcsReg->normalHardRegsNum.virtual_cfa_regnum/*VIRTUAL_CFA_REGNUM*/);
     self->x_global_rtl/* virtual_preferred_stack_boundary_rtx*/[MTCS_GR_VIRTUAL_PREFERRED_STACK_BOUNDARY] = mtcs_rtl_gen_raw_REG (self,pmode,
                 mtcsReg->normalHardRegsNum.virtual_preferred_stack_boundary_regnum/*VIRTUAL_PREFERRED_STACK_BOUNDARY_REGNUM*/);


    n_debug("mtcsrtl.c  ----22 GR_STACK_POINTER:%d %d\n",GR_STACK_POINTER,mtcsReg->normalHardRegsNum.stack_pointer_regnum);
    n_debug("mtcsrtl.c  ----22 GR_FRAME_POINTER:%d %d\n",GR_FRAME_POINTER,mtcsReg->normalHardRegsNum.frame_pointer_regnum);
    n_debug("mtcsrtl.c  ----22 GR_ARG_POINTER:%d %d\n",GR_ARG_POINTER,mtcsReg->normalHardRegsNum.arg_pointer_regnum);
    n_debug("mtcsrtl.c  ----22 GR_HARD_FRAME_POINTER:%d %d\n",GR_HARD_FRAME_POINTER,mtcsReg->normalHardRegsNum.hard_frame_pointer_regnum);
    n_debug("mtcsrtl.c  ----22 GR_VIRTUAL_INCOMING_ARGS:%d %d\n",GR_VIRTUAL_INCOMING_ARGS,mtcsReg->normalHardRegsNum.virtual_incoming_args_regnum);
    n_debug("mtcsrtl.c  ----22 GR_VIRTUAL_STACK_ARGS:%d %d\n",GR_VIRTUAL_STACK_ARGS,mtcsReg->normalHardRegsNum.virtual_stack_vars_regnum);
    n_debug("mtcsrtl.c  ----22 GR_VIRTUAL_STACK_DYNAMIC:%d %d\n",GR_VIRTUAL_STACK_DYNAMIC,mtcsReg->normalHardRegsNum.virtual_stack_dynamic_regnum);
    n_debug("mtcsrtl.c  ----22 GR_VIRTUAL_OUTGOING_ARGS:%d %d\n",GR_VIRTUAL_OUTGOING_ARGS,mtcsReg->normalHardRegsNum.virtual_outgoing_args_regnum);
    n_debug("mtcsrtl.c  ----22 GR_VIRTUAL_CFA:%d %d\n",GR_VIRTUAL_CFA,mtcsReg->normalHardRegsNum.virtual_cfa_regnum);
    n_debug("mtcsrtl.c  ----22 GR_VIRTUAL_PREFERRED_STACK_BOUNDARY:%d %d\n",
             GR_VIRTUAL_PREFERRED_STACK_BOUNDARY,mtcsReg->normalHardRegsNum.virtual_preferred_stack_boundary_regnum);
     int i;
     nuint   hardRegsCount= mtcs_reg_get_hard_reg_count(mtcsReg);

     for(i=0;i<hardRegsCount;i++){
         self->x_initial_regno_reg_rtx[i] = mtcs_rtl_gen_raw_REG (self,mtcsReg->hardRegs.x_reg_raw_mode[i], i);
     }

   if(mtcsReg->return_address_pointer_regnum>0){
     n_debug("mtcsrtl.c  ----33 RETURN_ADDRESS_POINTER_REGNUM\n");
      self->x_return_address_pointer_rtx = mtcs_rtl_gen_raw_REG (self,pmode, mtcsReg->return_address_pointer_regnum/*RETURN_ADDRESS_POINTER_REGNUM*/);
   }
   self->x_pic_offset_table_rtx=NULL_RTX;
   if ((unsigned) mtcsReg->pic_offset_table_regnum/*PIC_OFFSET_TABLE_REGNUM*/ != INVALID_REGNUM){
       n_debug("mtcsrtl.c ----55xx PIC_OFFSET_TABLE_REGNUM:%d INVALID_REGNUM:%d\n",(unsigned) PIC_OFFSET_TABLE_REGNUM,INVALID_REGNUM);
       self->x_pic_offset_table_rtx = mtcs_rtl_gen_raw_REG (self,pmode,mtcsReg->pic_offset_table_regnum/*PIC_OFFSET_TABLE_REGNUM*/);
   }

   /* Process stack-limiting command-line options.  */
   if (mtcsOpts->opt_fstack_limit_symbol_arg != NULL)
     self->stack_limit_rtx  = gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), ggc_strdup (mtcsOpts->opt_fstack_limit_symbol_arg));
   if (mtcsOpts->opt_fstack_limit_register_no >= 0)
     self->stack_limit_rtx =mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(self,
           mtcs_mode_get_Pmode(mtcsMode), mtcsOpts->opt_fstack_limit_register_no);

   int maxMode= mtcs_mode_get_max_number(mtcsMode);
   for (i = 0; i < maxMode; i++){
        mtcs_mode mode = (mtcs_mode) i;
        attrs =  ggc_cleared_alloc<mem_attrs> ();
        //attrs = n_slice_alloc0(sizeof(mem_attrs));///*! ggc_cleared_alloc*/<mem_attrs> ();

        attrs->align = BITS_PER_UNIT;
        attrs->addrspace = ADDR_SPACE_GENERIC;
        if (mode != mtcsMode->modes.M_BLKmode && mode != VOIDmode){ //BLKmode VOIDmode在每个平台都一样 BLKmode=1,VOIDmode=0
            attrs->size_known_p = true;
            attrs->size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
            if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign))
              attrs->align = mtcs_mode_get_alignment(mtcsMode,mode);//GET_MODE_ALIGNMENT (mode);
        }
        self->x_mode_mem_attrs[i] = attrs;
    }
}

/* Allocate a new reg_attrs structure and insert it into the hash table if
   one identical to it is not already in the table.  We are doing this for
   MEM of mode MODE.  */

static reg_attrs *get_reg_attrs (MtcsRTL *self,tree decl, poly_int64 offset)
{
  reg_attrs attrs;
  /* If everything is the default, we can just return zero.  */
  if (decl == 0 && known_eq (offset, 0))
    return 0;
  attrs.decl = decl;
  attrs.offset = offset;
  /*
  reg_attrs **slot = reg_attrs_htab->find_slot (&attrs, INSERT);
  if (*slot == 0){
      *slot = ggc_alloc<reg_attrs> ();
      memcpy (*slot, &attrs, sizeof (reg_attrs));
  }
  return *slot;
  */
  reg_attrs *slot = n_hash_table_lookup(self->reg_attrs_htab,&attrs);
  if (slot == NULL){
      slot = ggc_alloc<reg_attrs> ();
      memcpy (slot, &attrs, sizeof (reg_attrs));
      n_hash_table_insert(self->reg_attrs_htab,slot,slot);
  }
  return slot;

}

/* Set the REG_ATTRS for registers in value X, given that X represents
   decl T.  */
//原型 set_reg_attrs_for_decl_rtl emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_reg_attrs_for_decl_rtl (MtcsRTL *self,tree t, rtx x)
{
  if (!t)
    return;
  tree tdecl = t;
  if (GET_CODE (x) == SUBREG){
      gcc_assert (mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(self,x));
      x = SUBREG_REG (x);
  }
  if (REG_P (x))
    REG_ATTRS (x) = get_reg_attrs (self,t,
          byte_lowpart_offset (GET_MODE (x),DECL_P (tdecl)? DECL_MODE (tdecl): TYPE_MODE (TREE_TYPE (tdecl))));
  if (GET_CODE (x) == CONCAT){
      if (REG_P (XEXP (x, 0)))
        REG_ATTRS (XEXP (x, 0)) = get_reg_attrs (self,t, 0);
      if (REG_P (XEXP (x, 1)))
          REG_ATTRS (XEXP (x, 1))= get_reg_attrs (self,t, GET_MODE_UNIT_SIZE (GET_MODE (XEXP (x, 0))));
  }
  if (GET_CODE (x) == PARALLEL){
      int i, start;

      /* Check for a NULL entry, used to indicate that the parameter goes
     both on the stack and in registers.  */
      if (XEXP (XVECEXP (x, 0, 0), 0))
          start = 0;
      else
          start = 1;

      for (i = start; i < XVECLEN (x, 0); i++){
          rtx y = XVECEXP (x, 0, i);
          if (REG_P (XEXP (y, 0)))
            REG_ATTRS (XEXP (y, 0)) = get_reg_attrs (self,t, INTVAL (XEXP (y, 1)));
      }
  }
}

//原型 emit-rtl.cc gen_rtx_CONST_INT
rtx mtcs_rtl_gen_rtx_CONST_INT (MtcsRTL *self,mtcs_mode mode ATTRIBUTE_UNUSED, HOST_WIDE_INT arg)
{

  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  int storeFlagValue=mtcs_real_get_store_flag_value(mtcsReal);
  //n_debug("mtcsrtl.c mtcs_rtl_gen_rtx_CONST_INT  00 mode:%d arg:"HOST_WIDE_INT_PRINT_DEC" MAX_SAVED_CONST_INT:%d storeFlagValue:%d\n",
    //  mode,arg,MAX_SAVED_CONST_INT,storeFlagValue);
  if (arg >= - MAX_SAVED_CONST_INT && arg <= MAX_SAVED_CONST_INT){
   //n_debug("mtcsrtl.c mtcs_rtl_gen_rtx_CONST_INT  11 mode:%d arg:"HOST_WIDE_INT_PRINT_DEC" MAX_SAVED_CONST_INT:%d\n",mode,arg,MAX_SAVED_CONST_INT);

    return self->const_int_rtx[arg + MAX_SAVED_CONST_INT];
  }

//#if STORE_FLAG_VALUE != 1 && STORE_FLAG_VALUE != -1
//  if (const_true_rtx && arg == STORE_FLAG_VALUE)
//    return const_true_rtx;
//#endif
   if(storeFlagValue!=1 && storeFlagValue!=-1){
       if(self->const_true_rtx && arg==storeFlagValue){
          //n_debug("mtcsrtl.c mtcs_rtl_gen_rtx_CONST_INT  22 mode:%d arg:"HOST_WIDE_INT_PRINT_DEC" MAX_SAVED_CONST_INT:%d\n",mode,arg,MAX_SAVED_CONST_INT);

           return self->const_true_rtx;
       }
   }

  /* Look up the CONST_INT in the hash table.  */
   /*被n_hash_table_lookup_by_hash替换
  rtx *slot = const_int_htab->find_slot_with_hash (arg, (hashval_t) arg,INSERT);
  if (*slot == 0)
    *slot = gen_rtx_raw_CONST_INT (VOIDmode, arg);
  return *slot;
  */

  rtx slot = n_hash_table_lookup_by_hash(self->const_int_htab,&arg,arg);//->find_slot_with_hash (arg, (hashval_t) arg,INSERT);
  if(slot==NULL){
    // n_debug("mtcsrtl.c mtcs_rtl_gen_rtx_CONST_INT  33 mode:%d arg:"HOST_WIDE_INT_PRINT_DEC" MAX_SAVED_CONST_INT:%d\n",mode,arg,MAX_SAVED_CONST_INT);

      slot=gen_rtx_raw_CONST_INT (VOIDmode, arg);
      n_hash_table_insert(self->const_int_htab,slot,slot);
  }
  return slot;
}

//原型 GEN_INT rtl.h
rtx mtcs_rtl_GEN_INT(MtcsRTL *self,HOST_WIDE_INT m)
{
  return mtcs_rtl_gen_rtx_CONST_INT(self,VOIDmode,m);
}

/* Return a newly created CODE_LABEL rtx with a unique label number.  */
//原型 gen_label_rtx rtl.h emit-rtl.cc 由于label_num是全局的
rtx_code_label *mtcs_rtl_gen_label_rtx (MtcsRTL *self)
{
  return as_a <rtx_code_label *> (gen_rtx_CODE_LABEL (VOIDmode, NULL_RTX, NULL_RTX,NULL, self->label_num++, NULL));
}

/**
 * 原型 rtl.h emit-rtl.cc gen_int_mode
 */
rtx mtcs_rtl_gen_int_mode (MtcsRTL *self,poly_int64 c, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  c = mtcs_mode_trunc_int_for_mode_with_poly_int64 (mtcsMode,c, mode);
  if (c.is_constant ()){
    // n_debug("mtcsrtl.c gen_int_mode 00 mode:%d %d\n",mode,c.coeffs[0]);
    return mtcs_rtl_GEN_INT/*!GEN_INT*/(self,c.coeffs[0]);
  }
  unsigned int prec = mtcs_mode_get_precision/*GET_MODE_PRECISION*/(mtcsMode,mtcs_mode_as_a/*!as_a*/<scalar_mode> (mtcsMode,mode));
  poly_wide_int newc = poly_wide_int::from (c, prec, SIGNED);
  n_debug("mtcsrtl.c gen_int_mode 11 mode:%d prec:%u %d\n",mode,prec,newc);
  return mtcs_rtl_immed_wide_int_const (self,newc, mode);
}

/* Return an rtx for the sum of X and the integer C, given that X has
   mode MODE.  INPLACE is true if X can be modified inplace or false
   if it must be treated as immutable.  */
//原型  plus_constant rtl.h explow.cc inplace=false;
rtx mtcs_rtl_plus_constant (MtcsRTL *self,mtcs_mode mmode, rtx x, poly_int64 c, bool inplace)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

  RTX_CODE code;
  rtx y;
  rtx tem;
  int all_constant = 0;
  machine_mode mode=(machine_mode)mmode;
  gcc_assert (GET_MODE (x) == mtcsMode->modes.M_VOIDmode || GET_MODE (x) == mode);

  if (known_eq (c, 0))
    return x;

 restart:

  code = GET_CODE (x);
  y = x;

  switch (code){
    CASE_CONST_SCALAR_INT:
      return mtcs_rtl_immed_wide_int_const (self,wi::add (mtcs_rtx_mode_t/*!rtx_mode_t*/(x, mode), c), mode);
    case MEM:
      /* If this is a reference to the constant pool, try replacing it with
     a reference to a new constant.  If the resulting address isn't
     valid, don't return it because we have no way to validize it.  */
      if (GET_CODE (XEXP (x, 0)) == SYMBOL_REF && CONSTANT_POOL_ADDRESS_P (XEXP (x, 0))){
          rtx cst = get_pool_constant (XEXP (x, 0));
          if (GET_CODE (cst) == CONST_VECTOR  && GET_MODE_INNER (GET_MODE (cst)) == mode){
              cst = gen_lowpart (mode, cst);
              gcc_assert (cst);
          }else if (GET_MODE (cst) == mtcsMode->modes.M_VOIDmode && get_pool_mode (XEXP (x, 0)) != mode)
              break;
          if (GET_MODE (cst) == mtcsMode->modes.M_VOIDmode || GET_MODE (cst) == mode){
              tem = mtcs_rtl_plus_constant (self,(mtcs_mode)mode, cst, c,false);
              tem = mtcs_asm_force_const_mem/*!force_const_mem*/ (mtcsAsm,GET_MODE (x), tem);
              /* Targets may disallow some constants in the constant pool, thus
               force_const_mem may return NULL_RTX.  */
              if (tem && mtcs_recog_memory_address_p/*!memory_address_p*/(mtcsRecog,GET_MODE (tem), XEXP (tem, 0)))
                  return tem;
          }
      }
      break;

    case CONST:
      /* If adding to something entirely constant, set a flag
     so that we can add a CONST around the result.  */
      if (inplace && shared_const_p (x))
          inplace = false;
      x = XEXP (x, 0);
      all_constant = 1;
      goto restart;

    case SYMBOL_REF:
    case LABEL_REF:
      all_constant = 1;
      break;

    case PLUS:
      /* The interesting case is adding the integer to a sum.  Look
     for constant term in the sum and combine with C.  For an
     integer constant term or a constant term that is not an
     explicit integer, we combine or group them together anyway.

     We may not immediately return from the recursive call here, lest
     all_constant gets lost.  */

      if (CONSTANT_P (XEXP (x, 1))){
          rtx term = mtcs_rtl_plus_constant/*!plus_constant*/(self,mode, XEXP (x, 1), c, inplace);
          if (term == self->mtcs_const0_rtx)
            x = XEXP (x, 0);
          else if (inplace)
            XEXP (x, 1) = term;
          else
            x = gen_rtx_PLUS (mode, XEXP (x, 0), term);
          c = 0;
      }else if (rtx *const_loc = find_constant_term_loc (&y)){
          if (!inplace){
              /* We need to be careful since X may be shared and we can't
             modify it in place.  */
              x = copy_rtx (x);
              const_loc = find_constant_term_loc (&x);
          }
          *const_loc = mtcs_rtl_plus_constant (self,(mtcs_mode)mode, *const_loc, c, true);
          c = 0;
      }
      break;

    default:
      if (CONST_POLY_INT_P (x))
          return mtcs_rtl_immed_wide_int_const (self,const_poly_int_value (x) + c, mode);
      break;
  }

  if (maybe_ne (c, 0))
    x = gen_rtx_PLUS (mode, x, mtcs_rtl_gen_int_mode/*gen_int_mode*/ (self,c, mode));

  if (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == LABEL_REF)
    return x;
  else if (all_constant)
    return gen_rtx_CONST (mode, x);
  else
    return x;
}

//参见 https://blog.csdn.net/lidan113lidan/article/details/123961954
/*
+-------------------------------+ <- highmem
|                               |
|  incoming stack arguments     |
|                               |
+-------------------------------+ <-- incoming stack pointer (aligned)/virtual_incoming_args_rtx/arg_pointer_rtx
|                               |
|  callee-allocated save area   |
|  for register varargs         |
|                               |
+-------------------------------+ <-- virtual_stack_vars_rtx/frame_pointer_rtx
|  local variables              |
|                               |
+-------------------------------+
|  padding                      | \
+-------------------------------+  |
|  callee-saved registers       |  | frame.saved_regs_size
+-------------------------------+  |
|  LR'                          |  |
+-------------------------------+  |
|  FP'                          | /
+-------------------------------+ <-- aligned/hard_frame_pointer_rtx(fp)/函数入口时的virtual_stack_dynamic_rtx
|  dynamic allocation           |
+-------------------------------+
|  padding                      |
+-------------------------------+
|  outgoing stack arguments     |
|                               |
+-------------------------------+ <-- aligned/stack_pointer_rtx(sp)/virtual_outgoing_args_rtx
|                               |
                        | <- lowmem

*/

//硬件寄存器SP的当前位置
//原型 #define stack_pointer_rtx       (global_rtl[GR_STACK_POINTER]) rtl.h
rtx  mtcs_rtl_get_stack_pointer_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_STACK_POINTER];
}

//gcc栈帧中自动生成变量的存储地址
//原型 frame_pointer_rtx     (global_rtl[GR_FRAME_POINTER]) rtl.h
rtx  mtcs_rtl_get_frame_pointer_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_FRAME_POINTER];
}

//当前函数参数列表的地址
//原型 (global_rtl[GR_ARG_POINTER]) rtl.h
rtx  mtcs_rtl_get_arg_pointer_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_ARG_POINTER];
}

//硬件寄存器fp的当前位置
//原型 (global_rtl[GR_HARD_FRAME_POINTER]) rtl.h
rtx  mtcs_rtl_get_hard_frame_pointer_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_HARD_FRAME_POINTER];
}

//函数传入参数栈的首地址 也即caller的outgoing区,对callee是incoming区，实际上该区不是callee的栈区，是caller的栈区
//原型 #define virtual_incoming_args_rtx       (global_rtl[GR_VIRTUAL_INCOMING_ARGS])
rtx  mtcs_rtl_get_virtual_incoming_args_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_VIRTUAL_INCOMING_ARGS];
}

//第一个局部变量的尾地址
//原型 #define virtual_stack_vars_rtx          (global_rtl[GR_VIRTUAL_STACK_ARGS])
rtx  mtcs_rtl_get_virtual_stack_args_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_VIRTUAL_STACK_ARGS];
}

//在函数内执行alloca动态分配栈空间时分配到的首地址
//原型 #define virtual_stack_dynamic_rtx   (global_rtl[GR_VIRTUAL_STACK_DYNAMIC])
rtx  mtcs_rtl_get_virtual_stack_dynamic_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_VIRTUAL_STACK_DYNAMIC];
}

//当前函数outgoing栈的首地址
//原型 (global_rtl[GR_VIRTUAL_OUTGOING_ARGS]) rtl.h
rtx  mtcs_rtl_get_virtual_outgoing_args_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_VIRTUAL_OUTGOING_ARGS];
}

//原型 #define virtual_cfa_rtx         (global_rtl[GR_VIRTUAL_CFA])
rtx  mtcs_rtl_get_virtual_cfa_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_VIRTUAL_CFA];
}
//原型 #define virtual_preferred_stack_boundary_rtx   (global_rtl[GR_VIRTUAL_PREFERRED_STACK_BOUNDARY])
rtx  mtcs_rtl_get_virtual_preferred_stack_boundary_rtx(MtcsRTL *self)
{
    return self->x_global_rtl[MTCS_GR_VIRTUAL_PREFERRED_STACK_BOUNDARY];
}

//原型 #define pic_offset_table_rtx   (this_target_rtl->x_pic_offset_table_rtx)
rtx  mtcs_rtl_get_pic_offset_table_rtx(MtcsRTL *self)
{
   return self->x_pic_offset_table_rtx;
}


/* Return the attributes of a MEM rtx.  */
//原型 rtl.h get_mem_attrs
const class mem_attrs *mtcs_rtl_get_mem_attrs (MtcsRTL *self,const_rtx x)
{
  class mem_attrs *attrs;
  attrs = MEM_ATTRS (x);
  if (!attrs)
    attrs = self->x_mode_mem_attrs[(int) GET_MODE (x)];
  return attrs;
}

//原型 lookup_const_wide_int emit-rtl.cc
static rtx lookup_const_wide_int(MtcsRTL *self,rtx wint)
{
//  rtx *slot = const_wide_int_htab->find_slot (wint, INSERT);
//  if (*slot == 0)
//    *slot = wint;
//  return *slot;
  rtx slot = n_hash_table_lookup(self->const_wide_int_htab,wint);
  if (slot == NULL){
    slot = wint;
    n_hash_table_insert(self->const_wide_int_htab,slot,slot);
  }
  return slot;
}

/* Return an rtx constant for V, given that the constant has mode MODE.
   The returned rtx will be a CONST_INT if V fits, otherwise it will be
   a CONST_DOUBLE (if !TARGET_SUPPORTS_WIDE_INT) or a CONST_WIDE_INT
   (if TARGET_SUPPORTS_WIDE_INT).  */

static rtx immed_wide_int_const_1 (MtcsRTL *self,const wide_int_ref v, mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  unsigned int len = v.get_len ();
  /* Not scalar_int_mode because we also allow pointer bound modes.  */
  unsigned int prec =mtcs_mode_get_precision/* GET_MODE_PRECISION*/(mtcsMode,
          mtcs_mode_as_a <scalar_mode> (mtcsMode,(machine_mode)mode));

  /* Allow truncation but not extension since we do not know if the
     number is signed or unsigned.  */
  gcc_assert (prec <= v.get_precision ());

  if (len < 2 || prec <= HOST_BITS_PER_WIDE_INT)
    return mtcs_rtl_gen_int_mode/*gen_int_mode*/ (self,v.elt (0), mode);

#if TARGET_SUPPORTS_WIDE_INT //host=1  nvptx=1
  {
    unsigned int i;
    rtx value;
    unsigned int blocks_needed
      = (prec + HOST_BITS_PER_WIDE_INT - 1) / HOST_BITS_PER_WIDE_INT;

    if (len > blocks_needed)
      len = blocks_needed;

    value = const_wide_int_alloc (len);

    /* It is so tempting to just put the mode in here.  Must control
       myself ... */
    mtcs_rtl_put_mode/*!PUT_MODE*/(self,value, VOIDmode);
    CWI_PUT_NUM_ELEM (value, len);

    for (i = 0; i < len; i++)
      CONST_WIDE_INT_ELT (value, i) = v.elt (i);

    return lookup_const_wide_int(self,value);
  }
#else
  return immed_double_const (v.elt (0), v.elt (1), mode);//不会执行到这里
#endif
}

/* Return an rtx representation of C in mode MODE.  */
//原型 immed_wide_int_const rtl.h emit-rtl.cc
rtx mtcs_rtl_immed_wide_int_const (MtcsRTL *self,const poly_wide_int_ref c, mtcs_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (c.is_constant ())
    return immed_wide_int_const_1(self,c.coeffs[0], mode);

  /* Not scalar_int_mode because we also allow pointer bound modes.  */
  unsigned int prec = mtcs_mode_get_precision/*GET_MODE_PRECISION*/ (mtcsMode,
          mtcs_mode_as_a <scalar_mode> (mtcsMode,(machine_mode)mode));

  /* Allow truncation but not extension since we do not know if the
     number is signed or unsigned.  */
  gcc_assert (prec <= c.coeffs[0].get_precision ());
  poly_wide_int newc = poly_wide_int::from (c, prec, SIGNED);

  /* See whether we already have an rtx for this constant.  */
  inchash::hash h;
  h.add_int (mode);
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS/*insn-modes.h host nvptx相同 =1*/; ++i)
    h.add_wide_int (newc.coeffs[i]);
  /*
  const_poly_int_hasher::compare_type typed_value ((machine_mode)mode, newc);
  rtx *slot = const_poly_int_htab->find_slot_with_hash (typed_value,h.end (), INSERT);
  rtx x = *slot;
  */
  PolyIntCompareType comapreKey={(machine_mode)mode, newc};
  rtx slot=n_hash_table_lookup_by_hash(self->const_poly_int_htab,&comapreKey,h.end());
  rtx x = slot;
  if (x)
    return x;

  /* Create a new rtx.  There's a choice to be made here between installing
     the actual mode of the rtx or leaving it as VOIDmode (for consistency
     with CONST_INT).  In practice the handling of the codes is different
     enough that we get no benefit from using VOIDmode, and various places
     assume that VOIDmode implies CONST_INT.  Using the real mode seems like
     the right long-term direction anyway.  */
  typedef trailing_wide_ints<NUM_POLY_INT_COEFFS> twi;
  size_t extra_size = twi::extra_size (prec);
  x = rtx_alloc_v (CONST_POLY_INT,sizeof (struct const_poly_int_def) + extra_size);
  mtcs_rtl_put_mode/*!PUT_MODE*/(self,x, mode);
  CONST_POLY_INT_COEFFS (x).set_precision (prec);
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
    CONST_POLY_INT_COEFFS (x)[i] = newc.coeffs[i];
  //*slot = x;
  n_hash_table_insert(self->const_poly_int_htab,x,x);
  return x;
}


/* Return the mode of MEM's address.  */
//原型 rtl.h rtlanal.cc  get_address_mode
scalar_int_mode mtcs_rtl_get_address_mode (MtcsRTL *self,rtx mem)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  machine_mode mode;
  gcc_assert (MEM_P (mem));
  mode = GET_MODE (XEXP (mem, 0));
  if (mode != VOIDmode)
    return mtcs_mode_as_a <scalar_int_mode>(mtcsMode,mode);
  return target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,
        mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(self,mem));
}



/* Return a memory reference like MEMREF, but with its mode changed to MODE
   and its address changed to ADDR.  (VOIDmode means don't change the mode.
   NULL for ADDR means don't change the address.)  VALIDATE is nonzero if the
   returned memory location is required to be valid.  INPLACE is true if any
   changes can be made directly to MEMREF or false if MEMREF must be treated
   as immutable.

   The memory attributes are not changed.  */
//原型 emit-rtl.cc change_address_1
static rtx change_address_1 (MtcsRTL *self,rtx memref, machine_mode mode, rtx addr, int validate,bool inplace)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  addr_space_t as;
  rtx new_rtx;
  n_debug("mtcsrtl.c change_address_1 00 validate:%d inplace:%d\n",validate,inplace);
  gcc_assert (MEM_P (memref));
  as = mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(self,memref);
  if (mode == VOIDmode)
    mode = GET_MODE (memref);
  if (addr == 0)
    addr = XEXP (memref, 0);
  n_debug("mtcsrtl.c change_address_1 11 as:%d inplace:%d %d %d %d\n",as,inplace,mode,GET_MODE (memref),addr == XEXP (memref, 0));

  if (mode == GET_MODE (memref) && addr == XEXP (memref, 0)
      && (!validate || mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, addr,as,ERROR_MARK)))
    return memref;
  n_debug("mtcsrtl.c change_address_1 22 reload_in_progress:%d reload_completed:%d lra_in_progress:%d\n",
        reload_in_progress,reload_completed,lra_in_progress);

  /* Don't validate address for LRA.  LRA can make the address valid
     by itself in most efficient way.  */
  if (validate && !lra_in_progress){
      if (reload_in_progress || reload_completed)
          gcc_assert (mtcs_recog_memory_address_addr_space_p/*memory_address_addr_space_p*/ (mtcsRecog,mode, addr, as,ERROR_MARK));
      else
          addr = mtcs_explow_memory_address_addr_space/*!memory_address_addr_space*/(mtcsExplow,mode, addr, as);
  }

  if (rtx_equal_p (addr, XEXP (memref, 0)) && mode == GET_MODE (memref))
    return memref;
  n_debug("mtcsrtl.c change_address_1 33 as:%d inplace:%d\n",as,inplace);

  if (inplace){
      XEXP (memref, 0) = addr;
      return memref;
  }
  n_debug("mtcsrtl.c change_address_1 44 as:%d inplace:%d\n",as,inplace);

  new_rtx = gen_rtx_MEM (mode, addr);
  MEM_COPY_ATTRIBUTES (new_rtx, memref);
  return new_rtx;
}

/* Set MEM's memory attributes so that they are the same as ATTRS.  */
//原型 set_mem_attrs emit-rtl.cc
static void set_mem_attrs (MtcsRTL *self,rtx mem, mem_attrs *attrs)
{
  /* If everything is the default, we can just clear the attributes.  */
  if (mem_attrs_eq_p (attrs, self->x_mode_mem_attrs[(int) GET_MODE (mem)])) {
      MEM_ATTRS (mem) = 0;
      return;
  }

  if (!MEM_ATTRS (mem) || !mem_attrs_eq_p (attrs, MEM_ATTRS (mem))){
      MEM_ATTRS (mem) = ggc_alloc<mem_attrs> ();
      memcpy (MEM_ATTRS (mem), attrs, sizeof (mem_attrs));
  }
}

/* Set the offset of MEM to OFFSET.  */
//原型 set_mem_offset emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_offset (MtcsRTL *self,rtx mem, poly_int64 offset)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/(self,mem));
  attrs.offset_known_p = true;
  attrs.offset = offset;
  set_mem_attrs (self,mem, &attrs);
}



//原型 rtl.h emit-rtl.cc
rtx mtcs_rtl_adjust_address_1 (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset,
          int validate, int adjust_address, int adjust_object, poly_int64 size)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  rtx addr = XEXP (memref, 0);
  rtx new_rtx;
  scalar_int_mode address_mode;
  class mem_attrs attrs (*mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/ (self,memref)), *defattrs;
  unsigned HOST_WIDE_INT max_align;
  //POINTERS_EXTEND_UNSIGNED host=1 nvptx=0
//#ifdef POINTERS_EXTEND_UNSIGNED
//  scalar_int_mode pointer_mode
//    = targetm.addr_space.pointer_mode (attrs.addrspace);
//#endif

  /* VOIDmode means no mode change for change_address_1.  */
  if (mode == VOIDmode)
    mode = GET_MODE (memref);

  /* Take the size of non-BLKmode accesses from the mode.  */
  defattrs =self->x_mode_mem_attrs/*!mode_mem_attrs*/[(int) mode];
  if (defattrs->size_known_p)
    size = defattrs->size;

  /* If there are no changes, just return the original memory reference.  */
  if (mode == GET_MODE (memref)  && known_eq (offset, 0)  && (known_eq (size, 0)
      || (attrs.size_known_p && known_eq (attrs.size, size))) && (!validate ||
      mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/
                (mtcsRecog,mode, addr,attrs.addrspace,ERROR_MARK))) //code_helper = ERROR_MARK
    return memref;

  /* ??? Prefer to create garbage instead of creating shared rtl.
     This may happen even if offset is nonzero -- consider
     (plus (plus reg reg) const_int) -- so do this always.  */
  addr = copy_rtx (addr);

  /* Convert a possibly large offset to a signed value within the
     range of the target address space.  */
  address_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/ (self,memref);
  offset = mtcs_mode_trunc_int_for_mode_with_poly_int64/*!trunc_int_for_mode*/(mtcsMode,offset, address_mode);

  if (adjust_address){
      /* If MEMREF is a LO_SUM and the offset is within the alignment of the
     object, we can merge it into the LO_SUM.  */
      if (GET_MODE (memref) != mtcsMode->modes.M_BLKmode && GET_CODE (addr) == LO_SUM
              && known_in_range_p (offset, 0, (mtcs_mode_get_alignment/*GET_MODE_ALIGNMENT*/ (mtcsMode,GET_MODE (memref)) / BITS_PER_UNIT)))
          addr = gen_rtx_LO_SUM (address_mode, XEXP (addr, 0),mtcs_rtl_plus_constant/*!plus_constant*/ (self,address_mode,XEXP (addr, 1), offset,false));
 //POINTERS_EXTEND_UNSIGNED host=1 nvptx=0
//#ifdef POINTERS_EXTEND_UNSIGNED
//      /* If MEMREF is a ZERO_EXTEND from pointer_mode and the offset is valid
//     in that mode, we merge it into the ZERO_EXTEND.  We take advantage of
//     the fact that pointers are not allowed to overflow.  */
//      else if (POINTERS_EXTEND_UNSIGNED > 0
//           && GET_CODE (addr) == ZERO_EXTEND
//           && GET_MODE (XEXP (addr, 0)) == pointer_mode
//           && known_eq (mtcs_mode_trunc_int_for_mode (mtcsMode,offset, pointer_mode), offset))
//          addr = gen_rtx_ZERO_EXTEND (address_mode,mtcs_rtl_plus_constant/*!plus_constant*/ (self,pointer_mode,XEXP (addr, 0), offset));
//#endif
     else
          addr = mtcs_rtl_plus_constant/*!plus_constant*/ (self,address_mode, addr, offset,false);
  }

  new_rtx = change_address_1 (self,memref, mode, addr, validate, false);

  /* If the address is a REG, change_address_1 rightfully returns memref,
     but this would destroy memref's MEM_ATTRS.  */
  if (new_rtx == memref && maybe_ne (offset, 0))
    new_rtx = copy_rtx (new_rtx);

  /* Conservatively drop the object if we don't know where we start from.  */
  if (adjust_object && (!attrs.offset_known_p || !attrs.size_known_p)){
      attrs.expr = NULL_TREE;
      attrs.alias = 0;
  }

  /* Compute the new values of the memory attributes due to this adjustment.
     We add the offsets and update the alignment.  */
  if (attrs.offset_known_p){
      attrs.offset += offset;

      /* Drop the object if the new left end is not within its bounds.  */
      if (adjust_object && maybe_lt (attrs.offset, 0)){
          attrs.expr = NULL_TREE;
          attrs.alias = 0;
      }
  }

  /* Compute the new alignment by taking the MIN of the alignment and the
     lowest-order set bit in OFFSET, but don't change the alignment if OFFSET
     if zero.  */
  if (maybe_ne (offset, 0)){
      max_align = known_alignment (offset) * BITS_PER_UNIT;
      attrs.align = MIN (attrs.align, max_align);
  }

  if (maybe_ne (size, 0)){
      /* Drop the object if the new right end is not within its bounds.  */
      if (adjust_object && maybe_gt (offset + size, attrs.size)){
          attrs.expr = NULL_TREE;
          attrs.alias = 0;
      }
      attrs.size_known_p = true;
      attrs.size = size;
  }else if (attrs.size_known_p){
      gcc_assert (!adjust_object);
      attrs.size -= offset;
      /* ??? The store_by_pieces machinery generates negative sizes,
     so don't assert for that here.  */
  }

  set_mem_attrs (self,new_rtx, &attrs);

  return new_rtx;
}

//原型 adjust_address emit-rtl.h emit-rtl.cc  adjust_address_1 (MEMREF, MODE, OFFSET, 1, 1, 0, 0)
rtx mtcs_rtl_adjust_address (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset)
{
    return mtcs_rtl_adjust_address_1(self,memref,mode,offset, 1, 1, 0, 0);
}

//原型 adjust_address_nv  emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_adjust_address_nv (MtcsRTL *self,rtx memref, mtcs_mode mode, poly_int64 offset)
{
    return mtcs_rtl_adjust_address_1(self,memref,mode,offset, 0, 1, 0, 0);
}


/* Determine whether REAL, a CONST_DOUBLE, already exists in the
   hash table.  If so, return its counterpart; otherwise add it
   to the hash table and return it.  */
static rtx lookup_const_double (MtcsRTL *self,rtx real)
{
    /*
  rtx *slot = const_double_htab->find_slot (real, INSERT);
  if (*slot == 0)
    *slot = real;
  return *slot;
  */
  rtx slot =n_hash_table_lookup(self->const_double_htab,real);
    if (slot == NULL){
        slot=real;
        n_hash_table_insert(self->const_double_htab,real,real);
    }
    return slot;
}


/* Return a CONST_DOUBLE rtx for a floating-point value specified by
   VALUE in mode MODE.  */
//原型 const_double_from_real_value rtl.h emit-rtl.cc
rtx mtcs_rtl_const_double_from_real_value (MtcsRTL *self,REAL_VALUE_TYPE value, machine_mode mode)
{
  rtx real = rtx_alloc (CONST_DOUBLE);
  mtcs_rtl_put_mode/*!PUT_MODE*/(self,real, mode);
  real->u.rv = value;
  return lookup_const_double (self,real);
}

/* Determine whether FIXED, a CONST_FIXED, already exists in the
   hash table.  If so, return its counterpart; otherwise add it
   to the hash table and return it.  */
//原型 lookup_const_fixed emit-rtl.cc
static rtx lookup_const_fixed(MtcsRTL *self,rtx fixed)
{
  /*
  rtx *slot = const_fixed_htab->find_slot (fixed, INSERT);
  if (*slot == 0)
    *slot = fixed;
  return *slot;
  */
    rtx slot = n_hash_table_lookup(self->const_fixed_htab,fixed);
    if (slot == NULL){
        slot = fixed;
        n_hash_table_insert(self->const_fixed_htab,fixed,fixed);

    }
    return slot;
}

/* Return a CONST_FIXED rtx for a fixed-point value specified by
   VALUE in mode MODE.  */
//原型 const_fixed_from_fixed_value fixed-value.h
//原型 #define CONST_FIXED_FROM_FIXED_VALUE(r, m)  const_fixed_from_fixed_value (r, m)
rtx mtcs_rtl_const_fixed_from_fixed_value (MtcsRTL *self,FIXED_VALUE_TYPE value, machine_mode mode)
{
  rtx fixed = rtx_alloc (CONST_FIXED);
  mtcs_rtl_put_mode/*!PUT_MODE*/(self,fixed, mode);
  fixed->u.fv = value;
  return lookup_const_fixed(self,fixed);
}

/* Generate a vector like gen_rtx_raw_CONST_VEC, but use the zero vector when
   all elements are zero, and the one vector when all elements are one.  */
//原型gen_rtx_CONST_VECTOR emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_rtx_CONST_VECTOR (MtcsRTL *self,machine_mode mode, rtvec v)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  gcc_assert (known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode), GET_NUM_ELEM (v)));

  /* If the values are all the same, check to see if we can use one of the
     standard constant vectors.  */
  if (rtvec_all_equal_p (v))
    return mtcs_rtl_gen_const_vec_duplicate (self,mode, RTVEC_ELT (v, 0));

  unsigned int nunits = GET_NUM_ELEM (v);
  MtcsVectorBuilder builder (mtcsMode,mode, nunits, 1);
  for (unsigned int i = 0; i < nunits; ++i)
    builder.quick_push (RTVEC_ELT (v, i));
  return builder.build (v);
}



/* We want to create (subreg:OMODE (obj:IMODE) OFFSET).  Return true if
   this construct would be valid, and false otherwise.  */
//原型 validate_subreg rtl.h emit-rtl.cc
bool mtcs_rtl_validate_subreg (MtcsRTL *self,machine_mode omode, machine_mode imode,const_rtx reg, poly_uint64 offset)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  poly_uint64 isize = mtcs_mode_get_size(mtcsMode,imode);
  poly_uint64 osize = mtcs_mode_get_size (mtcsMode,omode);

  /* The sizes must be ordered, so that we know whether the subreg
     is partial, paradoxical or complete.  */
  if (!ordered_p (isize, osize))
    return false;

  /* All subregs must be aligned.  */
  if (!multiple_p (offset, osize))
    return false;

  /* The subreg offset cannot be outside the inner object.  */
  if (maybe_ge (offset, isize))
    return false;

  poly_uint64 regsize = mtcs_mode_get_regmode_natural_size/*REGMODE_NATURAL_SIZE*/(mtcsMode,imode);

  /* ??? This should not be here.  Temporarily continue to allow word_mode
     subregs of anything.  The most common offender is (subreg:SI (reg:DF)).
     Generally, backends are doing something sketchy but it'll take time to
     fix them all.  */
  if (omode == word_mode)
    ;
  /* ??? Similarly, e.g. with (subreg:DF (reg:TI)).  Though store_bit_field
     is the culprit here, and not the backends.  */
  else if (known_ge (osize, regsize) && known_ge (isize, osize))
    ;
  /* Allow component subregs of complex and vector.  Though given the below
     extraction rules, it's not always clear what that means.  */
  else if ((mtcs_mode_is_complex_p (mtcsMode,imode) || mtcs_mode_is_vector_p (mtcsMode,imode))
       && mtcs_mode_get_inner (mtcsMode,imode) == omode)
    ;
  /* ??? x86 sse code makes heavy use of *paradoxical* vector subregs,
     i.e. (subreg:V4SF (reg:SF) 0) or (subreg:V4SF (reg:V2SF) 0).  This
     surely isn't the cleanest way to represent this.  It's questionable
     if this ought to be represented at all -- why can't this all be hidden
     in post-reload splitters that make arbitrarily mode changes to the
     registers themselves.  */
  else if (mtcs_mode_is_vector_p (mtcsMode,omode)
       && mtcs_mode_get_unit_size (mtcsMode,omode) == mtcs_mode_get_unit_size (mtcsMode,imode))
    ;
  /* Subregs involving floating point modes are not allowed to
     change size unless it's an insert into a complex mode.
     Therefore (subreg:DI (reg:DF) 0) and (subreg:CS (reg:SF) 0) are fine, but
     (subreg:SI (reg:DF) 0) isn't.  */
  else if ((mtcs_mode_is_float_p (mtcsMode,imode) || mtcs_mode_is_float_p (mtcsMode,omode))
       && !mtcs_mode_is_complex_p (mtcsMode,omode)){
      if (! (known_eq (isize, osize)
         /* LRA can use subreg to store a floating point value in
        an integer mode.  Although the floating point and the
        integer modes need the same number of hard registers,
        the size of floating point mode can be less than the
        integer mode.  LRA also uses subregs for a register
        should be used in different mode in on insn.  */
         || lra_in_progress))
          return false;
  }

  /* Paradoxical subregs must have offset zero.  */
  if (maybe_gt (osize, isize))
    return known_eq (offset, 0U);

  /* This is a normal subreg.  Verify that the offset is representable.  */

  /* For hard registers, we already have most of these rules collected in
     subreg_offset_representable_p.  */
  if (reg && REG_P (reg) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg)){
      unsigned int regno = REGNO (reg);

      if ((mtcs_mode_is_complex_p (mtcsMode,imode) || mtcs_mode_is_vector_p (mtcsMode,imode))
              && mtcs_mode_get_inner (mtcsMode,imode) == omode)
          ;
      else if (!mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,regno, imode, omode))
          return false;

      return subreg_offset_representable_p (regno, imode, offset, omode);
  }
  /* Do not allow SUBREG with stricter alignment than the inner MEM.  */
  else if (reg && MEM_P (reg) && mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
          && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(self,reg) < mtcs_mode_get_alignment(mtcsMode,omode))
    return false;

  /* The outer size must be ordered wrt the register size, otherwise
     we wouldn't know at compile time how many registers the outer
     mode occupies.  */
  if (!ordered_p (osize, regsize))
    return false;

  /* For pseudo registers, we want most of the same checks.  Namely:

     Assume that the pseudo register will be allocated to hard registers
     that can hold REGSIZE bytes each.  If OSIZE is not a multiple of REGSIZE,
     the remainder must correspond to the lowpart of the containing hard
     register.  If BYTES_BIG_ENDIAN, the lowpart is at the highest offset,
     otherwise it is at the lowest offset.

     Given that we've already checked the mode and offset alignment,
     we only have to check subblock subregs here.  */
  if (maybe_lt (osize, regsize) && ! (lra_in_progress && (mtcs_mode_is_float_p(mtcsMode,imode) || mtcs_mode_is_float_p (mtcsMode,omode)))){
      /* It is invalid for the target to pick a register size for a mode
     that isn't ordered wrt to the size of that mode.  */
      poly_uint64 block_size = ordered_min (isize, regsize);
      unsigned int start_reg;
      poly_uint64 offset_within_reg;
      if (!can_div_trunc_p (offset, block_size, &start_reg, &offset_within_reg)
              || (BYTES_BIG_ENDIAN ? maybe_ne (offset_within_reg, block_size - osize): maybe_ne (offset_within_reg, 0U)))
          return false;
  }
  return true;
}

//原型 MEM_ALIGN rtl.h
nuint   mtcs_rtl_get_mem_align(MtcsRTL *self,rtx x)
{
   return  mtcs_rtl_get_mem_attrs (self,x)->align;
}

//原型MEM_SIZE rtl.h
poly_int64  mtcs_rtl_get_mem_size(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->size;

}
//原型 MEM_SIZE_KNOWN_P rtl.h
nboolean  mtcs_rtl_is_mem_size_known_p(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->size_known_p;

}
//原型 MEM_ADDR_SPACE rtl.h
nuchar  mtcs_rtl_get_mem_addr_space(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->addrspace;

}
//原型MEM_OFFSET rtl.h
poly_int64  mtcs_rtl_get_mem_offset(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->offset;
}

//原型MEM_OFFSET_KNOWN_P rtl.h
nboolean  mtcs_rtl_is_mem_offset_known_p(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->offset_known_p;
}

//原型MEM_ALIAS_SET rtl.h
alias_set_type mtcs_rtl_get_mem_alias(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->alias;

}
//原型MEM_EXPR rtl.h
tree mtcs_rtl_get_mem_expr(MtcsRTL *self,rtx x)
{
    return  mtcs_rtl_get_mem_attrs (self,x)->expr;
}



/* Fill in information about a subreg of a hard register.
   xregno - A regno of an inner hard subreg_reg (or what will become one).
   xmode  - The mode of xregno.
   offset - The byte offset.
   ymode  - The mode of a top level SUBREG (or what may become one).
   info   - Pointer to structure to fill in.

   Rather than considering one particular inner register (and thus one
   particular "outer" register) in isolation, this function really uses
   XREGNO as a model for a sequence of isomorphic hard registers.  Thus the
   function does not check whether adding INFO->offset to XREGNO gives
   a valid hard register; even if INFO->offset + XREGNO is out of range,
   there might be another register of the same type that is in range.
   Likewise it doesn't check whether targetm.hard_regno_mode_ok accepts
   the new register, since that can depend on things like whether the final
   register number is even or odd.  Callers that want to check whether
   this particular subreg can be replaced by a simple (reg ...) should
   use simplify_subreg_regno.  */
//原型subreg_get_info rtl.h rtlanal.cc
void mtcs_rtl_subreg_get_info (MtcsRTL *self,unsigned int xregno, machine_mode xmode,poly_uint64 offset, machine_mode ymode,struct subreg_info *info)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg    *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  unsigned int nregs_xmode, nregs_ymode;

  gcc_assert (xregno < mtcs_reg_get_first_pseudo_register(mtcsReg));

  poly_uint64 xsize = mtcs_mode_get_size(mtcsMode,xmode);
  poly_uint64 ysize = mtcs_mode_get_size (mtcsMode,ymode);

  bool rknown = false;

  /* If the register representation of a non-scalar mode has holes in it,
     we expect the scalar units to be concatenated together, with the holes
     distributed evenly among the scalar units.  Each scalar unit must occupy
     at least one register.  */
  if (MTCS_HARD_REGNO_NREGS_HAS_PADDING (xregno, xmode)){
      /* As a consequence, we must be dealing with a constant number of
     scalars, and thus a constant offset and number of units.  */
      HOST_WIDE_INT coffset = offset.to_constant ();
      HOST_WIDE_INT cysize = ysize.to_constant ();
      nregs_xmode = MTCS_HARD_REGNO_NREGS_WITH_PADDING (xregno, xmode);
      unsigned int nunits =mtcs_mode_get_nunits/* GET_MODE_NUNITS*/ (mtcsMode,xmode).to_constant ();
      scalar_mode xmode_unit = mtcs_mode_get_inner (mtcsMode,xmode);
      gcc_assert (MTCS_HARD_REGNO_NREGS_HAS_PADDING (xregno, xmode_unit));
      gcc_assert (nregs_xmode == (nunits* MTCS_HARD_REGNO_NREGS_WITH_PADDING (xregno, xmode_unit)));
      gcc_assert (mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/ (mtcsReg,xregno, xmode)
              == mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/ (mtcsReg,xregno, xmode_unit) * nunits);

      /* You can only ask for a SUBREG of a value with holes in the middle
     if you don't cross the holes.  (Such a SUBREG should be done by
     picking a different register class, or doing it in memory if
     necessary.)  An example of a value with holes is XCmode on 32-bit
     x86 with -m128bit-long-double; it's represented in 6 32-bit registers,
     3 for each part, but in memory it's two 128-bit parts.
     Padding is assumed to be at the end (not necessarily the 'high part')
     of each unit.  */
      if ((coffset / mtcs_mode_get_size (mtcsMode,xmode_unit) + 1 < nunits) && (coffset / mtcs_mode_get_size (mtcsMode,xmode_unit)
          != ((coffset + cysize - 1) / mtcs_mode_get_size (mtcsMode,xmode_unit)))){
          info->representable_p = false;
          rknown = true;
      }
  }else
    nregs_xmode = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,xregno, xmode);

  nregs_ymode = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/ (mtcsReg,xregno, ymode);

  /* Subreg sizes must be ordered, so that we can tell whether they are
     partial, paradoxical or complete.  */
  gcc_checking_assert (ordered_p (xsize, ysize));

  /* Paradoxical subregs are otherwise valid.  */
  if (!rknown && known_eq (offset, 0U) && maybe_gt (ysize, xsize)){
      info->representable_p = true;
      /* If this is a big endian paradoxical subreg, which uses more
     actual hard registers than the original register, we must
     return a negative offset so that we find the proper highpart
     of the register.

     We assume that the ordering of registers within a multi-register
     value has a consistent endianness: if bytes and register words
     have different endianness, the hard registers that make up a
     multi-register value must be at least word-sized.  */
      if (REG_WORDS_BIG_ENDIAN)
          info->offset = (int) nregs_xmode - (int) nregs_ymode;
      else
          info->offset = 0;
      info->nregs = nregs_ymode;
      return;
  }

  /* If registers store different numbers of bits in the different
     modes, we cannot generally form this subreg.  */
  poly_uint64 regsize_xmode, regsize_ymode;
  if (!MTCS_HARD_REGNO_NREGS_HAS_PADDING (xregno, xmode) && !MTCS_HARD_REGNO_NREGS_HAS_PADDING (xregno, ymode)
      && multiple_p (xsize, nregs_xmode, &regsize_xmode) && multiple_p (ysize, nregs_ymode, &regsize_ymode)){
      if (!rknown  && ((nregs_ymode > 1 && maybe_gt (regsize_xmode, regsize_ymode))
          || (nregs_xmode > 1 && maybe_gt (regsize_ymode, regsize_xmode)))){
          info->representable_p = false;
          if (!can_div_away_from_zero_p (ysize, regsize_xmode, &info->nregs)
              || !can_div_trunc_p (offset, regsize_xmode, &info->offset))
            /* Checked by validate_subreg.  We must know at compile time
               which inner registers are being accessed.  */
            gcc_unreachable ();
          return;
      }
      /* It's not valid to extract a subreg of mode YMODE at OFFSET that
     would go outside of XMODE.  */
      if (!rknown && maybe_gt (ysize + offset, xsize)){
          info->representable_p = false;
          info->nregs = nregs_ymode;
          if (!can_div_trunc_p (offset, regsize_xmode, &info->offset))
            /* Checked by validate_subreg.  We must know at compile time
               which inner registers are being accessed.  */
            gcc_unreachable ();
          return;
      }
      /* Quick exit for the simple and common case of extracting whole
     subregisters from a multiregister value.  */
      /* ??? It would be better to integrate this into the code below,
     if we can generalize the concept enough and figure out how
     odd-sized modes can coexist with the other weird cases we support.  */
      HOST_WIDE_INT count;
      if (!rknown  && WORDS_BIG_ENDIAN == REG_WORDS_BIG_ENDIAN  && known_eq (regsize_xmode, regsize_ymode)
          && constant_multiple_p (offset, regsize_ymode, &count)) {
          info->representable_p = true;
          info->nregs = nregs_ymode;
          info->offset = count;
          gcc_assert (info->offset + info->nregs <= (int) nregs_xmode);
          return;
      }
  }

  /* Lowpart subregs are otherwise valid.  */
  if (!rknown && known_eq (offset, mtcs_mode_subreg_lowpart_offset (mtcsMode,ymode, xmode))){
      info->representable_p = true;
      rknown = true;

      if (known_eq (offset, 0U) || nregs_xmode == nregs_ymode){
          info->offset = 0;
          info->nregs = nregs_ymode;
          return;
      }
  }

  /* Set NUM_BLOCKS to the number of independently-representable YMODE
     values there are in (reg:XMODE XREGNO).  We can view the register
     as consisting of this number of independent "blocks", where each
     block occupies NREGS_YMODE registers and contains exactly one
     representable YMODE value.  */
  gcc_assert ((nregs_xmode % nregs_ymode) == 0);
  unsigned int num_blocks = nregs_xmode / nregs_ymode;

  /* Calculate the number of bytes in each block.  This must always
     be exact, otherwise we don't know how to verify the constraint.
     These conditions may be relaxed but subreg_regno_offset would
     need to be redesigned.  */
  poly_uint64 bytes_per_block = exact_div (xsize, num_blocks);

  /* Get the number of the first block that contains the subreg and the byte
     offset of the subreg from the start of that block.  */
  unsigned int block_number;
  poly_uint64 subblock_offset;
  if (!can_div_trunc_p (offset, bytes_per_block, &block_number,&subblock_offset))
    /* Checked by validate_subreg.  We must know at compile time which
       inner registers are being accessed.  */
    gcc_unreachable ();

  if (!rknown){
      /* Only the lowpart of each block is representable.  */
      info->representable_p = known_eq (subblock_offset,subreg_size_lowpart_offset (ysize, bytes_per_block));
      rknown = true;
  }

  /* We assume that the ordering of registers within a multi-register
     value has a consistent endianness: if bytes and register words
     have different endianness, the hard registers that make up a
     multi-register value must be at least word-sized.  */
  if (WORDS_BIG_ENDIAN != REG_WORDS_BIG_ENDIAN)
    /* The block number we calculated above followed memory endianness.
       Convert it to register endianness by counting back from the end.
       (Note that, because of the assumption above, each block must be
       at least word-sized.)  */
    info->offset = (num_blocks - block_number - 1) * nregs_ymode;
  else
    info->offset = block_number * nregs_ymode;
  info->nregs = nregs_ymode;
}

/* Return the offset of (subreg:OUTER_MODE (mem:INNER_MODE X) OFFSET)
   from address X.  For paradoxical big-endian subregs this is a
   negative value, otherwise it's the same as OFFSET.  */
//原型 subreg_memory_offset rtl.h emit-rtl.cc 重载方法subreg_memory_offset
poly_int64 mtcs_rtl_subreg_memory_offset (MtcsRTL *self,machine_mode outer_mode, machine_mode inner_mode,poly_uint64 offset)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg    *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  if (mtcs_mode_paradoxical_subreg_p (mtcsMode,outer_mode, inner_mode)){
      gcc_assert (known_eq (offset, 0U));
      return -mtcs_mode_subreg_lowpart_offset (mtcsMode,inner_mode, outer_mode);
  }
  return offset;
}

//原型 rtl.h extern poly_int64 subreg_memory_offset (const_rtx); emit-rtl.cc 重载方法subreg_memory_offset
poly_int64 mtcs_rtl_subreg_memory_offset_with_rtx (MtcsRTL *self,const_rtx x)
{
  return mtcs_rtl_subreg_memory_offset (self,GET_MODE (x), GET_MODE (SUBREG_REG (x)),SUBREG_BYTE (x));
}

//原型 gen_rtx_SUBREG rtl.h emit-rtl.cc
rtx  mtcs_rtl_gen_rtx_SUBREG (MtcsRTL *self,machine_mode mode, rtx reg, poly_uint64 offset)
{
  gcc_assert (mtcs_rtl_validate_subreg (self,mode, GET_MODE (reg), reg, offset));
  return gen_rtx_raw_SUBREG (mode, reg, offset);
}

/* Generate a SUBREG representing the least-significant part of REG if MODE
   is smaller than mode of REG, otherwise paradoxical SUBREG.  */
//原型 gen_lowpart_SUBREG rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_lowpart_SUBREG (MtcsRTL *self,machine_mode mode, rtx reg)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  machine_mode inmode;
  inmode = GET_MODE (reg);
  if (inmode == VOIDmode)
    inmode = mode;
  return mtcs_rtl_gen_rtx_SUBREG (self,mode, reg,mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,mode, inmode));
}

/* Return the value of element I of CONST_VECTOR X.  */
//原型 const_vector_elt rtl.h emit-rtl.cc #define CONST_VECTOR_ELT(RTX, N) const_vector_elt (RTX, N)
rtx mtcs_rtl_const_vector_elt (MtcsRTL *self,const_rtx x, unsigned int i)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* First handle elements that are directly encoded.  */
  if (i < (unsigned int) XVECLEN (x, 0))
    return CONST_VECTOR_ENCODED_ELT (x, i);

  /* If there are no steps, the final encoded value is the right one.  */
  if (!CONST_VECTOR_STEPPED_P (x)){
      /* Identify the pattern that contains element I and work out the index of
     the last encoded element for that pattern.  */
      unsigned int encoded_nelts = const_vector_encoded_nelts (x);
      unsigned int npatterns = CONST_VECTOR_NPATTERNS (x);
      unsigned int pattern = i % npatterns;
      unsigned int final_i = encoded_nelts - npatterns + pattern;
      return CONST_VECTOR_ENCODED_ELT (x, final_i);
  }

  /* Otherwise work out the value from the last two encoded elements.  */
  return mtcs_rtl_immed_wide_int_const (self,const_vector_int_elt (x, i),mtcs_mode_get_inner(mtcsMode,GET_MODE (x)));
}


/* Return the number of a YMODE register to which

       (subreg:YMODE (reg:XMODE XREGNO) OFFSET)

   can be simplified.  Return -1 if the subreg can't be simplified.

   XREGNO is a hard register number.  */
//原型 simplify_subreg_regno rtl.h rtlanal.cc
int mtcs_rtl_simplify_subreg_regno (MtcsRTL *self,unsigned int xregno, machine_mode xmode, poly_uint64 offset, machine_mode ymode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg    *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  struct subreg_info info;
  unsigned int yregno;

  /* Give the backend a chance to disallow the mode change.  */
  if (mtcs_mode_get_class(mtcsMode,xmode) != MODE_COMPLEX_INT
      && mtcs_mode_get_class (mtcsMode,xmode) != MODE_COMPLEX_FLOAT
      && !mtcs_reg_can_change_mode/*!REG_CAN_CHANGE_MODE_P*/(mtcsReg,xregno, xmode, ymode))
    return -1;

  /* We shouldn't simplify stack-related registers.  */
  if ((!reload_completed || mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/)
          && xregno ==mtcsReg->normalHardRegsNum.frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/)
    return -1;

  if (mtcsReg->normalHardRegsNum.frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/
          != mtcsReg->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/
          && xregno == mtcsReg->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/)
    return -1;

  if (xregno == mtcsReg->normalHardRegsNum.stack_pointer_regnum/*!STACK_POINTER_REGNUM*/
      /* We should convert hard stack register in LRA if it is
     possible.  */
      && ! lra_in_progress)
    return -1;

  /* Try to get the register offset.  */
  mtcs_rtl_subreg_get_info (self,xregno, xmode, offset, ymode, &info);
  if (!info.representable_p)
    return -1;

  /* Make sure that the offsetted register value is in range.  */
  yregno = xregno + info.offset;
  if (!mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,yregno))
    return -1;

  /* See whether (reg:YMODE YREGNO) is valid.

     ??? We allow invalid registers if (reg:XMODE XREGNO) is also invalid.
     This is a kludge to work around how complex FP arguments are passed
     on IA-64 and should be fixed.  See PR target/49226.  */
  if (!mtcsTarget->hard_regno_mode_ok(mtcsTarget,yregno, ymode)  && mtcsTarget->hard_regno_mode_ok (mtcsTarget,xregno, xmode))
    return -1;

  return (int) yregno;
}

/* Generate a vector constant of mode MODE in which every element has
   value ELT.  */
//原型 gen_const_vec_duplicate emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_const_vec_duplicate (MtcsRTL *self,machine_mode mode, rtx elt)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsVectorBuilder builder (mtcsMode,mode, 1, 1);
  builder.quick_push (elt);
  return builder.build ();
}


/* Gets minimal and maximal values for MODE (signed or unsigned depending on
   SIGN).  The returned constants are made to be usable in TARGET_MODE.  */
//原型 get_mode_bounds rtl.h stor-layout.cc
void mtc_rtl_get_mode_bounds (MtcsRTL *self,scalar_int_mode mode, int sign, scalar_int_mode target_mode,rtx *mmin, rtx *mmax)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
  unsigned size = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/ (mtcsMode,mode);
  unsigned HOST_WIDE_INT min_val, max_val;

  gcc_assert (size <= HOST_BITS_PER_WIDE_INT);
  n_debug("mtcsrtl.c mtc_rtl_get_mode_bounds 00 mode:%d sign:%d target_mode:%d\n",mode,sign,target_mode);

  /* Special case BImode, which has values 0 and STORE_FLAG_VALUE.  */
  if (mode == mtcsMode->modes.M_BImode){
      if (mtcs_real_get_store_flag_value/*STORE_FLAG_VALUE*/(mtcsReal) < 0){

          min_val = mtcs_real_get_store_flag_value/*STORE_FLAG_VALUE*/(mtcsReal);
          max_val = 0;
          n_debug("mtcsrtl.c mtc_rtl_get_mode_bounds 11 mode:%d sign:%d target_mode:%d %lu %lu\n",
                mode,sign,target_mode,min_val,max_val);

      }else{
          min_val = 0;
          max_val = mtcs_real_get_store_flag_value/*STORE_FLAG_VALUE*/(mtcsReal);
          n_debug("mtcsrtl.c mtc_rtl_get_mode_bounds 22 mode:%d sign:%d target_mode:%d %lu %lu\n",
                mode,sign,target_mode,min_val,max_val);
      }
  }else if (sign){
      min_val = -(HOST_WIDE_INT_1U << (size - 1));
      max_val = (HOST_WIDE_INT_1U << (size - 1)) - 1;
      n_debug("mtcsrtl.c mtc_rtl_get_mode_bounds 33 mode:%d sign:%d target_mode:%d min:"
            HOST_WIDE_INT_PRINT_UNSIGNED" max:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",
            mode,sign,target_mode,min_val,max_val);
  }else{
      min_val = 0;
      max_val = (HOST_WIDE_INT_1U << (size - 1) << 1) - 1;
      n_debug("mtcsrtl.c mtc_rtl_get_mode_bounds 44 mode:%d sign:%d target_mode:%d %lu %lu\n",
            mode,sign,target_mode,min_val,max_val);
  }

  *mmin = mtcs_rtl_gen_int_mode (self,min_val, target_mode);
  *mmax = mtcs_rtl_gen_int_mode (self,max_val, target_mode);
  n_debug("mtcsrtl.c mtc_rtl_get_mode_bounds 55 mode:%d sign:%d target_mode:%d min:"
        HOST_WIDE_INT_PRINT_UNSIGNED" max:"HOST_WIDE_INT_PRINT_UNSIGNED"\n",
        mode,sign,target_mode,min_val,max_val);
}


//原型 paradoxical_subreg_p rtl.h 还有一个重载paradoxical_subreg_p(machine_mode m1)
nboolean mtcs_rtl_paradoxical_subreg_p (MtcsRTL *self,const_rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (GET_CODE (x) != SUBREG)
    return false;
  return mtcs_mode_paradoxical_subreg_p(mtcsMode,GET_MODE (x), GET_MODE (SUBREG_REG (x)));
}

/* Return a constant shift amount for shifting a value of mode MODE
   by VALUE bits.  */
//原型 gen_int_shift_amount emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_int_shift_amount (MtcsRTL *self,machine_mode, poly_int64 value)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  /* Use a 64-bit mode, to avoid any truncation.
     ??? Perhaps this should be automatically derived from the .md files
     instead, or perhaps have a target hook.  */
  scalar_int_mode shift_mode = (BITS_PER_UNIT == 8
                ? mtcsMode->modes.M_DImode: mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,64, 0).require ());
  return mtcs_rtl_gen_int_mode (self,value, shift_mode);
}

//原型 gen_highpart rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_highpart (MtcsRTL *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  poly_uint64 msize = mtcs_mode_get_size (mtcsMode,mode);
  rtx result;

  /* This case loses if X is a subreg.  To catch bugs early,
     complain if an invalid MODE is used even in other cases.  */
  gcc_assert (known_le (msize, (unsigned int) UNITS_PER_WORD) || known_eq (msize, mtcs_mode_get_unit_size(mtcsMode,GET_MODE (x))));

  /* gen_lowpart_common handles a lot of special cases due to needing to handle
     paradoxical subregs; it only calls simplify_gen_subreg when certain that
     it will produce something meaningful.  The only case we need to handle
     specially here is MEM.  */
  if (MEM_P (x)){
      poly_int64 offset =mtcs_mode_subreg_highpart_offset/*!subreg_highpart_offset*/ (mtcsMode,mode, GET_MODE (x));
      return mtcs_rtl_adjust_address (self,x, mode, offset);
  }

  result = simplify_gen_subreg (mode, x, GET_MODE (x),mtcs_mode_subreg_highpart_offset (mtcsMode,mode, GET_MODE (x)));
  /* Since we handle MEM directly above, we should never get a MEM back
     from simplify_gen_subreg.  */
  gcc_assert (result && !MEM_P (result));

  return result;
}

/* Return a value representing some low-order bits of X, where the number
   of low-order bits is given by MODE.  Note that no conversion is done
   between floating-point and fixed-point values, rather, the bit
   representation is returned.

   This function handles the cases in common between gen_lowpart, below,
   and two variants in cse.cc and combine.cc.  These are the cases that can
   be safely handled at all points in the compilation.

   If this is not a case we can handle, return 0.  */
//原型 gen_lowpart_common rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_lowpart_common (MtcsRTL *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

  poly_uint64 msize =mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,mode);
  machine_mode innermode;
  n_debug("mtcsrtl.c mtcs_rtl_gen_lowpart_common 00 mode:%d x:%p\n",mode,x);
  mtcs_print_rtl(stderr,x);

  /* Unfortunately, this routine doesn't take a parameter for the mode of X,
     so we have to make one up.  Yuk.  */
  innermode = GET_MODE (x);
  if (CONST_INT_P (x)  && known_le (msize * BITS_PER_UNIT, (unsigned HOST_WIDE_INT) HOST_BITS_PER_WIDE_INT))
    innermode = mtcs_mode_int_mode_for_size (mtcsMode,HOST_BITS_PER_WIDE_INT, 0).require ();
  else if (innermode == VOIDmode)
    innermode = mtcs_mode_int_mode_for_size (mtcsMode,HOST_BITS_PER_DOUBLE_INT, 0).require ();

  gcc_assert (innermode != VOIDmode && innermode != mtcsMode->modes.M_BLKmode);
  n_debug("mtcsrtl.c mtcs_rtl_gen_lowpart_common 11 innermode:%d x:%p %s\n",innermode,x,GET_RTX_NAME(GET_CODE (x)));

  if (innermode == mode)
    return x;

  /* The size of the outer and inner modes must be ordered.  */
  poly_uint64 xsize = mtcs_mode_get_size (mtcsMode,innermode);
  if (!ordered_p (msize, xsize))
    return 0;

  if (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/ (mtcsMode,mode)){
     n_debug("mtcsrtl.c mtcs_rtl_gen_lowpart_common 11aa innermode:%d x:%p %s\n",innermode,x,GET_RTX_NAME(GET_CODE (x)));

      /* Don't allow paradoxical FLOAT_MODE subregs.  */
      if (maybe_gt (msize, xsize))
          return 0;
  }else{
     n_debug("mtcsrtl.c mtcs_rtl_gen_lowpart_common 11bb innermode:%d x:%p %s\n",innermode,x,GET_RTX_NAME(GET_CODE (x)));

      /* MODE must occupy no more of the underlying registers than X.  */
      poly_uint64 regsize = mtcs_mode_get_regmode_natural_size/*!REGMODE_NATURAL_SIZE*/ (mtcsMode,innermode);
      unsigned int mregs, xregs;
      if (!can_div_away_from_zero_p (msize, regsize, &mregs) || !can_div_away_from_zero_p (xsize, regsize, &xregs) || mregs > xregs)
          return 0;
  }

  scalar_int_mode int_mode, int_innermode, from_mode;
  if ((GET_CODE (x) == ZERO_EXTEND || GET_CODE (x) == SIGN_EXTEND)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,mode, &int_mode)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,innermode, &int_innermode)
      && mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (XEXP (x, 0)), &from_mode)){
      /* If we are getting the low-order part of something that has been
     sign- or zero-extended, we can either just use the object being
     extended or make a narrower extension.  If we want an even smaller
     piece than the size of the object being extended, call ourselves
     recursively.

     This case is used mostly by combine and cse.  */
     n_debug("mtcsrtl.c mtcs_rtl_gen_lowpart_common 22 innermode:%d x:%p %s %d %d\n",
           innermode,x,GET_RTX_NAME(GET_CODE (x)),from_mode,int_mode);

      if (from_mode == int_mode)
          return XEXP (x, 0);
      else if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode) < mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,from_mode))
          return mtcs_rtl_gen_lowpart_common (self,int_mode, XEXP (x, 0));
      else if (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_mode) < mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,int_innermode))
          return gen_rtx_fmt_e (GET_CODE (x), int_mode, XEXP (x, 0));
  }else if (GET_CODE (x) == SUBREG || REG_P (x)
       || GET_CODE (x) == CONCAT || GET_CODE (x) == CONST_VECTOR
       || CONST_DOUBLE_AS_FLOAT_P (x) || CONST_SCALAR_INT_P (x)
       || CONST_POLY_INT_P (x)){
     n_debug("mtcsrtl.c mtcs_rtl_gen_lowpart_common 33 x:%p %s mode:%d innermode:%d\n",
           x,GET_RTX_NAME(GET_CODE (x)),mode,innermode);
    return mtcs_simplify_rtx_lowpart_subreg(mtcsSimplifyRtx,mode, x, innermode);//替换用mtcs_simplify_rtx_lowpart_subreg
  }
  /* Otherwise, we can't do this.  */
  return 0;
}
///**********************实现rtlhooks-def.h中的5个方法------运地时替换-----/

/**
 * rtlhooks-def.h rtlhooks.cc
 * reload_completed rtl.h 声明的全局变量
 * 原型 gen_lowpart rtl.h 中的struct rtl_hooks
 */
static rtx rtlGenLowpart(MtcsRTL *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  rtx result = mtcs_rtl_gen_lowpart_common (self,mode, x);

  if (result)
    return result;
  /* Handle SUBREGs and hard REGs that were rejected by
     simplify_gen_subreg.  */
  else if (REG_P (x) || GET_CODE (x) == SUBREG){
      result = mtcs_rtl_gen_lowpart_common (self,mode, mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,x));
      gcc_assert (result != 0);
      return result;
  }else{
      /* The only additional case we can do is MEM.  */
      gcc_assert (MEM_P (x));

      /* The following exposes the use of "x" to CSE.  */
      scalar_int_mode xmode;
      if (mtcs_mode_is_a <scalar_int_mode>(mtcsMode,GET_MODE (x), &xmode)  && mtcs_mode_get_size (mtcsMode,xmode) <= UNITS_PER_WORD
         && mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,mode, xmode)   && !reload_completed)
          return rtlGenLowpart(self,mode, mtcs_explow_force_reg/*!force_reg*/ (mtcsExplow,xmode, x));

      poly_int64 offset = mtcs_mode_byte_lowpart_offset (mtcsMode,mode, GET_MODE (x));
      return mtcs_rtl_adjust_address (self,x, mode, offset);
  }
}


/* Assuming that X is an rtx (e.g., MEM, REG or SUBREG) for a fixed-point
   number, return an rtx (MEM, SUBREG, or CONST_INT) that refers to the
   least-significant part of X.
   MODE specifies how big a part of X to return.

   If the requested operation cannot be done, 0 is returned.

   This is similar to gen_lowpart_general.  */
//原型 gen_lowpart_if_possible rtl.h rtlhooks.cc
rtx mtcs_rtl_gen_lowpart_if_possible (MtcsRTL *self,machine_mode mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

  rtx result = mtcs_rtl_gen_lowpart_common/*!gen_lowpart_common*/(self,mode, x);
  if (result)
    return result;
  else if (MEM_P (x)){
      /* This is the only other case we handle.  */
      poly_int64 offset = mtcs_mode_byte_lowpart_offset (mtcsMode,mode, GET_MODE (x));
      rtx new_rtx = mtcs_rtl_adjust_address_nv (self,x, mode, offset);
      if (! mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, XEXP (new_rtx, 0),
              mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(self,x),ERROR_MARK))
          return 0;

      return new_rtx;
  }else if (mode != GET_MODE (x) && GET_MODE (x) != VOIDmode && !SUBREG_P (x)
       && mtcs_rtl_validate_subreg (self,mode, GET_MODE (x), x,mtcs_mode_subreg_lowpart_offset (mtcsMode,mode, GET_MODE (x))))
    return mtcs_rtl_gen_lowpart_SUBREG (self,mode, x);
  else
    return 0;
}

/* Return subword OFFSET of operand OP.
   The word number, OFFSET, is interpreted as the word number starting
   at the low-order address.  OFFSET 0 is the low-order word if not
   WORDS_BIG_ENDIAN, otherwise it is the high-order word.

   If we cannot extract the required word, we return zero.  Otherwise,
   an rtx corresponding to the requested word will be returned.

   VALIDATE_ADDRESS is nonzero if the address should be validated.  Before
   reload has completed, a valid address will always be returned.  After
   reload, if a valid address cannot be returned, we return zero.

   If VALIDATE_ADDRESS is zero, we simply form the required address; validating
   it is the responsibility of the caller.

   MODE is the mode of OP in case it is a CONST_INT.

   ??? This is still rather broken for some cases.  The problem for the
   moment is that all callers of this thing provide no 'goal mode' to
   tell us to work with.  This exists because all callers were written
   in a word based SUBREG world.
   Now use of this function can be deprecated by simplify_subreg in most
   cases.
 */
//原型 operand_subword rtl.h emit-rtl.cc
rtx mtcs_rtl_operand_subword (MtcsRTL *self,rtx op, poly_uint64 offset, int validate_address,machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  if (mode == VOIDmode)
    mode = GET_MODE (op);
  gcc_assert (mode != VOIDmode);
  /* If OP is narrower than a word, fail.  */
  if (mode != mtcsMode->modes.M_BLKmode  && maybe_lt (mtcs_mode_get_size(mtcsMode,mode), UNITS_PER_WORD))
    return 0;
  /* If we want a word outside OP, return zero.  */
  if (mode != mtcsMode->modes.M_BLKmode  && maybe_gt ((offset + 1) * UNITS_PER_WORD, mtcs_mode_get_size (mtcsMode,mode)))
    return self->mtcs_const0_rtx;

  /* Form a new MEM at the requested address.  */
  if (MEM_P (op)){
      rtx new_rtx = mtcs_rtl_adjust_address_nv (self,op, word_mode, offset * UNITS_PER_WORD);
      if (! validate_address)
          return new_rtx;
      else if (reload_completed){
          if (! mtcs_recog_strict_memory_address_addr_space_p (mtcsRecog,word_mode,XEXP (new_rtx, 0),
                  mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(self,op),ERROR_MARK))
            return 0;
      } else
          return mtcs_rtl_replace_equiv_address (self,new_rtx, XEXP (new_rtx, 0),false);
  }

  /* Rest can be handled by simplify_subreg.  */
  return simplify_gen_subreg (word_mode, op, mode, (offset * UNITS_PER_WORD));
}

/* Return a memory reference like MEMREF, but with its address changed to
   ADDR.  The caller is asserting that the actual piece of memory pointed
   to is the same, just the form of the address is being changed, such as
   by putting something into a register.  INPLACE is true if any changes
   can be made directly to MEMREF or false if MEMREF must be treated as
   immutable.  */
//原型 replace_equiv_address rtl.h emit-rtl.cc
rtx mtcs_rtl_replace_equiv_address (MtcsRTL *self,rtx memref, rtx addr, bool inplace)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   /* change_address_1 copies the memory attribute structure without change
   and that's exactly what we want here.  */
   mtcs_func_update_temp_slot_address (mtcsFunc,XEXP (memref, 0), addr);
   return change_address_1 (self,memref, VOIDmode, addr, 1, inplace);
}

/* Likewise, but the reference is not required to be valid.  */
//原型 replace_equiv_address_nv rtl.h emit-rtl.cc
rtx mtcs_rtl_replace_equiv_address_nv (MtcsRTL *self,rtx memref, rtx addr, bool inplace)
{
  return change_address_1 (self,memref, VOIDmode, addr, 0, inplace);
}

/* Similar to `operand_subword', but never return 0.  If we can't
   extract the required subword, put OP into a register and try again.
   The second attempt must succeed.  We always validate the address in
   this case.

   MODE is the mode of OP, in case it is CONST_INT.  */
//原型 operand_subword_force rtl.h emit-rtl.cc
rtx mtcs_rtl_operand_subword_force (MtcsRTL *self,rtx op, poly_uint64 offset, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  rtx result = mtcs_rtl_operand_subword (self,op, offset, 1, mode);

  if (result)
    return result;

  if (mode != mtcsMode->modes.M_BLKmode && mode != VOIDmode){
      /* If this is a register which cannot be accessed by words, copy it
     to a pseudo register.  */
      if (REG_P (op))
          op = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,op);
      else
          op = mtcs_explow_force_reg (mtcsExplow,mode, op);
  }
  result = mtcs_rtl_operand_subword (self,op, offset, 1, mode);
  gcc_assert (result);

  return result;
}

//原型 virtual_stack_vars_rtx #define virtual_stack_vars_rtx  (global_rtl[GR_VIRTUAL_STACK_ARGS]) rtl.h
rtx mtcs_rtl_get_virtaul_stack_var_rtx (MtcsRTL *self)
{
  return self->x_global_rtl[MTCS_GR_VIRTUAL_STACK_ARGS];
}

/* Like change_address_1 with VALIDATE nonzero, but we are not saying in what
   way we are changing MEMREF, so we only preserve the alias set.  */
//原型 change_address emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_change_address (MtcsRTL *self,rtx memref, machine_mode mode, rtx addr)
{
  rtx new_rtx = change_address_1 (self,memref, mode, addr, 1, false);
  machine_mode mmode = GET_MODE (new_rtx);
  class mem_attrs *defattrs;

  mem_attrs attrs (*mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/(self,memref));
  defattrs =self->x_mode_mem_attrs/*!mode_mem_attrs*/[(int) mode];

  attrs.expr = NULL_TREE;
  attrs.offset_known_p = false;
  attrs.size_known_p = defattrs->size_known_p;
  attrs.size = defattrs->size;
  attrs.align = defattrs->align;

  /* If there are no changes, just return the original memory reference.  */
  if (new_rtx == memref){
      if (mem_attrs_eq_p (mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/(self,memref), &attrs))
          return new_rtx;

      new_rtx = gen_rtx_MEM (mmode, XEXP (memref, 0));
      MEM_COPY_ATTRIBUTES (new_rtx, memref);
  }

  set_mem_attrs (self,new_rtx, &attrs);
  return new_rtx;
}


///实现接口rtlhooks.h--------------------
static rtx genLowpart_cb (machine_mode mode, rtx x)
{
    MtcsCompile *mtcsCompile=mtcs_compile_get();
    MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcsCompile);
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    return rtlGenLowpart(mtcsRTL,mode,x);
}

static rtx genLowpartNoEmit_cb (machine_mode mode, rtx x)
{
    MtcsCompile *mtcsCompile=mtcs_compile_get();
    MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcsCompile);
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    return mtcs_rtl_gen_lowpart_if_possible(mtcsRTL,mode,x);
}

static rtx regNumSignBitCopies_cb (const_rtx, scalar_int_mode, scalar_int_mode,unsigned int *)
{
  return NULL;
}

static rtx regNonzeroBits_cb(const_rtx, scalar_int_mode, scalar_int_mode,unsigned HOST_WIDE_INT *)
{
   n_debug("mtcsrtl.c reg_nonzero_bits_general 执行的是 rtl_hooks.reg_nonzero_bits\n");

  return NULL;
}

static bool regTruncatedToMode_cb(machine_mode mode ATTRIBUTE_UNUSED,const_rtx x ATTRIBUTE_UNUSED)
{
  return false;
}


//实现接口MtcsBackupRestore 备分声明在machmode.h中的三个变量byte_mode word_mode ptr_mode
/**
 * In file included from ../../gcc-14/gcc/aet/mtcs/mtcsrtl.c:7:
../../gcc-14/gcc/aet/mtcs/mtcsrtl.c: In function ‘void set_cb(MtcsBackupRestore*)’:
../../gcc-14/gcc/rtl.h:4539:21: error: invalid use of ‘rtl_hooks::rtl_hooks’
 4539 | #define gen_lowpart rtl_hooks.gen_lowpart
 * 因为在rtl.h中已定义#define gen_lowpart rtl_hooks.gen_lowpart
 * 所以rtl_hooks.gen_lowpart=xxx 应改为gen_lowpart=xxx
 */
static   void backup_cb(MtcsBackupRestore *iface)
{
    MtcsRTL *self=(MtcsRTL *)iface->impl;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    /*
     备份的内容
    1.extern GTY(()) rtx const_int_rtx[MAX_SAVED_CONST_INT * 2 + 1];
    2.extern GTY(()) rtx const_tiny_rtx[4][(int) MAX_MACHINE_MODE];
    3.extern GTY(()) rtx pc_rtx;
    4.extern GTY(()) rtx ret_rtx;
    5.extern GTY(()) rtx simple_return_rtx;
    6.extern GTY(()) rtx_insn *invalid_insn_rtx;
    7.rtx x_global_rtl[GR_MAX];
    */
    MtcsRTLBackup *backup=(MtcsRTLBackup *)self->backup;
    int i,j;
    for(i=0;i<(MAX_SAVED_CONST_INT * 2 + 1);i++){
       backup->b_const_int_rtx[i]=const_int_rtx[i];
    }
    for(i=0;i<4;i++){
        for(j=0;j<MAX_MACHINE_MODE;j++){
            backup->b_const_tiny_rtx[i][j]=const_tiny_rtx[i][j];
        }
    }
    backup->b_pc_rtx=pc_rtx;
    backup->b_ret_rtx=ret_rtx;
    backup->b_simple_return_rtx=simple_return_rtx;
    backup->b_invalid_insn_rtx=invalid_insn_rtx;
    backup->b_const_true_rtx =const_true_rtx;
    //替换主机的 gen_lowpart是一个宏,不能用rtl_hooks.gen_lowpart=genLowpart_cb赋值
    gen_lowpart/*!rtl_hooks.gen_lowpart*/=genLowpart_cb;
    rtl_hooks.gen_lowpart_no_emit=genLowpartNoEmit_cb;
    rtl_hooks.reg_nonzero_bits=regNonzeroBits_cb;
    rtl_hooks.reg_num_sign_bit_copies=regNumSignBitCopies_cb;
    rtl_hooks.reg_truncated_to_mode=regTruncatedToMode_cb;
    //要保存设备的MAX_MACHINE_MODE小于主机的MAX_MACHINE_MODE
    int maxMachineMode=(int)mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode);
    for(i=0;i<(MAX_SAVED_CONST_INT * 2 + 1);i++){
        const_int_rtx[i]=self->const_int_rtx[i];
    }

    for(i=0;i<4;i++){
        for(j=0;j<maxMachineMode/*!MAX_MACHINE_MODE*/;j++){
            const_tiny_rtx[i][j]=self->const_tiny_rtx[i][j];
        }
    }
    pc_rtx=self->pc_rtx;
    ret_rtx=self->ret_rtx;
    simple_return_rtx=self->simple_return_rtx;
    invalid_insn_rtx=self->invalid_insn_rtx;
    const_true_rtx = self->const_true_rtx;

    backup->b_x_global_rtl[GR_STACK_POINTER] = stack_pointer_rtx;
    backup->b_x_global_rtl[GR_FRAME_POINTER] = frame_pointer_rtx;
    backup->b_x_global_rtl[GR_HARD_FRAME_POINTER] = hard_frame_pointer_rtx;
    backup->b_x_global_rtl[GR_ARG_POINTER] = arg_pointer_rtx;

    global_rtl[GR_STACK_POINTER] = self->x_global_rtl[MTCS_GR_STACK_POINTER];
    global_rtl[GR_FRAME_POINTER] = self->x_global_rtl[MTCS_GR_FRAME_POINTER];
    global_rtl[GR_HARD_FRAME_POINTER] = self->x_global_rtl[MTCS_GR_HARD_FRAME_POINTER];
    global_rtl[GR_ARG_POINTER] = self->x_global_rtl[MTCS_GR_ARG_POINTER];
};

static   void restore_cb(MtcsBackupRestore *iface)
{
   MtcsRTL *self=(MtcsRTL *)iface->impl;
   MtcsRTLBackup *backup=(MtcsRTLBackup *)self->backup;

   int i,j;
   for(i=0;i<(MAX_SAVED_CONST_INT * 2 + 1);i++){
      const_int_rtx[i]=backup->b_const_int_rtx[i];
   }
   for(i=0;i<4;i++){
      for(j=0;j<MAX_MACHINE_MODE;j++){
         const_tiny_rtx[i][j]=backup->b_const_tiny_rtx[i][j];
      }
   }
   pc_rtx=backup->b_pc_rtx;
   ret_rtx=backup->b_ret_rtx;
   simple_return_rtx=backup->b_simple_return_rtx;
   invalid_insn_rtx=backup->b_invalid_insn_rtx;
   const_true_rtx=backup->b_const_true_rtx;

   global_rtl[GR_STACK_POINTER] =backup->b_x_global_rtl[GR_STACK_POINTER];
   global_rtl[GR_FRAME_POINTER] = backup->b_x_global_rtl[GR_FRAME_POINTER];
   global_rtl[GR_HARD_FRAME_POINTER] = backup->b_x_global_rtl[GR_HARD_FRAME_POINTER];
   global_rtl[GR_ARG_POINTER] =backup->b_x_global_rtl[GR_ARG_POINTER];
}

/* Initialize some fake stack-frame MEM references for use in
   memory_move_secondary_cost.  */
//原型 init_fake_stack_mems rtl.h reginfo.cc
void mtcs_rtl_init_fake_stack_mems (MtcsRTL *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  int maxMachineMode=mtcs_mode_get_max_number(mtcsMode);
  int i;

  for (i = 0; i < maxMachineMode; i++)
    self->x_top_of_stack[i] = gen_rtx_MEM ((machine_mode) i, self->/*!stack_pointer_rtx*/x_global_rtl[MTCS_GR_STACK_POINTER]);
}


/* This function can be called multiple times to reinitialize the compiler
   back end when register classes or instruction sets have changed,
   before each function.  */
//原型 backend_init_target toplev.cc
static void backend_init_target (MtcsRTL *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
   MtcsLowerSubreg *mtcsLowerSubreg=mtcs_target_get_lower_subreg(mtcsTarget);
   MtcsCfgLoopanal *mtcsCfgLoopanal=mtcs_target_get_cfg_loopanal(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsReload *mtcsReload=mtcs_target_get_reload(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);

   /* This depends on stack_pointer_rtx.  */
   n_debug("mtcsrtl.c ----backend_init_target 00 init_fake_stack_mems %s\n",in_fnames[0]);
   mtcs_rtl_init_fake_stack_mems/*!init_fake_stack_mems*/(self);
   n_debug("mtcsrtl.c  ----backend_init_target 11 init_alias_target %s\n",in_fnames[0]);
   /* Sets static_base_value[HARD_FRAME_POINTER_REGNUM], which is
   mode-dependent.  */
   init_alias_target ();
   n_debug("mtcsrtl.c  ----backend_init_target 22 init_reload %s mtcs ira_use_lra_p:%d\n",in_fnames[0],mtcsIra->ira_use_lra_p);
   /* Depends on HARD_FRAME_POINTER_REGNUM.  */
   if (!mtcsIra->/*!ira_use_lra_p*/ira_use_lra_p)//ira_use_lra_p在host nvptx都是true
      mtcs_reload_init_reload/*!init_reload*/(mtcsReload);
   n_debug("mtcsrtl.c  ----backend_init_target 33 recog_init %s\n",in_fnames[0]);
   /* Depends on the enabled attribute.  */
   mtcs_recog_recog_init/*!recog_init*/ (mtcsRecog);
   n_debug("mtcsrtl.c  ----backend_init_target 44 init_dummy_function_start %s\n",in_fnames[0]);
   /* The following initialization functions need to generate rtl, so
   provide a dummy function context for them.  */
   mtcs_func_init_dummy_function_start/*!init_dummy_function_start*/(mtcsFunc);
   n_debug("mtcsrtl.c  ----backend_init_target 55 init_expmed %s\n",in_fnames[0]);
   /* rtx_cost is mode-dependent, so cached values need to be recomputed
   on a mode change.  */
   mtcs_expmed_init_expmed/*!init_expmed*/(mtcsExpmed);
   n_debug("mtcsrtl.c  ----backend_init_target 66 init_lower_subreg %s\n",in_fnames[0]);
   mtcs_lower_subreg_init_lower_subreg/*!init_lower_subreg*/(mtcsLowerSubreg);
   n_debug("mtcsrtl.c  ----backend_init_target 77 init_set_costs %s\n",in_fnames[0]);
   mtcs_cfg_loopanal_init_set_costs/*!init_set_costs*/(mtcsCfgLoopanal);
   n_debug("mtcsrtl.c  ----backend_init_target 88 init_expr_target %s\n",in_fnames[0]);
   mtcs_expr_init_expr_target/*!init_expr_target*/(mtcsExpr);
   n_debug("mtcsrtl.c  ----backend_init_target 99 ira_init %s\n",in_fnames[0]);
   mtcs_ira_init /*!ira_init*/(mtcsIra);

   /* We may need to recompute regno_save_code[] and regno_restore_code[]
   after a mode change as well.  */
   mtcsReload->target_reload.x_caller_save_initialized_p/*!caller_save_initialized_p*/= false;

   mtcs_func_expand_dummy_function_end/*!expand_dummy_function_end*/(mtcsFunc);
   n_debug("mtcsrtl.c ----backend_init_target 101 ira_init %s\n",in_fnames[0]);

}

//原型 initialize_rtl toplev.h toplev.cc
//原型 rtl_initialized toplev.cc
void mtcs_rtl_initialize_rtl (MtcsRTL *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsIraMgr *mtcsIraMgr=mtcs_target_get_ira_mgr(mtcsTarget);
   MtcsIra *mtcsIra = mtcs_ira_mgr_get_ira(mtcsIraMgr);
  /* Initialization done just once per compilation, but delayed
     till code generation.  */
  if (!self->rtl_initialized)
      mtcs_ira_init_once/*!ira_init_once*/(mtcsIra);

  self->rtl_initialized = true;

  /* Target specific RTL backend initialization.  */
  if (!self/*!this_target_rtl*/->target_specific_initialized){
     n_debug("mtcsrtl.c toplev.cc initialize_rtl 22 backend_init_target\n");
      backend_init_target(self);
      self/*!this_target_rtl*/->target_specific_initialized = true;
  }
}

/* Set the size for MEM to SIZE.  */
//原型 set_mem_size emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_size (MtcsRTL *self,rtx mem, poly_int64 size)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs (self,mem));
  attrs.size_known_p = true;
  attrs.size = size;
  set_mem_attrs (self,mem, &attrs);
}

/* Set the expr for MEM to EXPR.  */
//原型 set_mem_expr emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_expr (MtcsRTL *self,rtx mem, tree expr)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs (self,mem));
  attrs.expr = expr;
  set_mem_attrs (self,mem, &attrs);
}

/* Set the alias set of MEM to SET.  */
//原型set_mem_alias_set emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_alias_set (MtcsRTL *self,rtx mem, alias_set_type set)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   /* If the new and old alias sets don't conflict, something is wrong.  */
   gcc_checking_assert (mtcs_alias_alias_sets_conflict_p/*!alias_sets_conflict_p*/(mtcsAlias,
                  set, mtcs_rtl_get_mem_alias/*!MEM_ALIAS_SET*/(self,mem)));
   mem_attrs attrs (*mtcs_rtl_get_mem_attrs (self,mem));
   attrs.alias = set;
   set_mem_attrs (self,mem, &attrs);
}

/* Set the alignment of MEM to ALIGN bits.  */
//原型set_mem_align emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_align (MtcsRTL *self,rtx mem, unsigned int align)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs (self,mem));
  attrs.align = align;
  set_mem_attrs (self,mem, &attrs);
}

/* Identify REG as a probable pointer register and show its alignment
   as ALIGN, if nonzero.  */
//原型 mark_reg_pointer rtl.h emit-rtl.cc
void  mtcs_rtl_mark_reg_pointer (MtcsRTL *self,rtx reg, int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  if (! REG_POINTER (reg)){
      REG_POINTER (reg) = 1;

      if (align)
          mtcs_rtl_data_set_regno_pointer_align/*!REGNO_POINTER_ALIGN (REGNO (reg))*/(mtcsRtlData,REGNO (reg),align);/*!= align;*/
  }else if (align && align < mtcs_rtl_data_get_regno_pointer_align/*!REGNO_POINTER_ALIGN*/(mtcsRtlData,REGNO (reg)))
    /* We can no-longer be sure just how aligned this pointer is.  */
      mtcs_rtl_data_set_regno_pointer_align/*!REGNO_POINTER_ALIGN (REGNO (reg))*/(mtcsRtlData,REGNO (reg),align);/*! = align;*/
}

/* Given REF (a MEM) and T, either the type of X or the expression
   corresponding to REF, set the memory attributes.  OBJECTP is nonzero
   if we are making a new object of this type.  BITPOS is nonzero if
   there is an offset outstanding on T that will be applied later.  */
//原型 set_mem_attributes_minus_bitpos emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_attributes_minus_bitpos(MtcsRTL *self,rtx ref, tree t, int objectp,poly_int64 bitpos)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

   poly_int64 apply_bitpos = 0;
   tree type;
   class mem_attrs attrs, *defattrs, *refattrs;
   addr_space_t as;

   /* It can happen that type_for_mode was given a mode for which there
   is no language-level type.  In which case it returns NULL, which
   we can see here.  */
   if (t == NULL_TREE)
      return;

   type = TYPE_P (t) ? t : TREE_TYPE (t);
   if (type == error_mark_node)
      return;

   /* If we have already set DECL_RTL = ref, get_alias_set will get the
   wrong answer, as it assumes that DECL_RTL already has the right alias
   info.  Callers should not set DECL_RTL until after the call to
   set_mem_attributes.  */
   gcc_assert (!DECL_P (t) || ref != DECL_RTL_IF_SET (t));

   /* Get the alias set from the expression or type (perhaps using a
   front-end routine) and use it.  */
   attrs.alias = mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,t);

   MEM_VOLATILE_P (ref) |= TYPE_VOLATILE (type);
   MEM_POINTER (ref) = POINTER_TYPE_P (type);

   /* Default values from pre-existing memory attributes if present.  */
   refattrs = MEM_ATTRS (ref);
   if (refattrs){
      /* ??? Can this ever happen?  Calling this routine on a MEM that
      already carries memory attributes should probably be invalid.  */
      attrs.expr = refattrs->expr;
      attrs.offset_known_p = refattrs->offset_known_p;
      attrs.offset = refattrs->offset;
      attrs.size_known_p = refattrs->size_known_p;
      attrs.size = refattrs->size;
      attrs.align = refattrs->align;
   }else{ /* Otherwise, default values from the mode of the MEM reference.  */
      defattrs = self->x_mode_mem_attrs/*!mode_mem_attrs*/[(int) GET_MODE (ref)];
      gcc_assert (!defattrs->expr);
      gcc_assert (!defattrs->offset_known_p);

      /* Respect mode size.  */
      attrs.size_known_p = defattrs->size_known_p;
      attrs.size = defattrs->size;
      /* ??? Is this really necessary?  We probably should always get
      the size from the type below.  */

      /* Respect mode alignment for STRICT_ALIGNMENT targets if T is a type;
      if T is an object, always compute the object alignment below.  */
      if (TYPE_P (t))
         attrs.align = defattrs->align;
      else
         attrs.align = BITS_PER_UNIT;
      /* ??? If T is a type, respecting mode alignment may *also* be wrong
      e.g. if the type carries an alignment attribute.  Should we be
      able to simply always use TYPE_ALIGN?  */
   }

   /* We can set the alignment from the type if we are making an object or if
   this is an INDIRECT_REF.  */
   if (objectp || TREE_CODE (t) == INDIRECT_REF)
      attrs.align = MAX (attrs.align, TYPE_ALIGN (type));

   /* If the size is known, we can set that.  */
   tree new_size = TYPE_SIZE_UNIT (type);
   /* The address-space is that of the type.  */
   as = TYPE_ADDR_SPACE (type);
   /* If T is not a type, we may be able to deduce some more information about
   the expression.  */
   if (! TYPE_P (t)){
      tree base;

      if (TREE_THIS_VOLATILE (t))
         MEM_VOLATILE_P (ref) = 1;

      /* Now remove any conversions: they don't change what the underlying
      object is.  Likewise for SAVE_EXPR.  */
      while (CONVERT_EXPR_P (t) || TREE_CODE (t) == VIEW_CONVERT_EXPR || TREE_CODE (t) == SAVE_EXPR)
         t = TREE_OPERAND (t, 0);

      /* Note whether this expression can trap.  */
      MEM_NOTRAP_P (ref) = !tree_could_trap_p (t);

      base = get_base_address (t);
      if (base){
         if (DECL_P (base)  && TREE_READONLY (base) && (TREE_STATIC (base) || DECL_EXTERNAL (base)) && !TREE_THIS_VOLATILE (base))
            MEM_READONLY_P (ref) = 1;

         /* Mark static const strings readonly as well.  */
         if (TREE_CODE (base) == STRING_CST && TREE_READONLY (base) && TREE_STATIC (base))
            MEM_READONLY_P (ref) = 1;

         /* Address-space information is on the base object.  */
         if (TREE_CODE (base) == MEM_REF || TREE_CODE (base) == TARGET_MEM_REF)
            as = TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (TREE_OPERAND (base, 0))));
         else
            as = TYPE_ADDR_SPACE (TREE_TYPE (base));
      }

      /* If this expression uses it's parent's alias set, mark it such
      that we won't change it.  */
      if (mtcs_alias_component_uses_parent_alias_set_from/*!component_uses_parent_alias_set_from*/(mtcsAlias,t) != NULL_TREE)
         MEM_KEEP_ALIAS_SET_P (ref) = 1;

      /* If this is a decl, set the attributes of the MEM from it.  */
      if (DECL_P (t)){
         attrs.expr = t;
         attrs.offset_known_p = true;
         attrs.offset = 0;
         apply_bitpos = bitpos;
         new_size = DECL_SIZE_UNIT (t);
      }
      /* ???  If we end up with a constant or a descriptor do not
      record a MEM_EXPR.  */
      else if (CONSTANT_CLASS_P (t) || TREE_CODE (t) == CONSTRUCTOR)
         ;

      /* If this is a field reference, record it.  */
      else if (TREE_CODE (t) == COMPONENT_REF){
         attrs.expr = t;
         attrs.offset_known_p = true;
         attrs.offset = 0;
         apply_bitpos = bitpos;
         if (DECL_BIT_FIELD (TREE_OPERAND (t, 1)))
            new_size = DECL_SIZE_UNIT (TREE_OPERAND (t, 1));
      }else{/* Else record it.  */
         gcc_assert (handled_component_p (t)  || TREE_CODE (t) == MEM_REF  || TREE_CODE (t) == TARGET_MEM_REF);
         attrs.expr = t;
         attrs.offset_known_p = true;
         attrs.offset = 0;
         apply_bitpos = bitpos;
      }

      /* If this is a reference based on a partitioned decl replace the
      base with a MEM_REF of the pointer representative we created
      during stack slot partitioning.  */
      if (attrs.expr   && VAR_P (base)  && ! is_global_var (base)
      && cfun->gimple_df->decls_to_pointers != NULL){
         tree *namep = cfun->gimple_df->decls_to_pointers->get (base);
         if (namep){
            attrs.expr = unshare_expr (attrs.expr);
            tree *orig_base = &attrs.expr;
            while (handled_component_p (*orig_base))
               orig_base = &TREE_OPERAND (*orig_base, 0);
            if (TREE_CODE (*orig_base) == MEM_REF || TREE_CODE (*orig_base) == TARGET_MEM_REF)
               TREE_OPERAND (*orig_base, 0) = *namep;
            else{
               tree aptrt =  mtcs_alias_reference_alias_ptr_type/*!reference_alias_ptr_type*/(mtcsAlias,*orig_base);
               *orig_base = build2 (MEM_REF, TREE_TYPE (*orig_base),*namep, mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,aptrt, 0));
            }
         }
      }

      /* Compute the alignment.  */
      unsigned int obj_align;
      unsigned HOST_WIDE_INT obj_bitpos;
      mtcs_builtins_get_object_alignment_1/*!get_object_alignment_1*/(mtcsBuiltins,t, &obj_align, &obj_bitpos);
      unsigned int diff_align = known_alignment (obj_bitpos - bitpos);
      if (diff_align != 0)
         obj_align = MIN (obj_align, diff_align);
      attrs.align = MAX (attrs.align, obj_align);
   }
   poly_uint64 const_size;
   if (poly_int_tree_p (new_size, &const_size)){
      attrs.size_known_p = true;
      attrs.size = const_size;
   }

   /* If we modified OFFSET based on T, then subtract the outstanding
   bit position offset.  Similarly, increase the size of the accessed
   object to contain the negative offset.  */
   if (maybe_ne (apply_bitpos, 0)){
      gcc_assert (attrs.offset_known_p);
      poly_int64 bytepos = bits_to_bytes_round_down (apply_bitpos);
      attrs.offset -= bytepos;
      if (attrs.size_known_p)
         attrs.size += bytepos;
   }
   /* Now set the attributes we computed above.  */
   attrs.addrspace = as;
   set_mem_attrs(self,ref, &attrs);
}

//原型 set_mem_attributes emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_attributes(MtcsRTL *self,rtx ref, tree t, int objectp)
{
   mtcs_rtl_set_mem_attributes_minus_bitpos/*!set_mem_attributes_minus_bitpos*/(self,ref, t, objectp, 0);
}

/* Set the address space of MEM to ADDRSPACE (target-defined).  */
//原型 set_mem_addr_space emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_mem_addr_space (MtcsRTL *self,rtx mem, addr_space_t addrspace)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/(self,mem));
  attrs.addrspace = addrspace;
  set_mem_attrs (self,mem, &attrs);
}

//原型 HAVE_PRE_DECREMENT rtl.h 每个平台可能不一样
bool mtcs_rtl_have_pre_decrement(MtcsRTL *self)
{
    return false;
}
//原型 HAVE_POST_DECREMENT rtl.h 每个平台可能不一样
bool mtcs_rtl_have_post_decrement(MtcsRTL *self)
{
    return false;
}

/* Return a memory reference like MEMREF, but whose address is changed by
   adding OFFSET, an RTX, to it.  POW2 is the highest power of two factor
   known to be in OFFSET (possibly 1).  */
//原型 offset_address emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_offset_address (MtcsRTL *self,rtx memref, rtx offset, unsigned HOST_WIDE_INT pow2)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

  rtx new_rtx, addr = XEXP (memref, 0);
  machine_mode address_mode;
  class mem_attrs *defattrs;

  mem_attrs attrs (*get_mem_attrs (memref));
  address_mode = mtcs_rtl_get_address_mode/*!get_address_mode*/(self,memref);
  new_rtx = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, address_mode, addr, offset);

  /* At this point we don't know _why_ the address is invalid.  It
     could have secondary memory references, multiplies or anything.

     However, if we did go and rearrange things, we can wind up not
     being able to recognize the magic around pic_offset_table_rtx.
     This stuff is fragile, and is yet another example of why it is
     bad to expose PIC machinery too early.  */
  if (! mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,GET_MODE (memref), new_rtx, attrs.addrspace)
      && GET_CODE (addr) == PLUS  && XEXP (addr, 0) == mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(self)){
      addr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (addr), addr);
      new_rtx = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, address_mode, addr, offset);
  }

  mtcs_func_update_temp_slot_address (mtcsFunc,XEXP (memref, 0), new_rtx);
  new_rtx = change_address_1(self,memref, VOIDmode, new_rtx, 1, false);

  /* If there are no changes, just return the original memory reference.  */
  if (new_rtx == memref)
    return new_rtx;

  /* Update the alignment to reflect the offset.  Reset the offset, which
     we don't know.  */
  defattrs = self->x_mode_mem_attrs/*!mode_mem_attrs*/[(int) GET_MODE (new_rtx)];
  attrs.offset_known_p = false;
  attrs.size_known_p = defattrs->size_known_p;
  attrs.size = defattrs->size;
  attrs.align = MIN (attrs.align, pow2 * BITS_PER_UNIT);
  set_mem_attrs (self,new_rtx, &attrs);
  return new_rtx;
}

/* Return a memory reference like MEMREF, but with its mode changed
   to MODE and its address changed to ADDR, which is assumed to be
   MEMREF offset by OFFSET bytes.  If VALIDATE is
   nonzero, the memory address is forced to be valid.  */
//原型 adjust_automodify_address_1 emit-rtl.h emit-rtl.cc
rtx mtcs_rtl_adjust_automodify_address_1 (MtcsRTL *self,rtx memref, machine_mode mode, rtx addr,
                 poly_int64 offset, int validate)
{
  memref = change_address_1(self,memref, VOIDmode, addr, validate, false);
  return mtcs_rtl_adjust_address_1/*!adjust_address_1*/(self,memref, mode, offset, validate, 0, 0, 0);
}

/* Macros to access fconst0 and fconst1 via machine modes.  */
#define MTCS_FCONST0(mode)   self->fconst0[mode - mtcsMode->modes.M_QQmode]
#define MTCS_FCONST1(mode)   self->fconst1[mode - mtcsMode->modes.M_HAmode]
/* Create some permanent unique rtl objects shared between all functions.  */
//原型 init_emit_once rtl.h emit-rtl.cc
void mtcs_rtl_init_emit_once (MtcsRTL *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

  int storeFlagValue=mtcs_real_get_store_flag_value(mtcsReal);
  int i;
  machine_mode mode;
  scalar_float_mode double_mode;
  opt_scalar_mode smode_iter;

  /* Initialize the CONST_INT, CONST_WIDE_INT, CONST_DOUBLE,
     CONST_FIXED, and memory attribute hash tables.  */
  /*
  const_int_htab = hash_table<const_int_hasher>::create_ggc (37);

#if TARGET_SUPPORTS_WIDE_INT //host=1 nvptx=1
  const_wide_int_htab = hash_table<const_wide_int_hasher>::create_ggc (37);
#endif
  const_double_htab = hash_table<const_double_hasher>::create_ggc (37);

  if (NUM_POLY_INT_COEFFS > 1)
    const_poly_int_htab = hash_table<const_poly_int_hasher>::create_ggc (37);

  const_fixed_htab = hash_table<const_fixed_hasher>::create_ggc (37);

  reg_attrs_htab = hash_table<reg_attr_hasher>::create_ggc (37);
  */

#ifdef INIT_EXPANDERS
  /* This is to initialize {init|mark|free}_machine_status before the first
     call to push_function_context_to.  This is needed by the Chill front
     end which calls push_function_context_to before the first call to
     init_function_start.  */
  INIT_EXPANDERS;
#endif

  /* Create the unique rtx's for certain rtx codes and operand values.  */

  /* Don't use gen_rtx_CONST_INT here since gen_rtx_CONST_INT in this case
     tries to use these variables.  */
  for (i = - MAX_SAVED_CONST_INT; i <= MAX_SAVED_CONST_INT; i++)
    self->const_int_rtx[i + MAX_SAVED_CONST_INT] = gen_rtx_raw_CONST_INT (VOIDmode, (HOST_WIDE_INT) i);

  if (STORE_FLAG_VALUE >= - MAX_SAVED_CONST_INT && STORE_FLAG_VALUE <= MAX_SAVED_CONST_INT)
    self->const_true_rtx = self->const_int_rtx[storeFlagValue/*!STORE_FLAG_VALUE*/ + MAX_SAVED_CONST_INT];
  else
    self->const_true_rtx = mtcs_rtl_gen_rtx_CONST_INT/*!gen_rtx_CONST_INT*/(self,VOIDmode, storeFlagValue/*!STORE_FLAG_VALUE*/ );
  /*原型 const0_rtx const1_rtx const2_rtx constm1_rtx rtl.h
#define const0_rtx (const_int_rtx[MAX_SAVED_CONST_INT])
#define const1_rtx  (const_int_rtx[MAX_SAVED_CONST_INT+1])
#define const2_rtx  (const_int_rtx[MAX_SAVED_CONST_INT+2])
#define constm1_rtx (const_int_rtx[MAX_SAVED_CONST_INT-1])
  */
  self->mtcs_const0_rtx=self->const_int_rtx[MAX_SAVED_CONST_INT];
  self->mtcs_const1_rtx=self->const_int_rtx[MAX_SAVED_CONST_INT+1];
  self->mtcs_const2_rtx=self->const_int_rtx[MAX_SAVED_CONST_INT+2];
  self->mtcs_constm1_rtx=self->const_int_rtx[MAX_SAVED_CONST_INT-1];

  mtcs_real_init_once(mtcsReal);
  /*
   * 被 mtcs_real_init_once 替换
    double_mode = float_mode_for_size(DOUBLE_TYPE_SIZE).require ();

    real_from_integer(mtcsReal,&dconst0, double_mode, 0, SIGNED);
    real_from_integer(mtcsReal,&dconst1, double_mode, 1, SIGNED);
    real_from_integer(mtcsReal,&dconst2, double_mode, 2, SIGNED);

    dconstm0 = dconst0;
    dconstm0.sign = 1;

    dconstm1 = dconst1;
    dconstm1.sign = 1;

    dconsthalf = dconst1;
    SET_REAL_EXP (&dconsthalf, REAL_EXP (&dconsthalf) - 1);

    real_inf (&dconstinf);
    real_inf (&dconstninf, true);
  */

  for (i = 0; i < 3; i++){
      const REAL_VALUE_TYPE *const r = (i == 0 ? &mtcsReal->dconst0 : i == 1 ? &mtcsReal->dconst1 : &mtcsReal->dconst2);

      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_FLOAT)
          self->const_tiny_rtx[i][(int) mode] = mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(self,*r, mode);

      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_DECIMAL_FLOAT)
          self->const_tiny_rtx[i][(int) mode] = mtcs_rtl_const_double_from_real_value/*!const_double_from_real_value*/(self,*r, mode);

      self->const_tiny_rtx[i][(int) VOIDmode] = mtcs_rtl_GEN_INT/*!GEN_INT*/(self,i);

      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_INT)
          self->const_tiny_rtx[i][(int) mode] = mtcs_rtl_GEN_INT/*!GEN_INT*/(self,i);

      for (mode =mtcsMode->modesMinMax.min_PARTIAL_INT/*!MIN_MODE_PARTIAL_INT*/;
              mode <= mtcsMode->modesMinMax.max_PARTIAL_INT/*!MAX_MODE_PARTIAL_INT*/;   mode = (machine_mode)((int)(mode) + 1))
          self->const_tiny_rtx[i][(int) mode] = mtcs_rtl_GEN_INT/*!GEN_INT*/(self,i);
  }

  self->const_tiny_rtx[3][(int) VOIDmode] =self->mtcs_constm1_rtx/*!constm1_rtx*/;

  MTCS_FOR_EACH_MODE_IN_CLASS(mtcsMode,mode, MODE_INT)
    self->const_tiny_rtx[3][(int) mode] = self->mtcs_constm1_rtx/*!constm1_rtx*/;;

  /* For BImode, 1 and -1 are unsigned and signed interpretations
     of the same value.  */
  for (mode = mtcsMode->modesMinMax.min_BOOL/*!MIN_MODE_BOOL*/;
       mode <= mtcsMode->modesMinMax.max_BOOL/*!MAX_MODE_BOOL*/; mode = (machine_mode)((int)(mode) + 1)){
      self->const_tiny_rtx[0][(int) mode] = self->mtcs_const0_rtx;
      if (mode ==mtcsMode->modes.M_BImode){
          self->const_tiny_rtx[1][(int) mode] = self->const_true_rtx;
          self->const_tiny_rtx[3][(int) mode] = self->const_true_rtx;
      }else{
          self->const_tiny_rtx[1][(int) mode] = self->mtcs_const1_rtx;
          self->const_tiny_rtx[3][(int) mode] = self->mtcs_constm1_rtx;
      }
  }

  for (mode = mtcsMode->modesMinMax.min_PARTIAL_INT/*!MIN_MODE_PARTIAL_INT*/;
       mode <= mtcsMode->modesMinMax.max_PARTIAL_INT/*!MAX_MODE_PARTIAL_INT*/; mode = (machine_mode)((int)(mode) + 1))
      self->const_tiny_rtx[3][(int) mode] = self->mtcs_constm1_rtx;

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_COMPLEX_INT){
      rtx inner = self->const_tiny_rtx[0][(int)mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode)];
      self->const_tiny_rtx[0][(int) mode] = gen_rtx_CONCAT (mode, inner, inner);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_COMPLEX_FLOAT){
      rtx inner = self->const_tiny_rtx[0][(int)mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode)];
      self->const_tiny_rtx[0][(int) mode] = gen_rtx_CONCAT (mode, inner, inner);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_BOOL){
      const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
      const_tiny_rtx[3][(int) mode] = gen_const_vector(self,mode, 3);
      if (mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode) == mtcsMode->modes.M_BImode)
        /* As for BImode, "all 1" and "all -1" are unsigned and signed
           interpretations of the same value.  */
        self->const_tiny_rtx[1][(int) mode] = self->const_tiny_rtx[3][(int) mode];
      else
         self->const_tiny_rtx[1][(int) mode] = gen_const_vector(self,mode, 1);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_INT){
      self->const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
      self->const_tiny_rtx[1][(int) mode] = gen_const_vector(self,mode, 1);
      self->const_tiny_rtx[3][(int) mode] = gen_const_vector(self,mode, 3);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_FLOAT){
      self->const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
      self->const_tiny_rtx[1][(int) mode] = gen_const_vector(self,mode, 1);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,smode_iter, MODE_FRACT){
      scalar_mode smode = smode_iter.require ();
      MTCS_FCONST0(smode).data.high = 0;
      MTCS_FCONST0(smode).data.low = 0;
      MTCS_FCONST0(smode).mode = smode;
      self->const_tiny_rtx[0][(int) smode]=mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(self,MTCS_FCONST0(smode), smode);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,smode_iter, MODE_UFRACT){
      scalar_mode smode = smode_iter.require ();
      MTCS_FCONST0(smode).data.high = 0;
      MTCS_FCONST0(smode).data.low = 0;
      MTCS_FCONST0(smode).mode = smode;
      self->const_tiny_rtx[0][(int) smode] = mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(self,MTCS_FCONST0(smode), smode);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,smode_iter, MODE_ACCUM){
      scalar_mode smode = smode_iter.require ();
      MTCS_FCONST0(smode).data.high = 0;
      MTCS_FCONST0(smode).data.low = 0;
      MTCS_FCONST0(smode).mode = smode;
      self->const_tiny_rtx[0][(int) smode]= mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(self,MTCS_FCONST0(smode), smode);

      /* We store the value 1.  */
      MTCS_FCONST1(smode).data.high = 0;
      MTCS_FCONST1(smode).data.low = 0;
      MTCS_FCONST1(smode).mode = smode;
      MTCS_FCONST1(smode).data= double_int_one.lshift (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,smode),
                 HOST_BITS_PER_DOUBLE_INT,
                 mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,smode));
      self->const_tiny_rtx[1][(int) smode]= mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(self,MTCS_FCONST1(smode), smode);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,smode_iter, MODE_UACCUM){
      scalar_mode smode = smode_iter.require ();
      MTCS_FCONST1(smode).data.high = 0;
      MTCS_FCONST1(smode).data.low = 0;
      MTCS_FCONST1(smode).mode = smode;
      self->const_tiny_rtx[0][(int) smode]= mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(self,MTCS_FCONST0(smode), smode);

      /* We store the value 1.  */
      MTCS_FCONST1(smode).data.high = 0;
      MTCS_FCONST1(smode).data.low = 0;
      MTCS_FCONST1(smode).mode = smode;
      MTCS_FCONST1(smode).data= double_int_one.lshift (mtcs_mode_get_fbit/*!GET_MODE_FBIT*/(mtcsMode,smode),
                 HOST_BITS_PER_DOUBLE_INT, mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,smode));
      self->const_tiny_rtx[1][(int) smode] = mtcs_rtl_const_fixed_from_fixed_value/*!CONST_FIXED_FROM_FIXED_VALUE*/(self,MTCS_FCONST1(smode), smode);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_FRACT){
      self->const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_UFRACT){
      self->const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_ACCUM){
      self->const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
      self->const_tiny_rtx[1][(int) mode] = gen_const_vector(self,mode, 1);
  }

  MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_VECTOR_UACCUM){
      self->const_tiny_rtx[0][(int) mode] = gen_const_vector(self,mode, 0);
      self->const_tiny_rtx[1][(int) mode] = gen_const_vector(self,mode, 1);
  }

  for (i = (int) CCmode; i < (int)mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode); ++i)
    if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,(machine_mode) i) == MODE_CC)
      self->const_tiny_rtx[0][i] = self->mtcs_const0_rtx;

  self->pc_rtx = gen_rtx_fmt_ (PC, VOIDmode);
  self->ret_rtx = gen_rtx_fmt_ (RETURN, VOIDmode);
  self->simple_return_rtx = gen_rtx_fmt_ (SIMPLE_RETURN, VOIDmode);
  self->invalid_insn_rtx = gen_rtx_INSN (VOIDmode,
                   /*prev_insn=*/NULL,
                   /*next_insn=*/NULL,
                   /*bb=*/NULL,
                   /*pattern=*/NULL_RTX,
                   /*location=*/-1,
                   CODE_FOR_nothing,
                   /*reg_notes=*/NULL_RTX);
}

//原型 max_label_num rtl.h emit-rtl.cc label_num原型定义在emit-rtl.cc
int mtcs_rtl_get_label_num(MtcsRTL *self)
{
    return self->label_num;
}

/* Return 1 + the largest label number used so far in the current function.  */
//原型 max_label_num rtl.h emit-rtl.cc
int mtcs_rtl_max_label_num (MtcsRTL *self)
{
  return self->label_num;
}

void mtcs_rtl_set_label_num(MtcsRTL *self,int value)
{
    self->label_num=value;
}

//原型 stack_limit_rtx rtl.h toplev.cc
rtx mtcs_rtl_get_stack_limit_rtx(MtcsRTL *self)
{
    return self->stack_limit_rtx;
}


/* Copy REG's attributes from X, if X has any attributes.  If REG and X
   have different modes, REG is a (possibly paradoxical) lowpart of X.  */
//原型 set_reg_attrs_from_value emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_reg_attrs_from_value (MtcsRTL *self,rtx reg, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  poly_int64 offset;
  bool can_be_reg_pointer = true;

  /* Don't call mark_reg_pointer for incompatible pointer sign
     extension.  */
  while (GET_CODE (x) == SIGN_EXTEND
     || GET_CODE (x) == ZERO_EXTEND
     || GET_CODE (x) == TRUNCATE
     || (GET_CODE (x) == SUBREG && mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(self,x)))
    {
      /*
#if defined(POINTERS_EXTEND_UNSIGNED) //host=1 nvptx=0
      if (((GET_CODE (x) == SIGN_EXTEND && POINTERS_EXTEND_UNSIGNED)
       || (GET_CODE (x) == ZERO_EXTEND && ! POINTERS_EXTEND_UNSIGNED)
       || (paradoxical_subreg_p (x)
           && ! (SUBREG_PROMOTED_VAR_P (x)
             && SUBREG_CHECK_PROMOTED_SIGN (x,
                            POINTERS_EXTEND_UNSIGNED))))
      && !targetm.have_ptr_extend ())
    can_be_reg_pointer = false;
#endif
      */
      x = XEXP (x, 0);
    }

  /* Hard registers can be reused for multiple purposes within the same
     function, so setting REG_ATTRS, REG_POINTER and REG_POINTER_ALIGN
     on them is wrong.  */
  if (mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,reg))
    return;

  offset = mtcs_mode_byte_lowpart_offset/*!byte_lowpart_offset*/(mtcsMode,GET_MODE (reg), GET_MODE (x));
  if (MEM_P (x)){
      if (mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(self,x))
          REG_ATTRS (reg) = get_reg_attrs(self,
                mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(self,x),MEM_OFFSET (x) + offset);
      if (can_be_reg_pointer && MEM_POINTER (x))
          mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(self,reg, 0);
  }else if (REG_P (x)){
      if (REG_ATTRS (x))
          update_reg_offset(self,reg, x, offset);
      if (can_be_reg_pointer && REG_POINTER (x))
          mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(self,reg,
                mtcs_rtl_data_get_regno_pointer_align/*!REGNO_POINTER_ALIGN*/(mtcsRtlData,REGNO (x)));
  }
}

/* Set the register attributes for registers contained in PARM_RTX.
   Use needed values from memory attributes of MEM.  */
//原型 set_reg_attrs_for_parm emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_reg_attrs_for_parm (MtcsRTL *self,rtx parm_rtx, rtx mem)
{
  if (REG_P (parm_rtx))
      mtcs_rtl_set_reg_attrs_from_value/*!set_reg_attrs_from_value*/(self,parm_rtx, mem);
  else if (GET_CODE (parm_rtx) == PARALLEL){
      /* Check for a NULL entry in the first slot, used to indicate that the
     parameter goes both on the stack and in registers.  */
      int i = XEXP (XVECEXP (parm_rtx, 0, 0), 0) ? 0 : 1;
      for (; i < XVECLEN (parm_rtx, 0); i++){
          rtx x = XVECEXP (parm_rtx, 0, i);
          if (REG_P (XEXP (x, 0)))
            REG_ATTRS (XEXP (x, 0)) = get_reg_attrs(self,
                  mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(self,mem),INTVAL (XEXP (x, 1)));
     }
  }
}

/* Assign the RTX X to parameter declaration T.  BY_REFERENCE_P is true
   if the ABI requires the parameter to be passed by reference.  */
//原型 set_decl_incoming_rtl emit-rtl.h emit-rtl.cc
void mtcs_rtl_set_decl_incoming_rtl (MtcsRTL *self,tree t, rtx x, bool by_reference_p)
{
  DECL_INCOMING_RTL (t) = x;
  if (x && !by_reference_p)
      mtcs_rtl_set_reg_attrs_for_decl_rtl/*!set_reg_attrs_for_decl_rtl*/(self,t, x);
}

/* Return true iff X, assumed to be a SUBREG,
   refers to the least significant part of its containing reg.
   If X is not a SUBREG, always return true (it is its own low part!).  */
//原型 subreg_lowpart_p rtl.h emit-rtl.cc
bool mtcs_rtl_subreg_lowpart_p (MtcsRTL *self,const_rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (GET_CODE (x) != SUBREG)
    return true;
  else if (GET_MODE (SUBREG_REG (x)) == VOIDmode)
    return false;

  return known_eq (mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,GET_MODE (x),
                      GET_MODE (SUBREG_REG (x))), SUBREG_BYTE (x));
}

/* Likewise return true if X is a subreg that is smaller than the inner
   register.  Use read_modify_subreg_p to test whether writing to such
   a subreg preserves any part of the inner register.  */
//原型 partial_subreg_p rtl.h rtl.h
bool mtcs_rtl_partial_subreg_p (MtcsRTL *self,const_rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  if (GET_CODE (x) != SUBREG)
    return false;
  return mtcs_mode_partial_subreg_p/*!partial_subreg_p*/(mtcsMode,GET_MODE (x), GET_MODE (SUBREG_REG (x)));
}

/* If loads from memories of mode MODE always sign or zero extend,
   return SIGN_EXTEND or ZERO_EXTEND as appropriate.  Return UNKNOWN
   otherwise.  */
//原型 rtx_code load_extend_op rtl.h
rtx_code mtcs_rtl_load_extend_op (MtcsRTL *self,machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  scalar_int_mode int_mode;
  if (mtcs_mode_is_a <scalar_int_mode> (mtcsMode,mode, &int_mode)
      && mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,int_mode) < BITS_PER_WORD)
    return mtcs_mode_load_extend_op/*!LOAD_EXTEND_OP*/(mtcsMode,int_mode);
  return UNKNOWN;
}


/* Clear the offset of MEM.  */
//原型 clear_mem_offset emit-rtl.h emit-rtl.cc
void mtcs_rtl_clear_mem_offset (MtcsRTL *self,rtx mem)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/(self,mem));
  attrs.offset_known_p = false;
  set_mem_attrs (self,mem, &attrs);
}

/* Assign the RTX X to declaration T.  */
//原型 set_decl_rtl tree.h emit-rtl.cc
//#define SET_DECL_RTL(NODE, RTL) set_decl_rtl (NODE, RTL)
void mtcs_rtl_set_decl_rtl (MtcsRTL *self,tree t, rtx x)
{
  DECL_WRTL_CHECK (t)->decl_with_rtl.rtl = x;
  if (x)
     mtcs_rtl_set_reg_attrs_for_decl_rtl/*!set_reg_attrs_for_decl_rtl*/(self,t, x);
}

//原型 gen_rtx_REG rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_rtx_REG (MtcsRTL *self,machine_mode mode, unsigned int regno)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   /* In case the MD file explicitly references the frame pointer, have
   all such references point to the same frame pointer.  This is
   used during frame pointer elimination to distinguish the explicit
   references to these registers from pseudos that happened to be
   assigned to them.

   If we have eliminated the frame pointer or arg pointer, we will
   be using it as a normal register, for example as a spill
   register.  In such cases, we might be accessing it in a mode that
   is not Pmode and therefore cannot use the pre-allocated rtx.

   Also don't do this when we are making new REGs in reload, since
   we don't want to get confused with the real pointers.  */

   if (mode ==mtcs_mode_get_Pmode(mtcsMode) && !reload_in_progress && !lra_in_progress){
      if (regno ==mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg)
            && (!reload_completed || mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/))
         return mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(self);

      if (!mtcs_reg_hard_frame_pointer_is_frame_pointer/*!HARD_FRAME_POINTER_IS_FRAME_POINTER*/(mtcsReg)
            && regno ==mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
            && (!reload_completed || mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/))
         return mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(self);
      if(!mtcs_reg_hard_frame_pointer_is_arg_pointer/*!HARD_FRAME_POINTER_IS_ARG_POINTER*/(mtcsReg)){
         int argPointerRegnum= mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg);
         if(mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg)!= argPointerRegnum/*!ARG_POINTER_REGNUM*/
               && regno==argPointerRegnum/*!ARG_POINTER_REGNUM*/)
         return mtcs_rtl_get_arg_pointer_rtx(self);
      }

   /*!
   #if !HARD_FRAME_POINTER_IS_ARG_POINTER
   if (FRAME_POINTER_REGNUM != ARG_POINTER_REGNUM
   && regno == ARG_POINTER_REGNUM)
   return arg_pointer_rtx;
   #endif
   */
#ifdef RETURN_ADDRESS_POINTER_REGNUM //host=0 nvptx=0
   if (regno == RETURN_ADDRESS_POINTER_REGNUM)
      return return_address_pointer_rtx;
#endif
      int picOffsetTableRegnum= mtcs_reg_get_pic_offset_table_regnum/*!PIC_OFFSET_TABLE_REGNUM*/(mtcsReg);
      if (regno == (unsigned) picOffsetTableRegnum/*!PIC_OFFSET_TABLE_REGNUM*/
            &&  picOffsetTableRegnum/*!PIC_OFFSET_TABLE_REGNUM*/ != INVALID_REGNUM
            && mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[picOffsetTableRegnum/*!PIC_OFFSET_TABLE_REGNUM*/])
         return mtcs_rtl_get_pic_offset_table_rtx/*!pic_offset_table_rtx*/(self);
      if (regno == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(mtcsReg))
         return mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(self);
   }

   #if 0
   /* If the per-function register table has been set up, try to re-use
   an existing entry in that table to avoid useless generation of RTL.

   This code is disabled for now until we can fix the various backends
   which depend on having non-shared hard registers in some cases.   Long
   term we want to re-enable this code as it can significantly cut down
   on the amount of useless RTL that gets generated.

   We'll also need to fix some code that runs after reload that wants to
   set ORIGINAL_REGNO.  */

   if (cfun
   && cfun->emit
   && regno_reg_rtx
   && regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
   && mtcsReg->hardRegs.x_reg_raw_mode/*!reg_raw_mode*/[regno] == mode)
   return regno_reg_rtx[regno];
   #endif

   return mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(self,mode, regno);
}

/* Clear the size of MEM.  */
//原型 clear_mem_size emit-rtl.h emit-rtl.cc
void mtcs_rtl_clear_mem_size (MtcsRTL *self,rtx mem)
{
  mem_attrs attrs (*mtcs_rtl_get_mem_attrs/*!get_mem_attrs*/(self,mem));
  attrs.size_known_p = false;
  set_mem_attrs (self,mem, &attrs);
}


/* X is the expression to scan.  INSN is the insn it appears in.
   NOTE_FLAG is nonzero if X is from INSN's notes rather than its body.
   We should only record information for REGs with numbers
   greater than or equal to MIN_REGNO.  */
static void reg_scan_mark_refs (MtcsRTL *self,rtx x, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   enum rtx_code code;
   rtx dest;
   rtx note;

   if (!x)
      return;
   code = GET_CODE (x);
   switch (code){
      case CONST:
      CASE_CONST_ANY:
      case PC:
      case SYMBOL_REF:
      case LABEL_REF:
      case ADDR_VEC:
      case ADDR_DIFF_VEC:
      case REG:
         return;

      case EXPR_LIST:
         if (XEXP (x, 0))
            reg_scan_mark_refs(self,XEXP (x, 0), insn);
         if (XEXP (x, 1))
            reg_scan_mark_refs(self,XEXP (x, 1), insn);
         break;

      case INSN_LIST:
      case INT_LIST:
         if (XEXP (x, 1))
            reg_scan_mark_refs(self,XEXP (x, 1), insn);
         break;

      case CLOBBER:
         if (MEM_P (XEXP (x, 0)))
            reg_scan_mark_refs(self,XEXP (XEXP (x, 0), 0), insn);
         break;

      case SET:
         /* Count a set of the destination if it is a register.  */
         for (dest = SET_DEST (x); GET_CODE (dest) == SUBREG
         || GET_CODE (dest) == STRICT_LOW_PART || GET_CODE (dest) == ZERO_EXTRACT; dest = XEXP (dest, 0))
            ;

         /* If this is setting a pseudo from another pseudo or the sum of a
         pseudo and a constant integer and the other pseudo is known to be
         a pointer, set the destination to be a pointer as well.

         Likewise if it is setting the destination from an address or from a
         value equivalent to an address or to the sum of an address and
         something else.

         But don't do any of this if the pseudo corresponds to a user
         variable since it should have already been set as a pointer based
         on the type.  */

         if (REG_P (SET_DEST (x))  && REGNO (SET_DEST (x)) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
         /* If the destination pseudo is set more than once, then other
         sets might not be to a pointer value (consider access to a
         union in two threads of control in the presence of global
         optimizations).  So only set REG_POINTER on the destination
         pseudo if this is the only set of that pseudo.  */
         && DF_REG_DEF_COUNT (REGNO (SET_DEST (x))) == 1
         && ! REG_USERVAR_P (SET_DEST (x))
         && ! REG_POINTER (SET_DEST (x))
         && ((REG_P (SET_SRC (x))
         && REG_POINTER (SET_SRC (x)))
         || ((GET_CODE (SET_SRC (x)) == PLUS
         || GET_CODE (SET_SRC (x)) == LO_SUM)
         && CONST_INT_P (XEXP (SET_SRC (x), 1))
         && REG_P (XEXP (SET_SRC (x), 0))
         && REG_POINTER (XEXP (SET_SRC (x), 0)))
         || GET_CODE (SET_SRC (x)) == CONST
         || GET_CODE (SET_SRC (x)) == SYMBOL_REF
         || GET_CODE (SET_SRC (x)) == LABEL_REF
         || (GET_CODE (SET_SRC (x)) == HIGH
         && (GET_CODE (XEXP (SET_SRC (x), 0)) == CONST
         || GET_CODE (XEXP (SET_SRC (x), 0)) == SYMBOL_REF
         || GET_CODE (XEXP (SET_SRC (x), 0)) == LABEL_REF))
         || ((GET_CODE (SET_SRC (x)) == PLUS
         || GET_CODE (SET_SRC (x)) == LO_SUM)
         && (GET_CODE (XEXP (SET_SRC (x), 1)) == CONST
         || GET_CODE (XEXP (SET_SRC (x), 1)) == SYMBOL_REF
         || GET_CODE (XEXP (SET_SRC (x), 1)) == LABEL_REF))
         || ((note = find_reg_note (insn, REG_EQUAL, 0)) != 0
         && (GET_CODE (XEXP (note, 0)) == CONST
         || GET_CODE (XEXP (note, 0)) == SYMBOL_REF
         || GET_CODE (XEXP (note, 0)) == LABEL_REF))))
            REG_POINTER (SET_DEST (x)) = 1;

         /* If this is setting a register from a register or from a simple
         conversion of a register, propagate REG_EXPR.  */
         if (REG_P (dest) && !REG_ATTRS (dest))
            mtcs_rtl_set_reg_attrs_from_value/*!set_reg_attrs_from_value*/(self,dest, SET_SRC (x));

      /* fall through */

      default:
      {
         const char *fmt = GET_RTX_FORMAT (code);
         int i;
         for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
            if (fmt[i] == 'e')
               reg_scan_mark_refs(self,XEXP (x, i), insn);
            else if (fmt[i] == 'E' && XVEC (x, i) != 0){
               int j;
               for (j = XVECLEN (x, i) - 1; j >= 0; j--)
                  reg_scan_mark_refs(self,XVECEXP (x, i, j), insn);
            }
         }
      }
   }
}

//原型 reg_scan rtl.h reginfo.cc
void mtcs_rtl_reg_scan (MtcsRTL *self,rtx_insn *f, unsigned int nregs ATTRIBUTE_UNUSED)
{
   rtx_insn *insn;
   for (insn = f; insn; insn = NEXT_INSN (insn))
      if (INSN_P (insn)){
         reg_scan_mark_refs(self,PATTERN (insn), insn);
         if (REG_NOTES (insn))
            reg_scan_mark_refs(self,REG_NOTES (insn), insn);
      }
}

//原型 #define CONSTANT_ADDRESS_P(X)   (CONSTANT_P (X) && GET_CODE (X) != CONST_DOUBLE) defaults.h host nvptx实现不一样
bool mtcs_rtl_constant_address_p(MtcsRTL *self,rtx x)
{
   return self->constant_address_p(self,x);
}


/* Place a note of KIND on insn INSN with DATUM as the datum. If a
   note of this type already exists, remove it first.  */
//原型 set_unique_reg_note rtl.h emit-rtl.cc
rtx mtcs_rtl_set_unique_reg_note (MtcsRTL *self,rtx insn, enum reg_note kind, rtx datum)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   rtx note = find_reg_note (insn, kind, NULL_RTX);
   switch (kind){
      case REG_EQUAL:
      case REG_EQUIV:
         /* We need to support the REG_EQUAL on USE trick of find_reloads.  */
         if (!set_for_reg_notes (insn) && GET_CODE (PATTERN (insn)) != USE)
            return NULL_RTX;

         /* Don't add ASM_OPERAND REG_EQUAL/REG_EQUIV notes.
         It serves no useful purpose and breaks eliminate_regs.  */
         if (GET_CODE (datum) == ASM_OPERANDS)
            return NULL_RTX;

         /* Notes with side effects are dangerous.  Even if the side-effect
         initially mirrors one in PATTERN (INSN), later optimizations
         might alter the way that the final register value is calculated
         and so move or alter the side-effect in some way.  The note would
         then no longer be a valid substitution for SET_SRC.  */
         if (side_effects_p (datum))
            return NULL_RTX;
         break;

      default:
         break;
   }

   if (note)
      XEXP (note, 0) = datum;
   else{
      add_reg_note (insn, kind, datum);
      note = REG_NOTES (insn);
   }

   switch (kind){
      case REG_EQUAL:
      case REG_EQUIV:
         mtcs_dfscan_df_notes_rescan/*!df_notes_rescan*/(mtcsDfscan,as_a <rtx_insn *> (insn));
         break;
      default:
         break;
   }

   return note;
}

/* Like set_unique_reg_note, but don't do anything unless INSN sets DST.  */
//原型 set_dst_reg_note rtl.h emit-rtl.cc
rtx mtcs_rtl_set_dst_reg_note (MtcsRTL *self,rtx insn, enum reg_note kind, rtx datum, rtx dst)
{
   n_debug("mtcsrtl.c mtcs_rtl_set_dst_reg_note 00 insn:%p datum:%p dst:%p\n",insn,datum,dst);
   rtx set = set_for_reg_notes (insn);
   if (set && SET_DEST (set) == dst)
      return mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(self,insn, kind, datum);
   return NULL_RTX;
}

/* A fake decl that is used as the MEM_EXPR of spill slots.  */
static GTY(()) tree spill_slot_decl;
//原型 get_spill_slot_decl emit-rtl.h emit-rtl.cc
tree mtcs_rtl_get_spill_slot_decl (MtcsRTL *self,bool force_build_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   tree d = spill_slot_decl;
   rtx rd;

   if (d || !force_build_p)
      return d;

   d = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,DECL_SOURCE_LOCATION (current_function_decl),
         VAR_DECL, get_identifier ("%sfp"), void_type_node);
   DECL_ARTIFICIAL (d) = 1;
   DECL_IGNORED_P (d) = 1;
   TREE_USED (d) = 1;
   spill_slot_decl = d;

   rd = gen_rtx_MEM (mtcsMode->modes.M_BLKmode, mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(self));
   MEM_NOTRAP_P (rd) = 1;
   mem_attrs attrs (*self->x_mode_mem_attrs/*!mode_mem_attrs*/[(int) mtcsMode->modes.M_BLKmode]);
   attrs.alias = mtcs_alias_new_alias_set/*!new_alias_set*/(mtcsAlias);
   attrs.expr = d;
   set_mem_attrs(self,rd, &attrs);
   mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(self,d, rd);

   return d;
}

/* Replace insn with an deleted instruction note.  */
//原型 set_insn_deleted rtl.h emit-rtl.cc
void mtcs_rtl_set_insn_deleted (MtcsRTL *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   if (INSN_P (insn))
      mtcs_dfscan_df_insn_delete/*!df_insn_delete*/(mtcsDfscan,insn);
   PUT_CODE (insn, NOTE);
   NOTE_KIND (insn) = NOTE_INSN_DELETED;
}

/* Go through all the RTL insn bodies and copy any invalid shared structure.
   Assumes the mark bits are cleared at entry.  */
//原型 unshare_all_rtl_in_chain rtl.h emit-rtl.cc
void mtcs_rtl_unshare_all_rtl_in_chain (MtcsRTL *self,rtx_insn *insn)
{
   for (; insn; insn = NEXT_INSN (insn))
      if (INSN_P (insn)){
         PATTERN (insn) = mtcs_rtl_copy_rtx_if_shared/*!copy_rtx_if_shared*/(self,PATTERN (insn));
         REG_NOTES (insn) = mtcs_rtl_copy_rtx_if_shared/*!copy_rtx_if_shared*/(self,REG_NOTES (insn));
         if (CALL_P (insn))
            CALL_INSN_FUNCTION_USAGE (insn) = mtcs_rtl_copy_rtx_if_shared/*!copy_rtx_if_shared*/(self,
                  CALL_INSN_FUNCTION_USAGE (insn));
      }
}

/* Mark *ORIG1 as in use, and set it to a copy of it if it was already in
   use.  Recursively does the same for subexpressions.  */
//原型 copy_rtx_if_shared_1 emit-rtl.cc
static void copy_rtx_if_shared_1 (MtcsRTL *self,rtx *orig1)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx x;
   int i;
   enum rtx_code code;
   rtx *last_ptr;
   const char *format_ptr;
   int copied = 0;
   int length;

   /* Repeat is used to turn tail-recursion into iteration.  */
repeat:
   x = *orig1;

   if (x == 0)
      return;

   code = GET_CODE (x);

   /* These types may be freely shared.  */

   switch (code){
      case REG:
      case DEBUG_EXPR:
      case VALUE:
      CASE_CONST_ANY:
      case SYMBOL_REF:
      case LABEL_REF:
      case CODE_LABEL:
      case PC:
      case RETURN:
      case SIMPLE_RETURN:
      case SCRATCH:
         /* SCRATCH must be shared because they represent distinct values.  */
         return;
      case CLOBBER:
         /* Share clobbers of hard registers, but do not share pseudo reg
         clobbers or clobbers of hard registers that originated as pseudos.
         This is needed to allow safe register renaming.  */
         if (REG_P (XEXP (x, 0))
         && mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,REGNO (XEXP (x, 0)))
         && mtcs_reg_is_hard/*!HARD_REGISTER_NUM_P*/(mtcsReg,ORIGINAL_REGNO (XEXP (x, 0))))
            return;
         break;

      case CONST:
         if (shared_const_p (x))
            return;
         break;

      case DEBUG_INSN:
      case INSN:
      case JUMP_INSN:
      case CALL_INSN:
      case NOTE:
      case BARRIER:
         /* The chain of insns is not being copied.  */
         return;

      default:
         break;
   }

   /* This rtx may not be shared.  If it has already been seen,
   replace it with a copy of itself.  */

   if (RTX_FLAG (x, used)){
      x = shallow_copy_rtx (x);
      copied = 1;
   }
   RTX_FLAG (x, used) = 1;

   /* Now scan the subexpressions recursively.
   We can store any replaced subexpressions directly into X
   since we know X is not shared!  Any vectors in X
   must be copied if X was copied.  */

   format_ptr = GET_RTX_FORMAT (code);
   length = GET_RTX_LENGTH (code);
   last_ptr = NULL;

   for (i = 0; i < length; i++){
      switch (*format_ptr++){
         case 'e':
            if (last_ptr)
               copy_rtx_if_shared_1(self,last_ptr);
            last_ptr = &XEXP (x, i);
            break;

         case 'E':
            if (XVEC (x, i) != NULL){
               int j;
               int len = XVECLEN (x, i);

               /* Copy the vector iff I copied the rtx and the length
               is nonzero.  */
               if (copied && len > 0)
                  XVEC (x, i) = gen_rtvec_v (len, XVEC (x, i)->elem);

               /* Call recursively on all inside the vector.  */
               for (j = 0; j < len; j++){
                  if (last_ptr)
                     copy_rtx_if_shared_1(self,last_ptr);
                  last_ptr = &XVECEXP (x, i, j);
               }
            }
            break;
      }
   }
   *orig1 = x;
   if (last_ptr){
      orig1 = last_ptr;
      goto repeat;
   }
}

/* Mark ORIG as in use, and return a copy of it if it was already in use.
   Recursively does the same for subexpressions.  Uses
   copy_rtx_if_shared_1 to reduce stack space.  */
//原型 copy_rtx_if_shared rtl.h emit-rtl.cc
rtx mtcs_rtl_copy_rtx_if_shared (MtcsRTL *self,rtx orig)
{
  copy_rtx_if_shared_1 (self,&orig);
  return orig;
}

/* This function is deprecated, please use sequences instead.

   Move a consecutive bunch of insns to a different place in the chain.
   The insns to be moved are those between FROM and TO.
   They are moved to a new position after the insn AFTER.
   AFTER must not be FROM or TO or any insn in between.

   This function does not know about SEQUENCEs and hence should not be
   called after delay-slot filling has been done.  */
//原型 reorder_insns_nobb rtl.h emit-rtl.cc
void mtcs_rtl_reorder_insns_nobb (MtcsRTL *self,rtx_insn *from, rtx_insn *to, rtx_insn *after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   if (mtcsOptionsItem->x_flag_checking){
      for (rtx_insn *x = from; x != to; x = NEXT_INSN (x))
         gcc_assert (after != x);
      gcc_assert (after != to);
   }

   /* Splice this bunch out of where it is now.  */
   if (PREV_INSN (from))
      SET_NEXT_INSN (PREV_INSN (from)) = NEXT_INSN (to);
   if (NEXT_INSN (to))
      SET_PREV_INSN (NEXT_INSN (to)) = PREV_INSN (from);
   if (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData) == to)
      mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,PREV_INSN (from));
   if (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData) == from)
      mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,NEXT_INSN (to));

   /* Make the new neighbors point to it and it to them.  */
   if (NEXT_INSN (after))
      SET_PREV_INSN (NEXT_INSN (after)) = to;

   SET_NEXT_INSN (to) = NEXT_INSN (after);
   SET_PREV_INSN (from) = after;
   SET_NEXT_INSN (after) = from;
   if (after == mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData))
      mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,to);
}

/* Same as function above, but take care to update BB boundaries.  */
//原型 reorder_insns rtl.h emit-rtl.cc
void mtcs_rtl_reorder_insns (MtcsRTL *self,rtx_insn *from, rtx_insn *to, rtx_insn *after)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   rtx_insn *prev = PREV_INSN (from);
   basic_block bb, bb2;
    mtcs_rtl_reorder_insns_nobb/*!reorder_insns_nobb*/(self,from, to, after);
   if (!BARRIER_P (after) && (bb = BLOCK_FOR_INSN (after))){
      rtx_insn *x;
      mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb);

      if (!BARRIER_P (from)  && (bb2 = BLOCK_FOR_INSN (from))){
         if (BB_END (bb2) == to)
            BB_END (bb2) = prev;
         mtcs_dfcore_df_set_bb_dirty/*!df_set_bb_dirty*/(mtcsDfcore,bb2);
      }

      if (BB_END (bb) == after)
         BB_END (bb) = to;

      for (x = from; x != NEXT_INSN (to); x = NEXT_INSN (x))
         if (!BARRIER_P (x))
            mtcs_dfscan_df_insn_change_bb/*!df_insn_change_bb*/(mtcsDfscan,x, bb);
   }
}

/* Adjust REG in-place so that it has mode MODE.  It is assumed that the
   new register is a (possibly paradoxical) lowpart of the old one.  */
//原型 adjust_reg_mode emit-rtl.h emit-rtl.cc
void mtcs_rtl_adjust_reg_mode (MtcsRTL *self,rtx reg, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   update_reg_offset(self,reg, reg,
         mtcs_mode_byte_lowpart_offset/*!byte_lowpart_offset*/(mtcsMode,mode, GET_MODE (reg)));
   mtcs_rtl_put_mode/*!PUT_MODE*/(self,reg, mode);
}

//原型 #define INCOMING_RETURN_ADDR_RTX    gen_rtx_MEM (Pmode, stack_pointer_rtx) host=1 nvptx=0
rtx mtcs_rtl_incoming_return_addr_rtx (MtcsRTL *self)
{
   if(self->incoming_return_addr_rtx)
      return self->incoming_return_addr_rtx(self);
   return NULL;
}

//原型 regstack_completed rtl.h reg-stack.cc
int mtcs_rtl_get_regstack_completed(MtcsRTL *self)
{
   return self->regstack_completed;
}

void mtcs_rtl_set_regstack_completed (MtcsRTL *self,int value)
{
   self->regstack_completed = value;
}

//原型 #define SELECT_CC_MODE(OP, X, Y) ix86_cc_mode ((OP), (X), (Y))
machine_mode mtcs_rtl_select_cc_mode (MtcsRTL *self,enum rtx_code code, rtx op0, rtx op1)
{
   if(self->select_cc_mode)
      return self->select_cc_mode(self,code,op0,op1);
   return VOIDmode;
}

void mtcs_rtl_init(MtcsRTL *self)
{
   mtcsRTLInit(self);
}

/* Generate a REG rtx for a new pseudo register, copying the mode
   and attributes from X.  */
//原型 gen_reg_rtx_and_attrs rtl.h emit-rtl.cc
rtx mtcs_rtl_gen_reg_rtx_and_attrs (MtcsRTL *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   rtx reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (x));
   mtcs_rtl_set_reg_attrs_from_value/*!set_reg_attrs_from_value*/(self,reg, x);
   return reg;
}


//原型 namespace wi ... struct int_traits <rtx_mode_t> rtl.h 因为调用了GET_MODE_PRECISION 所以需要重写
unsigned int wi::int_traits <mtcs_rtx_mode_t>::get_precision (const mtcs_rtx_mode_t &x)
{
    MtcsTarget *mtcsTarget = mtcs_compile_get_current_target(mtcs_compile_get());
    MtcsMode *mtcsMode = mtcsTarget->mtcsMode;
    return mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mtcs_mode_as_a <scalar_mode> (mtcsMode,x.second));
}


wi::storage_ref wi::int_traits <mtcs_rtx_mode_t>::decompose (HOST_WIDE_INT *, unsigned int precision,  const mtcs_rtx_mode_t &x)
{
   MtcsTarget *mtcsTarget = mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsMode *mtcsMode = mtcsTarget->mtcsMode;
   MtcsConfig *mtcsConfig = mtcs_target_get_config(mtcsTarget);

   gcc_checking_assert (precision == get_precision (x));
   switch (GET_CODE (x.first)){
      case CONST_INT:
         if (precision < HOST_BITS_PER_WIDE_INT)
            /* Nonzero BImodes are stored as STORE_FLAG_VALUE, which on many
            targets is 1 rather than -1.  */
            gcc_checking_assert (INTVAL (x.first)  == sext_hwi (INTVAL (x.first), precision)
                  || (x.second == mtcsMode->modes.M_BImode && INTVAL (x.first) == 1));

         return wi::storage_ref (&INTVAL (x.first), 1, precision);

      case CONST_WIDE_INT:
         return wi::storage_ref (&CONST_WIDE_INT_ELT (x.first, 0), CONST_WIDE_INT_NUNITS (x.first), precision);

      //#if TARGET_SUPPORTS_WIDE_INT == 0
      case CONST_DOUBLE:
         if(mtcs_config_get_value(mtcsConfig,MTCS_TARGET_SUPPORTS_WIDE_INT)==0)
            return wi::storage_ref (&CONST_DOUBLE_LOW (x.first), 2, precision);
         else
            gcc_unreachable ();
      //#endif

      default:
         gcc_unreachable ();
   }
}





