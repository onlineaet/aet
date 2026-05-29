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
#ifndef __GCC_MTCS_MODE__
#define __GCC_MTCS_MODE__

#include "../nlib.h"
#include "mtcsinterface.h"
#include "explow.h"

//介绍machine_mode
//https://blog.csdn.net/wuhui_gdnt/article/details/5319053
//具体见下面两个文件
//build/平台/insn-modes.h
//build/平台/insn-modes.cc

typedef nuint   mtcs_mode;

typedef struct _MtcsMode MtcsMode;
struct _MtcsMode
{
    MtcsBackupRestore mtcsBackupRestore;
    NHashTable *modeNameHashTable;
    mtcs_mode      (*get_Pmode)(MtcsMode *self);//返回Pmode #define MTCS_Pmode (MTCS_ABI64 ? DImode : SImode) mtcsptxmode.c
    bool           (*standard_type_bitsize)(MtcsMode *self,int bitsize);
    void           (*init_int)(MtcsMode *self);
    const char    *(*get_type)(MtcsMode *self,mtcs_mode mode, nboolean promote);
    mtcs_mode      (*promote_inner)(MtcsMode *self,mtcs_mode mode,int unsignedp,const_tree type);
    unsigned HOST_WIDE_INT (*get_mask)(MtcsMode *self,mtcs_mode mode);
    struct real_format *(*get_real_format)(MtcsMode *self,mtcs_mode mode);//原型 REAL_MODE_FORMAT real.h
    int            (*clz_defined_value_at_zero)(MtcsMode *self,mtcs_mode mode,int *value);//原型#define CLZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
    int            (*ctz_defined_value_at_zero)(MtcsMode *self,mtcs_mode mode,int *value);//原型#define CTZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
    nushort        (*get_reversible_cc_mode)(MtcsMode *self,mtcs_mode mode);//原型 REVERSIBLE_CC_MODE (mode)
    //原型 STACK_SAVEAREA_MODE default.h
    mtcs_mode      (*get_stack_savearea_mode)(MtcsMode *self,enum save_level level);
    ////原型 REGMODE_NATURAL_SIZE regs.h
    int             (*get_regmode_natural_size)(MtcsMode *self,mtcs_mode mode);
    //原型 init_adjust_machine_modes machmode.h insn-modes.cc
    void (*init_adjust_machine_modes)(MtcsMode *self);
    //原型 LOAD_EXTEND_OP(M) defaults.h
    rtx_code (*load_extend_op)(MtcsMode *self,mtcs_mode m);
    //原型 targetm.unwind_word_mode() #define TARGET_UNWIND_WORD_MODE default_unwind_word_mode
    scalar_int_mode (*unwind_word_mode)(MtcsMode *self);
    //原型 targetm.eh_return_filter_mode () #define TARGET_EH_RETURN_FILTER_MODE default_eh_return_filter_mode
    scalar_int_mode (*eh_return_filter_mode)(MtcsMode *self);
    //原型 #define IS_STACK_MODE(MODE)   (X87_FLOAT_MODE_P (MODE) && (!(SSE_FLOAT_MODE_P (MODE) && TARGET_SSE_MATH) || TARGET_MIX_SSE_I387))
    //nvptx 未定义 IS_STACK_MODE
    nboolean (* is_stack_mode)(MtcsMode *self,machine_mode m);
    //原型 #define CASE_VECTOR_SHORTEN_MODE(MIN, MAX, BODY) 平台定义 nvptx无，无缺省
    scalar_int_mode (*case_vector_shorten_mode)(MtcsMode *self,int min,int max,rtx body);
    //原型 targetm.c.mode_for_floating_type (TI_DOUBLE_TYPE); #define TARGET_C_MODE_FOR_FLOATING_TYPE default_mode_for_floating_type
    machine_mode (*mode_for_floating_type)(MtcsMode *self,enum tree_index ti);
    //原型 push_rounding 平台定义
    poly_int64 (*push_rounding)(MtcsMode *self,poly_int64 bytes);

    nuint number;//machine_mode数量
    int maxNumber; //最大machine_mode 一般与number相同
    bool *int_n_enabled_p;
    int  mtcs_NUM_INT_N_ENTS;
    int_n_data_t *intData;

    int max_bitsize_mode_any_int;//原型 MAX_BITSIZE_MODE_ANY_INT insn-modes.h

    struct{
        nuint M_VOIDmode;              /* machmode.def:194 */
        nuint M_BLKmode;               /* machmode.def:198 */
        nuint M_CCmode;                /* machmode.def:236 */
        nuint M_BImode;                /* machmode.def:201 */
        nuint M_QImode;                /* machmode.def:209 */
        nuint M_HImode;                /* machmode.def:210 */
        nuint M_SImode;                /* machmode.def:211 */
        nuint M_DImode;                /* machmode.def:212 */
        nuint M_TImode;                /* machmode.def:213 */
        nuint M_QQmode;                /* machmode.def:239 */
        nuint M_HQmode;                /* machmode.def:240 */
        nuint M_SQmode;                /* machmode.def:241 */
        nuint M_DQmode;                /* machmode.def:242 */
        nuint M_TQmode;                /* machmode.def:243 */
        nuint M_UQQmode;               /* machmode.def:245 */
        nuint M_UHQmode;               /* machmode.def:246 */
        nuint M_USQmode;               /* machmode.def:247 */
        nuint M_UDQmode;               /* machmode.def:248 */
        nuint M_UTQmode;               /* machmode.def:249 */
        nuint M_HAmode;                /* machmode.def:251 */
        nuint M_SAmode;                /* machmode.def:252 */
        nuint M_DAmode;                /* machmode.def:253 */
        nuint M_TAmode;                /* machmode.def:254 */
        nuint M_UHAmode;               /* machmode.def:256 */
        nuint M_USAmode;               /* machmode.def:257 */
        nuint M_UDAmode;               /* machmode.def:258 */
        nuint M_UTAmode;               /* machmode.def:259 */
        nuint M_HFmode;                /* config/nvptx/nvptx-modes.def:1 */
        nuint M_SFmode;                /* machmode.def:231 */
        nuint M_DFmode;                /* machmode.def:232 */
        nuint M_SDmode;                /* machmode.def:272 */
        nuint M_DDmode;                /* machmode.def:273 */
        nuint M_TDmode;                /* machmode.def:274 */
        nuint M_CQImode;               /* machmode.def:267 */
        nuint M_CHImode;               /* machmode.def:267 */
        nuint M_CSImode;               /* machmode.def:267 */
        nuint M_CDImode;               /* machmode.def:267 */
        nuint M_CTImode;               /* machmode.def:267 */
        nuint M_HCmode;                /* machmode.def:269 */
        nuint M_SCmode;                /* machmode.def:269 */
        nuint M_DCmode;                /* machmode.def:269 */
        nuint M_V2SImode;              /* config/nvptx/nvptx-modes.def:3 */
        nuint M_V2DImode;              /* config/nvptx/nvptx-modes.def:5 */
    }modes;

    //原型 machmode.h
    scalar_int_mode byte_mode;
    scalar_int_mode word_mode;
    scalar_int_mode ptr_mode;

    char       **modeName;
    poly_uint16 *modeSize;
    nuchar      *modeClass;
    nushort     *modeBaseAlign;
    poly_uint16 *modePrecision;
    nushort     *modeInner;
    poly_uint16 *modeNunits;
    nuchar      *modeUnitSize;
    nushort     *modeUnitPrecision;
    nushort     *classNarrowestMode;
    nushort     *modeWider;
    nushort     *modeNext;
    nushort     *mode2xwider;
    nushort     *modeComplex;
    nuchar      *modeFBit;//原型 mode_fbit insn-modes.cc
    nuchar      *modeIBit;//原型 mode_ibit insn-modes.cc

    struct{
        nuint min_RANDOM;
        nuint max_RANDOM;
        nuint min_CC;
        nuint max_CC;
        nuint min_BOOL;
        nuint max_BOOL;
        nuint min_INT;
        nuint max_INT;
        nuint min_PARTIAL_INT;
        nuint max_PARTIAL_INT;
        nuint min_FRACT;
        nuint max_FRACT;
        nuint min_UFRACT;
        nuint max_UFRACT;
        nuint min_ACCUM;
        nuint max_ACCUM;
        nuint min_UACCUM;
        nuint max_UACCUM;
        nuint min_FLOAT;
        nuint max_FLOAT;
        nuint min_DECIMAL_FLOAT;
        nuint max_DECIMAL_FLOAT;
        nuint min_COMPLEX_INT;
        nuint max_COMPLEX_INT;
        nuint min_COMPLEX_FLOAT;
        nuint max_COMPLEX_FLOAT;
        nuint min_VECTOR_BOOL;
        nuint max_VECTOR_BOOL;
        nuint min_VECTOR_INT;
        nuint max_VECTOR_INT;
        nuint min_VECTOR_FRACT;
        nuint max_VECTOR_FRACT;
        nuint min_VECTOR_UFRACT;
        nuint max_VECTOR_UFRACT;
        nuint min_VECTOR_ACCUM;
        nuint max_VECTOR_ACCUM;
        nuint min_VECTOR_UACCUM;
        nuint max_VECTOR_UACCUM;
        nuint min_VECTOR_FLOAT;
        nuint max_VECTOR_FLOAT;
        nuint min_OPAQUE;
        nuint max_OPAQUE;
    }modesMinMax;

    struct{
        nuint num_RANDOM;
        nuint num_CC;
        nuint num_INT;
        nuint num_PARTIAL_INT;
        nuint num_FRACT;
        nuint num_UFRACT;
        nuint num_ACCUM;
        nuint num_UACCUM;
        nuint num_FLOAT;
        nuint num_DECIMAL_FLOAT;
        nuint num_COMPLEX_INT;
        nuint num_COMPLEX_FLOAT;
        nuint num_VECTOR_BOOL;
        nuint num_VECTOR_INT;
        nuint num_VECTOR_FRACT;
        nuint num_VECTOR_UFRACT;
        nuint num_VECTOR_ACCUM;
        nuint num_VECTOR_UACCUM;
        nuint num_VECTOR_FLOAT;
        nuint num_OPAQUE;
    }modesNum;

    mtcs_mode functionMode;//原型 #define FUNCTION_MODE QImode nvptx
    mtcs_mode stackSizeMode;//原型 #define STACK_SIZE_MODE Pmode nvptx.h
    npointer target;//MtcsTarget

    //原型 machmode.h 备分主机的三个mode
    void *backup;
};


/* A class for iterating through possible bitfield modes.  */
//原型 bit_field_mode_iterator machmode.h
class mtcs_bit_field_mode_iterator
{
public:
    mtcs_bit_field_mode_iterator (MtcsMode *mode,HOST_WIDE_INT, HOST_WIDE_INT,
               poly_int64, poly_int64,
               unsigned int, bool);
  bool next_mode (scalar_int_mode *);
  bool prefer_smaller_modes ();

private:
  MtcsMode *mtcsMode;
  opt_scalar_int_mode m_mode;
  /* We use signed values here because the bit position can be negative
     for invalid input such as gcc.dg/pr48335-8.c.  */
  HOST_WIDE_INT m_bitsize;
  HOST_WIDE_INT m_bitpos;
  poly_int64 m_bitregion_start;
  poly_int64 m_bitregion_end;
  unsigned int m_align;
  bool m_volatilep;
  int m_count;
};

void           mtcs_mode_init(MtcsMode *self);
unsigned short mtcs_mode_get_bitsize(MtcsMode *self,mtcs_mode mode);//GET_MODE_BITSIZE
poly_uint16    mtcs_mode_get_bitsize_poly(MtcsMode *self,mtcs_mode mode);//GET_MODE_BITSIZE

unsigned int   mtcs_mode_get_alignment(MtcsMode *self,mtcs_mode mode);//GET_MODE_ALIGNMENT
unsigned short mtcs_mode_get_precision(MtcsMode *self,mtcs_mode mode);
poly_uint16    mtcs_mode_get_precision_poly(MtcsMode *self,mtcs_mode mode);

unsigned short mtcs_mode_get_size(MtcsMode *self,mtcs_mode mode);
poly_uint16    mtcs_mode_get_size_poly(MtcsMode *self,mtcs_mode mode);
//原型 GET_MODE_INNER
mtcs_mode      mtcs_mode_get_inner(MtcsMode *self,mtcs_mode mode);
unsigned char  mtcs_mode_get_class(MtcsMode *self,mtcs_mode mode);//原型 GET_MODE_CLASS
nuint          mtcs_mode_get_modes_by_class(MtcsMode *self,enum mode_class modeClass,nuint *modes,int len);//返回指定类型的mode
const char    *mtcs_mode_get_type(MtcsMode *self,mtcs_mode mode, nboolean promote);
const char    *mtcs_mode_get_name(MtcsMode *self,mtcs_mode mode);
//原型 NUM_MACHINE_MODES insn-modes.h
nuint          mtcs_mode_get_number(MtcsMode *self);
void           mtcs_mode_set_number(MtcsMode *self,nuint number);
//原型 MAX_MACHINE_MODE insn-modes.h
int            mtcs_mode_get_max_number(MtcsMode *self);
void           mtcs_mode_set_max_number(MtcsMode *self,int maxNumber);
void           mtcs_mode_set_target(MtcsMode *self,npointer target);
//void           mtcs_mode_init_int(MtcsMode *self,int_n_data_t *intData,int mtcs_NUM_INT_N_ENTS);
void           mtcs_mode_init_int(MtcsMode *self);

mtcs_mode      mtcs_mode_get_Pmode(MtcsMode *self);
mtcs_mode      mtcs_mode_host2device(MtcsMode *self,machine_mode hostMode);//主机到设备
/**
 * machine_mode从主机到设备
 */
mtcs_mode       mtcs_mode_host2device_by_tree(MtcsMode *self,tree declorType,machine_mode mode);
//原型 #define SCALAR_TYPE_MODE(NODE)  (as_a <scalar_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_mode     mtcs_mode_host2device_scalar(MtcsMode *self,tree value);
scalar_mode     mtcs_mode_scalar_type_mode(MtcsMode *self,tree value);

//原型 #define SCALAR_INT_TYPE_MODE(NODE) (as_a <scalar_int_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_int_mode mtcs_mode_host2device_scalar_int(MtcsMode *self,tree value);
scalar_int_mode mtcs_mode_scalar_int_type_mode(MtcsMode *self,tree value);

//原型 #define SCALAR_FLOAT_TYPE_MODE(NODE)  (as_a <scalar_float_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_float_mode mtcs_mode_host2device_scalar_float(MtcsMode *self,tree value);
//原型 #define SCALAR_FLOAT_TYPE_MODE(NODE)  (as_a <scalar_float_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_float_mode mtcs_mode_scalar_float_type_mode(MtcsMode *self,tree value);

nboolean       mtcs_mode_is_complex_p(MtcsMode *self,mtcs_mode mode);
//原型 MINIMUM_ALIGNMENT defaults.h
unsigned int   mtcs_mode_get_mininum_alignment(MtcsMode *self,tree exp,mtcs_mode mode,unsigned int align);
machine_mode   mtcs_mode_device2host(MtcsMode *self,mtcs_mode deviceMode);
nboolean       mtcs_mode_is_int(MtcsMode *self,mtcs_mode mode);
//原型 #define INTEGRAL_MODE_P(MODE)   machmode.h
nboolean       mtcs_mode_is_integral_p(MtcsMode *self,mtcs_mode mode);
//原型 trunc_int_for_mode rtl.h explow.cc
HOST_WIDE_INT  mtcs_mode_trunc_int_for_mode (MtcsMode *self,HOST_WIDE_INT c, mtcs_mode mode);
poly_int64     mtcs_mode_trunc_int_for_mode_with_poly_int64 (MtcsMode *self,poly_int64 x, mtcs_mode mode);
poly_int64     mtcs_mode_trunc_int_for_mode (MtcsMode *self,poly_int64 x, mtcs_mode mode);
//原型 promote_ssa_mode explow.h explow.cc
mtcs_mode      mtcs_mode_promote_ssa_mode (MtcsMode *self,const_tree name, int *punsignedp);
//原型 promote_decl_mode explow.h explow.cc
mtcs_mode      mtcs_mode_promote_decl_mode (MtcsMode *self,const_tree decl, int *punsignedp);
//原型 promote_mode explow.h
mtcs_mode      mtcs_mode_promote_mode (MtcsMode *self,const_tree type ATTRIBUTE_UNUSED, mtcs_mode mode,int *punsignedp ATTRIBUTE_UNUSED);
//原型 promote_function_mode explow.h explow.cc
mtcs_mode      mtcs_mode_promote_function_mode (MtcsMode *self,const_tree type,
        mtcs_mode mode, int *punsignedp,const_tree funtype, int for_return);
//原型 FLOAT_MODE_P
nboolean       mtcs_mode_is_float_p(MtcsMode *self,mtcs_mode mode);

//原型 SCALAR_FLOAT_MODE_P machmode.h
nboolean       mtcs_mode_is_scalar_float_p(MtcsMode *self,mtcs_mode mode);
//原型 SCALAR_INT_MODE_P machmode.h
nboolean       mtcs_mode_is_scalar_int_p(MtcsMode *self,mtcs_mode mode);
/* Nonzero if MODE is a complex mode.  */
//原型 #define COMPLEX_MODE_P(MODE)            \
//  (GET_MODE_CLASS (MODE) == MODE_COMPLEX_INT    \
//   || GET_MODE_CLASS (MODE) == MODE_COMPLEX_FLOAT)
nboolean       mtcs_mode_is_complex_p(MtcsMode *self,mtcs_mode mode);


//原型#define MAX_FIXED_MODE_SIZE GET_MODE_BITSIZE (DImode)
nuint          mtcs_mode_get_max_fixed_size(MtcsMode *self);
//原型 mode_for_size stor-layout.cc
opt_machine_mode mtcs_mode_mode_for_size (MtcsMode *self,poly_uint64 size, enum mode_class mclass, int limit);
//原型 int_mode_for_mode stor-layout.cc
opt_scalar_int_mode mtcs_mode_int_mode_for_mode (MtcsMode *self,mtcs_mode mode);
//原型 machmode.h int_mode_for_size
opt_scalar_int_mode mtcs_mode_int_mode_for_size (MtcsMode *self,poly_uint64 size, int limit);
//原型 mode_fro_size machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_for_size (MtcsMode *self,poly_uint64 size, enum mode_class mclass, int limit);
//原型 #define VECTOR_MODE_P(MODE)
nboolean       mtcs_mode_is_vector_p(MtcsMode *self,mtcs_mode mode);
//原型 #define DECIMAL_FLOAT_MODE_P(MODE)  (GET_MODE_CLASS (MODE) == MODE_DECIMAL_FLOAT)
nboolean       mtcs_mode_is_decimal_float_p(MtcsMode *self,mtcs_mode mode);
//原型 MODE_COMPOSITE_P real.h
nboolean       mtcs_mode_is_composite_p(MtcsMode *self,mtcs_mode mode);
//原型 GET_MODE_NUNITS machmode.h mode_to_nunits
poly_uint16    mtcs_mode_get_nunits (MtcsMode *self,mtcs_mode  mode);
//原型 #define GET_MODE_UNIT_SIZE(MODE) mode_to_unit_size (MODE)
unsigned char  mtcs_mode_get_unit_size(MtcsMode *self,mtcs_mode mode);
//原型 paradoxical_subreg_p rtl.h 矛盾的
nboolean       mtcs_mode_paradoxical_subreg_p (MtcsMode *self,mtcs_mode outermode, mtcs_mode innermode);
//原型 GET_MODE_MASK machmode.h
unsigned HOST_WIDE_INT   mtcs_mode_get_mask(MtcsMode *self,mtcs_mode mode);
//原型 subreg_lowpart_offset rtl.h
poly_uint64    mtcs_mode_subreg_lowpart_offset (MtcsMode *self,machine_mode outermode, machine_mode innermode);
//原型 byte_lowpart_offset rtl.h emit-rtl.cc
poly_int64     mtcs_mode_byte_lowpart_offset (MtcsMode *self,machine_mode outer_mode, machine_mode inner_mode);
//原型 subreg_highpart_offset rtl.h
poly_uint64    mtcs_mode_subreg_highpart_offset (MtcsMode *self,machine_mode outermode, machine_mode innermode);
//原型 #define GET_MODE_UNIT_PRECISION(MODE) (mode_to_unit_precision (MODE))
nushort        mtcs_mode_get_unit_precision(MtcsMode *self,mtcs_mode mode);
//原型 REAL_MODE_FORMAT real.h
struct real_format *mtcs_mode_get_real_format(MtcsMode *self,mtcs_mode mode);
//原型 real.h MODE_HAS_INFINITIES
//#define MODE_HAS_INFINITIES(MODE) \
//  (FLOAT_MODE_P (MODE) && FLOAT_MODE_FORMAT (MODE)->has_inf)
nboolean  mtcs_mode_has_infinities(MtcsMode *self,mtcs_mode mode);
//原型 HONOR_INFINITIES (machine_mode m) real.h real.cc
nboolean mtcs_mode_honor_infinities (MtcsMode *self, mtcs_mode m);
//原型 HONOR_INFINITIES (const_tree t) real.h real.cc
nboolean mtcs_mode_honor_infinities (MtcsMode *self, const_tree t);
//原型 HONOR_INFINITIES (const_rtx rtx) real.h real.cc
nboolean mtcs_mode_honor_infinities (MtcsMode *self, const_rtx rtx);

//原型 MODE_HAS_SIGNED_ZEROS real.h
nboolean  mtcs_mode_has_signed_zeros(MtcsMode *self,mtcs_mode mode);
//原型 HONOR_SIGNED_ZEROS real.h real.cc
nboolean  mtcs_mode_honor_signed_zeros(MtcsMode *self,mtcs_mode mode);
//原型 HONOR_SIGNED_ZEROS real.h real.cc
nboolean  mtcs_mode_honor_signed_zeros(MtcsMode *self,const_tree t);
//原型 HONOR_SIGNED_ZEROS real.h real.cc
nboolean  mtcs_mode_honor_signed_zeros(MtcsMode *self,const_rtx rtx);
//原型 HONOR_SNANS real.h real.cc
nboolean  mtcs_mode_honor_snans(MtcsMode *self,mtcs_mode mode);
//原型 HONOR_SNANS real.h real.cc
nboolean  mtcs_mode_honor_snans(MtcsMode *self,const_tree t);
//原型 HONOR_SNANS real.h real.cc
nboolean  mtcs_mode_honor_snans(MtcsMode *self,const_rtx rtx);
//原型  MODE_HAS_NANS real.h
//#define MODE_HAS_NANS(MODE) \
//  (FLOAT_MODE_P (MODE) && FLOAT_MODE_FORMAT (MODE)->has_nans)
nboolean  mtcs_mode_has_nans(MtcsMode *self,mtcs_mode mode);
//原型 HONOR_NANS real.h real.cc
nboolean  mtcs_mode_honor_nans(MtcsMode *self,mtcs_mode mode);
//原型 extern bool HONOR_NANS (const_tree); real.h real.cc
nboolean  mtcs_mode_honor_nans(MtcsMode *self,const_tree t);
//原型 extern bool HONOR_NANS (const_rtx); real.h real.cc
nboolean  mtcs_mode_honor_nans(MtcsMode *self,const_rtx rtx);


//原型  MODE_HAS_SIGN_DEPENDENT_ROUNDING real.h
//#define MODE_HAS_SIGN_DEPENDENT_ROUNDING(MODE) \
//  (FLOAT_MODE_P (MODE) \
//   && FLOAT_MODE_FORMAT (MODE)->has_sign_dependent_rounding)
nboolean  mtcs_mode_has_sign_dependent_rounding(MtcsMode *self,mtcs_mode mode);
//原型 HONOR_SIGN_DEPENDENT_ROUNDING real.h
nboolean  mtcs_mode_honor_sign_dependent_rounding(MtcsMode *self,mtcs_mode mode);
nboolean  mtcs_mode_honor_sign_dependent_rounding(MtcsMode *self,const_tree t);
nboolean  mtcs_mode_honor_sign_dependent_rounding(MtcsMode *self,const_rtx x);

//原型 MAX_BITSIZE_MODE_ANY_INT insn-modes.h
int       mtcs_mode_get_max_bitsize_mode_any_int(MtcsMode *self);
void      mtcs_mode_set_max_bitsize_mode_any_int(MtcsMode *self,int max);
//原型#define CLZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
int       mtcs_mode_clz_defined_value_at_zero(MtcsMode *self,mtcs_mode mode,int *value);
//原型#define CTZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
int       mtcs_mode_ctz_defined_value_at_zero(MtcsMode *self,mtcs_mode mode,int *value);
//原型 related_vector_mode machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_related_vector_mode(MtcsMode *self,machine_mode, scalar_mode,poly_uint64 = 0);
//原型 related_int_vector_mode machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_related_int_vector_mode (MtcsMode *self,machine_mode vector_mode);
//原型 mode_for_vector machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_mode_for_vector (MtcsMode *self,scalar_mode innermode, poly_uint64 nunits);

/* Return true if MODE is a scalar integer mode that fits in a
   HOST_WIDE_INT.  */
//原型 HWI_COMPUTABLE_MODE_P machmode.h
nboolean mtcs_mode_is_hwi_computable_p(MtcsMode *self,machine_mode mode);
//原型 #define GET_MODE_UNIT_BITSIZE(MODE) \
//  ((unsigned short) (GET_MODE_UNIT_SIZE (MODE) * BITS_PER_UNIT)) machmode.h
unsigned short   mtcs_mode_get_unit_bitsize(MtcsMode *self,mtcs_mode mode);
/* Nonzero if MODE is a scalar/vector accum mode.  */
//原型
//#define ACCUM_MODE_P(MODE)      \
//  (GET_MODE_CLASS (MODE) == MODE_ACCUM  \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_ACCUM)
//machmode.h
nboolean mtcs_mode_is_accum_p(MtcsMode *self,mtcs_mode mode);

/* Nonzero if MODE is a scalar/vector fract mode.  */
//原型
//#define FRACT_MODE_P(MODE)      \
//  (GET_MODE_CLASS (MODE) == MODE_FRACT  \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_FRACT)
//machmode.h
nboolean mtcs_mode_is_fract_p(MtcsMode *self,mtcs_mode mode);

/* Nonzero if MODE is a scalar/vector fract or accum mode.  */
//原型
//#define SIGNED_FIXED_POINT_MODE_P(MODE)     \
//  (FRACT_MODE_P (MODE) || ACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_signed_fixed_point_p(MtcsMode *self,mtcs_mode mode);

/* Nonzero if MODE is a scalar/vector ufract mode.  */
//原型
//#define UFRACT_MODE_P(MODE)     \
//  (GET_MODE_CLASS (MODE) == MODE_UFRACT \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UFRACT)
//machmode.h
nboolean mtcs_mode_is_ufract_p(MtcsMode *self,mtcs_mode mode);

/* Nonzero if MODE is a scalar/vector uaccum mode.  */
//原型
//#define UACCUM_MODE_P(MODE)     \
//  (GET_MODE_CLASS (MODE) == MODE_UACCUM \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UACCUM)
//machmode.h
nboolean mtcs_mode_is_uaccum_p(MtcsMode *self,mtcs_mode mode);

/* Nonzero if MODE is a scalar/vector ufract or uaccum mode.  */
//原型
//#define UNSIGNED_FIXED_POINT_MODE_P(MODE)   \
//  (UFRACT_MODE_P (MODE) || UACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_unsigned_fixed_point_p(MtcsMode *self,mtcs_mode mode);

//原型 #define SCALAR_FRACT_MODE_P(MODE)   (GET_MODE_CLASS (MODE) == MODE_FRACT) machmode.h
nboolean mtcs_mode_is_scalar_fract_p(MtcsMode *self,mtcs_mode mode);
//原型 #define SCALAR_UFRACT_MODE_P(MODE)    (GET_MODE_CLASS (MODE) == MODE_UFRACT)
nboolean mtcs_mode_is_scalar_ufract_p(MtcsMode *self,mtcs_mode mode);
//原型 #define SCALAR_ACCUM_MODE_P(MODE)   (GET_MODE_CLASS (MODE) == MODE_ACCUM)
nboolean mtcs_mode_is_scalar_accum_p(MtcsMode *self,mtcs_mode mode);
//原型 #define SCALAR_UACCUM_MODE_P(MODE)  (GET_MODE_CLASS (MODE) == MODE_UACCUM)
nboolean mtcs_mode_is_scalar_uaccum_p(MtcsMode *self,mtcs_mode mode);

/* Nonzero if MODE is a scalar/vector fract, ufract, accum or uaccum mode.  */
//原型
//#define ALL_FIXED_POINT_MODE_P(MODE)        \
//  (SIGNED_FIXED_POINT_MODE_P (MODE)     \
//   || UNSIGNED_FIXED_POINT_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_all_fixed_point_p(MtcsMode *self,mtcs_mode mode);

//原型 init_derived_machine_modes rtl.h emit-rtl.cc
void mtcs_mode_init_derived_machine_modes (MtcsMode *self);

//原型
//#define TRULY_NOOP_TRUNCATION_MODES_P(MODE1, MODE2) \
//  (targetm.truly_noop_truncation (GET_MODE_PRECISION (MODE1), \
//                  GET_MODE_PRECISION (MODE2)))
//machmode.h
nboolean mtcs_mode_truly_noop_truncation_p(MtcsMode *self,mtcs_mode out,mtcs_mode in);
//原型 REVERSIBLE_CC_MODE
nushort mtcs_mode_get_reversible_cc_mode(MtcsMode *self,mtcs_mode mode);
/* Get the complex mode from the component mode.  */
// 原型 extern const unsigned short mode_complex[NUM_MACHINE_MODES];
//#define GET_MODE_COMPLEX_MODE(MODE) ((machine_mode) mode_complex[MODE])
nushort mtcs_mode_get_complex(MtcsMode *self,mtcs_mode mode);
//原型 extern machine_mode smallest_mode_for_size (poly_uint64, enum mode_class); machmode.h stor-layout.cc
mtcs_mode mtcs_mode_smallest_mode_for_size (MtcsMode *self,poly_uint64 size, enum mode_class mclass);



//原型 is_int_mode machmode.h
template<typename T>
inline bool mtcs_mode_is_int_mode (MtcsMode *self,machine_mode mode, T *int_mode)
{
  if (mtcs_mode_get_class(self,mode) == MODE_INT){
      *int_mode = scalar_int_mode (scalar_int_mode::from_int (mode));
      return true;
  }
  return false;
}

//原型 is_complex_int_mode machmode.h
template<typename T>
inline bool mtcs_mode_is_complex_int(MtcsMode *self,machine_mode mode, T *cmode)
{
  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(self,mode) == MODE_COMPLEX_INT){
      *cmode = complex_mode (complex_mode::from_int (mode));
      return true;
  }
  return false;
}

/**
 * 离class最近的机器码
 * 原型 GET_CLASS_NARROWEST_MODE machmode.h
 */
mtcs_mode mtcs_mode_get_class_narrowest(MtcsMode *self,enum mode_class cl);

//原型 GET_MODE_WIDER_MODE (const T &m) machmode.h
template<typename T> ALWAYS_INLINE opt_mode<T> mtcs_mode_get_wider (MtcsMode *self,const T &m)
{
  return typename opt_mode<T>::from_int (self->modeWider[m]);
}

//原型 GET_MODE_NEXT_MODE (const T &m) machmode.h
template<typename T> ALWAYS_INLINE opt_mode<T> mtcs_mode_get_next(MtcsMode *self,const T &m)
{
  return typename opt_mode<T>::from_int (self->modeNext[m]);
}

//原型 GET_MODE_2XWIDER_MODE (const T &m) machmode.h
template<typename T> ALWAYS_INLINE opt_mode<T> mtcs_mode_get_2xwider(MtcsMode *self,const T &m)
{
  return typename opt_mode<T>::from_int (self->mode2xwider[m]);
}
/* Return the narrowest mode in T's class.  */

template<typename T> inline T mtcs_mode_get_narrowest_mode (MtcsMode *self,T mode)
{
    //fprintf(stderr,"mtcs_mode_get_narrowest_mode mode:%d %d %d\n",
           // mode,mtcs_mode_get_class(self,mode),self->classNarrowestMode[mtcs_mode_get_class(self,mode)]);
  return typename mode_traits<T>::from_int
    (self->classNarrowestMode[mtcs_mode_get_class/*!GET_MODE_CLASS*/(self,mode)]);
}

/*
 * 注意 from_int
#ifdef USE_ENUM_MODES
  typedef machine_mode from_int;
#else
  enum from_int { dummy = MAX_MACHINE_MODE };
#endif
*/
/* Return true if M represents some kind of scalar value.  */
template<typename T> inline T mtcs_mode_as_a (MtcsMode *self,mtcs_mode m)
{
  gcc_checking_assert (T::includes_p ((void*)self,m));
  return typename mode_traits<T>::from_int (m);
}

//原型 mode_iterator machmode.h
namespace mtcs_mode_iterator
{
  /* Start mode iterator *ITER at the first mode in class MCLASS, if any.  */

  template<typename T>
  inline void
  start (MtcsMode *self,opt_mode<T> *iter, enum mode_class mclass)
  {
    if (mtcs_mode_get_class_narrowest/*!GET_CLASS_NARROWEST_MODE*/ (self,mclass) == E_VOIDmode)
      *iter = opt_mode<T> ();
    else
      *iter = mtcs_mode_as_a<T> (self,mtcs_mode_get_class_narrowest/*!GET_CLASS_NARROWEST_MODE*/ (self,mclass));
  }

  inline void
  start (MtcsMode *self,machine_mode *iter, enum mode_class mclass)
  {
    *iter = mtcs_mode_get_class_narrowest/*!GET_CLASS_NARROWEST_MODE*/ (self,mclass);
  }

  /* Return true if mode iterator *ITER has not reached the end.  */

  template<typename T>
  inline bool
  iterate_p (opt_mode<T> *iter)
  {
    return iter->exists ();
  }

  inline bool
  iterate_p (machine_mode *iter)
  {
    return *iter != E_VOIDmode;
  }

  /* Set mode iterator *ITER to the next mode in the same class,
     if any.  */

  template<typename T>
  inline void
  get_next (MtcsMode *self,opt_mode<T> *iter)
  {
    *iter = mtcs_mode_get_next/*!GET_MODE_NEXT_MODE*/ (self,iter->require ());
  }

  inline void
  get_next (MtcsMode *self,machine_mode *iter)
  {
    *iter =mtcs_mode_get_next/*!GET_MODE_NEXT_MODE*/ (self,*iter).else_void ();
  }

  /* Set mode iterator *ITER to the next mode in the same class.
     Such a mode is known to exist.  */

  template<typename T>
  inline void
  get_known_next (MtcsMode *self,T *iter)
  {
    *iter = mtcs_mode_get_next/*!GET_MODE_NEXT_MODE*/ (self,*iter).require ();
  }

  /* Set mode iterator *ITER to the next wider mode in the same class,
     if any.  */

  template<typename T>
  inline void
  get_wider (MtcsMode *self,opt_mode<T> *iter)
  {
    *iter = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(self,iter->require ());
  }

  inline void
  get_wider (MtcsMode *self,machine_mode *iter)
  {
    *iter = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/ (self,*iter).else_void ();
  }

  /* Set mode iterator *ITER to the next wider mode in the same class.
     Such a mode is known to exist.  */

  template<typename T>
  inline void
  get_known_wider (MtcsMode *self,T *iter)
  {
    *iter = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(self,*iter).require ();
  }

  /* Set mode iterator *ITER to the mode that is two times wider than the
     current one, if such a mode exists.  */

  template<typename T>
  inline void
  get_2xwider (MtcsMode *self,opt_mode<T> *iter)
  {
    *iter = mtcs_mode_get_2xwider/*!GET_MODE_2XWIDER_MODE*/ (self,iter->require ());
  }

  inline void
  get_2xwider (MtcsMode *self,machine_mode *iter)
  {
    *iter = mtcs_mode_get_2xwider/*!GET_MODE_2XWIDER_MODE*/ (self,*iter).else_void ();
  }
}

//原型 FOR_EACH_MODE_IN_CLASS machmode.h
#define MTCS_FOR_EACH_MODE_IN_CLASS(MTCSMODE,ITERATOR, CLASS)  \
  for (mtcs_mode_iterator::start (MTCSMODE,&(ITERATOR), CLASS); \
  mtcs_mode_iterator::iterate_p (&(ITERATOR)); \
  mtcs_mode_iterator::get_next (MTCSMODE,&(ITERATOR)))

/* Make ITERATOR iterate over modes in the same class as MODE, in order
   of increasing width.  Start at the first mode wider than START,
   or don't iterate at all if there is no wider mode.  */
//原型 FOR_EACH_WIDER_MODE machmode.h
#define MTCS_FOR_EACH_WIDER_MODE(MTCSMODE,ITERATOR, START) \
  for ((ITERATOR) = (START), mtcs_mode_iterator::get_wider (MTCSMODE,&(ITERATOR)); \
  mtcs_mode_iterator::iterate_p (&(ITERATOR)); \
  mtcs_mode_iterator::get_wider (MTCSMODE,&(ITERATOR)))

/* Make ITERATOR iterate over all the modes in the range [START, END),
   in order of increasing width.  */
#define MTCS_FOR_EACH_MODE(MTCSMODE,ITERATOR, START, END) \
  for ((ITERATOR) = (START); \
       (ITERATOR) != (END); \
       mtcs_mode_iterator::get_known_next (MTCSMODE,&(ITERATOR)))

/* Make ITERATOR iterate over START and all non-narrower modes in the same
   class, in order of increasing width.  */
//原型 FOR_EACH_MODE_FROM machmode.h
#define MTCS_FOR_EACH_MODE_FROM(MTCSMODE,ITERATOR, START) \
  for ((ITERATOR) = (START); \
  mtcs_mode_iterator::iterate_p (&(ITERATOR)); \
  mtcs_mode_iterator::get_next (MTCSMODE,&(ITERATOR)))

/* Make ITERATOR iterate over modes in the range [NARROWEST, END)
   in order of increasing width, where NARROWEST is the narrowest mode
   in END's class.  */
#define MTCS_FOR_EACH_MODE_UNTIL(MTCSMODE,ITERATOR, END) \
  MTCS_FOR_EACH_MODE (MTCSMODE,ITERATOR, mtcs_mode_get_narrowest_mode (MTCSMODE,END), END)

/* Make ITERATOR iterate over START and all wider modes in the same
   class, in order of strictly increasing width.  */
#define MTCS_FOR_EACH_WIDER_MODE_FROM(MTCSMODE,ITERATOR, START) \
  for ((ITERATOR) = (START); \
  mtcs_mode_iterator::iterate_p (&(ITERATOR)); \
  mtcs_mode_iterator::get_wider (MTCSMODE,&(ITERATOR)))

//原型 FLOAT_LIB_COMPARE_RETURNS_BOOL (mode, comparison) default.h
nboolean mtcs_mode_float_lib_compare_return_bool(MtcsMode *self,mtcs_mode mode,enum rtx_code code);

/* Nonzero if MODE is a scalar fract, ufract, accum or uaccum mode.  */
//原型
//#define ALL_SCALAR_FIXED_POINT_MODE_P(MODE) \
//  (SIGNED_SCALAR_FIXED_POINT_MODE_P (MODE)  \
//   || UNSIGNED_SCALAR_FIXED_POINT_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_all_scalar_fixed_point_p(MtcsMode *self,mtcs_mode mode);

//原型
///* Nonzero if MODE is a scalar fract or accum mode.  */
//#define SIGNED_SCALAR_FIXED_POINT_MODE_P(MODE)  \
//  (SCALAR_FRACT_MODE_P (MODE) || SCALAR_ACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_signed_scalar_fixed_point_p(MtcsMode *self,mtcs_mode mode);

//原型 #define GET_MODE_IBIT(MODE) mode_ibit[MODE]
nuchar   mtcs_mode_get_ibit(MtcsMode *self,mtcs_mode mode);
void     mtcs_mode_set_ibit(MtcsMode *self ,nuchar *modeIBit);
/* Get the number of fractional bits of an object of mode MODE.  */
//原型 GET_MODE_FBIT machmode.h
//extern CONST_MODE_FBIT unsigned char mode_fbit[NUM_MACHINE_MODES];
//#define GET_MODE_FBIT(MODE) mode_fbit[MODE]
nuchar   mtcs_mode_get_fbit(MtcsMode *self,mtcs_mode mode);
//原型 mode_fbit insn-modes.cc
void       mtcs_mode_set_fbit(MtcsMode *self ,nuchar *modeFBit);
//原型 #define FUNCTION_MODE QImode nvptx
void       mtcs_mode_set_function_mode(MtcsMode *self,mtcs_mode functionMode);
mtcs_mode  mtcs_mode_get_function_mode(MtcsMode *self);
//原型 #define STACK_SIZE_MODE Pmode nvptx.h
void       mtcs_mode_set_stack_size_mode(MtcsMode *self,mtcs_mode functionMode);
mtcs_mode  mtcs_mode_get_stack_size_mode(MtcsMode *self);

//原型
///* Nonzero if MODE is a scalar ufract or uaccum mode.  */
//#define UNSIGNED_SCALAR_FIXED_POINT_MODE_P(MODE)    \
//  (SCALAR_UFRACT_MODE_P (MODE) || SCALAR_UACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_unsigned_scalar_fixed_point_p(MtcsMode *self,mtcs_mode mode);
//原型 get_best_mode machmode.h stor-layout.cc
bool mtcs_mode_get_best_mode(MtcsMode *self,int bitsize, int bitpos, poly_uint64 bitregion_start, poly_uint64 bitregion_end,
           unsigned int align, unsigned HOST_WIDE_INT largest_mode_bitsize, bool volatilep, scalar_int_mode *best_mode);

//原型 STACK_SAVEAREA_MODE default.h
mtcs_mode mtcs_mode_get_stack_savearea_mode(MtcsMode *self,enum save_level level);
//原型 REGMODE_NATURAL_SIZE regs.h
//原型 #define REGMODE_NATURAL_SIZE(MODE) ix86_regmode_natural_size (MODE)
//nvptx用的是regs.h中的#define REGMODE_NATURAL_SIZE(MODE) UNITS_PER_WORD
//#define UNITS_PER_WORD        (TARGET_64BIT ? 8 : 4) i386的定义 nvptx  #define UNITS_PER_WORD 8
int mtcs_mode_get_regmode_natural_size(MtcsMode *self,mtcs_mode mode);

void mtcs_mode_set_name_data(MtcsMode *self,char **name);
void mtcs_mode_set_size_data(MtcsMode *self,poly_uint16 *modeSize);
void mtcs_mode_set_class_data(MtcsMode *self,nuchar *modeClass);
void mtcs_mode_set_base_align_data(MtcsMode *self,nushort *modeBaseAlign);
void mtcs_mode_set_precision_data(MtcsMode *self,poly_uint16 *modePrecision);
void mtcs_mode_set_inner_data(MtcsMode *self,nushort *modeInner);
void mtcs_mode_set_nunits_data(MtcsMode *self,poly_uint16 *modeNunits);
void mtcs_mode_set_unit_size_data(MtcsMode *self,nuchar *modeUnitSize);
void mtcs_mode_set_unit_precision_data(MtcsMode *self,nushort *modeUnitPrecision);
void mtcs_mode_set_class_narrowest_data(MtcsMode *self,nushort *classNarrowestMode);
void mtcs_mode_set_wider_data(MtcsMode *self,nushort *modeWider);
void mtcs_mode_set_next_data(MtcsMode *self,nushort *modeNext);
void mtcs_mode_set_2xwider_data(MtcsMode *self,nushort *mode2xwider);
void mtcs_mode_set_complex_data(MtcsMode *self,nushort *modeComplex);

//原型 NUM_MODE_IP_INT expmed.h
inline nuint mtcs_mode_get_num_ip_int(MtcsMode *self)
{
    return self->modesNum.num_INT+self->modesNum.num_PARTIAL_INT;
}

//原型 mode_to_bytes mach_mode.h
inline poly_uint16 mtcs_mode_mode_to_bytes (MtcsMode *self,machine_mode mode)
{
    return self->modeSize[mode];
}

//原型 NUM_MODE_IPV_INT expmed.h
inline nuint mtcs_mode_get_num_ipv_int(MtcsMode *self)
{
    return mtcs_mode_get_num_ip_int(self)+self->modesNum.num_VECTOR_INT;
}

//原型 #define NARROWEST_INT_MODE  ( scalar_int_mode(scalar_int_mode::from_int (class_narrowest_mode[MODE_INT])) )
inline mtcs_mode mtcs_mode_get_narrowest_int_mode(MtcsMode *self)
{
  return (mtcs_mode)(scalar_int_mode::from_int (self->classNarrowestMode[MODE_INT]));
}

/**
 * 有5种类型需要判断
 * 1.scalar_int_mode
 * 2.scalar_float_mode
 * 3.scalar_mode
 * 4.complex_mode
 * 5.fixed_size_mode
 */
inline bool scalar_int_mode::includes_p (void *self,machine_mode m)
{
  return mtcs_mode_is_scalar_int_p((MtcsMode *)self,m);
}

inline bool scalar_float_mode::includes_p (void *self,machine_mode m)
{
  return mtcs_mode_is_scalar_float_p((MtcsMode *)self,m);
}

inline bool scalar_mode::includes_p (void *self,machine_mode m)
{
  switch (mtcs_mode_get_class((MtcsMode *)self,m))
    {
    case MODE_INT:
    case MODE_PARTIAL_INT:
    case MODE_FRACT:
    case MODE_UFRACT:
    case MODE_ACCUM:
    case MODE_UACCUM:
    case MODE_FLOAT:
    case MODE_DECIMAL_FLOAT:
      return true;
    default:
      return false;
    }
}

inline bool complex_mode::includes_p (void *self,machine_mode m)
{
  return mtcs_mode_is_complex_p((MtcsMode *)self,m);
}

inline bool fixed_size_mode::includes_p (void *self,machine_mode mode)
{
  return mtcs_mode_mode_to_bytes ((MtcsMode *)self,mode).is_constant ();
}

template<typename T>inline opt_mode<T> mtcs_mode_dyn_cast (MtcsMode *self,machine_mode m)
{
  if (T::includes_p ((void*)self,m))
    return T (typename mode_traits<T>::from_int (m));
  return opt_mode<T> ();
}

template<typename T, typename U> inline opt_mode<T> mtcs_mode_dyn_cast (MtcsMode *self,const opt_mode<U> &m)
{
  return mtcs_mode_dyn_cast <T> (self,m.else_void ());
}

//原型 is_a machmode.h
template<typename T> inline bool mtcs_mode_is_a(MtcsMode *self,machine_mode m)
{
  return T::includes_p((void*)self,m);
}

template<typename T, typename U> inline bool mtcs_mode_is_a(MtcsMode *self,const opt_mode<U> &m)
{
  return T::includes_p ((void*)self,m.else_void ());
}

template<typename T, typename U> inline bool mtcs_mode_is_a(MtcsMode *self,machine_mode m, U *result)
{
  if (T::includes_p ((void*)self,m)){
      *result = T (typename mode_traits<T>::from_int (m));
      return true;
  }
  return false;
}

//原形 as_a machmode.h
template<typename T> inline T mtcs_mode_as_a(MtcsMode *self,machine_mode m)
{
  gcc_checking_assert (T::includes_p((void*)self,m));
  return typename mode_traits<T>::from_int (m);
}

template<typename T, typename U> inline T mtcs_mode_as_a (MtcsMode *self,const opt_mode<U> &m)
{
  return mtcs_mode_as_a <T> (self,m.else_void ());
}

/* Return the machine mode to use for a MODE_FLOAT of SIZE bits, if one
   exists.  */
//原型 float_mode_for_size machmode.h
inline opt_scalar_float_mode mtcs_mode_float_mode_for_size (MtcsMode *self,poly_uint64 size)
{
    return mtcs_mode_dyn_cast <scalar_float_mode> (self,mtcs_mode_mode_for_size/*!mode_for_size*/(self,size, MODE_FLOAT, 0));
}


/* Return true if MODE is a scalar integer mode with a precision
   smaller than LIMIT's precision.  */
//原型 is_narrower_int_mode machmode.h
inline bool mtcs_mode_is_narrower_int_mode (MtcsMode *self,machine_mode mode, scalar_int_mode limit)
{
  scalar_int_mode int_mode;
  return (mtcs_mode_is_a <scalar_int_mode>(self,mode, &int_mode)
      && mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(self,int_mode) < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(self,limit));
}

/* Find the narrowest integer mode that contains at least SIZE bits.
   Such a mode must exist.  */
//原型 smallest_int_mode_for_size machmode.h
inline scalar_int_mode mtcs_mode_smallest_int_mode_for_size (MtcsMode *self,poly_uint64 size)
{
  return mtcs_mode_as_a <scalar_int_mode> (self,(machine_mode)mtcs_mode_smallest_mode_for_size (self,size, MODE_INT));
}

/* Return true if a subreg of mode OUTERMODE would only access part of
   an inner register with mode INNERMODE.  The other bits of the inner
   register would then be "don't care" on read.  The behavior for writes
   depends on REGMODE_NATURAL_SIZE; bits in the same REGMODE_NATURAL_SIZE-d
   chunk would be clobbered but other bits would be preserved.  */
//原型 partial_subreg_p rtl.h
inline bool mtcs_mode_partial_subreg_p (MtcsMode *self,machine_mode outermode, machine_mode innermode)
{
  /* Modes involved in a subreg must be ordered.  In particular, we must
     always know at compile time whether the subreg is paradoxical.  */
  poly_int64 outer_prec =mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(self,outermode);
  poly_int64 inner_prec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(self,innermode);
  gcc_checking_assert (ordered_p (outer_prec, inner_prec));
  return maybe_lt (outer_prec, inner_prec);
}

/* Given that a subreg has outer mode OUTERMODE and inner mode INNERMODE,
   return the mode that is big enough to hold both the outer and inner
   values.  Prefer the outer mode in the event of a tie.  */
//原型 wider_subreg_mode rtl.h
inline machine_mode mtcs_mode_wider_subreg_mode (MtcsMode *self,machine_mode outermode, machine_mode innermode)
{
  return mtcs_mode_partial_subreg_p/*!partial_subreg_p*/(self,outermode, innermode) ? innermode : outermode;
}

//原型 MACRO_MODE machmode.h
mtcs_mode mtcs_mode_get_macro(MtcsMode *self,mtcs_mode mode);
//原型 init_adjust_machine_modes machmode.h insn-modes.cc
void mtcs_mode_init_adjust_machine_modes(MtcsMode *self);
//原型 LOAD_EXTEND_OP(M) defaults.h
rtx_code mtcs_mode_load_extend_op(MtcsMode *self,mtcs_mode m);
//原型 targetm.unwind_word_mode() #define TARGET_UNWIND_WORD_MODE default_unwind_word_mode
scalar_int_mode mtcs_mode_unwind_word_mode(MtcsMode *self);
//原型 targetm.eh_return_filter_mode () #define TARGET_EH_RETURN_FILTER_MODE default_eh_return_filter_mode
scalar_int_mode mtcs_mode_eh_return_filter_mode(MtcsMode *self);
//原型 #define IS_STACK_MODE(MODE)   (X87_FLOAT_MODE_P (MODE) && (!(SSE_FLOAT_MODE_P (MODE) && TARGET_SSE_MATH) || TARGET_MIX_SSE_I387))
//nvptx 未定义 IS_STACK_MODE
nboolean mtcs_mode_is_stack_mode(MtcsMode *self,machine_mode m);
//原型 #define OPAQUE_MODE_P(MODE)    (GET_MODE_CLASS (MODE) == MODE_OPAQUE)
nboolean mtcs_mode_opaque_mode_p(MtcsMode *self,machine_mode m);
//原型 subreg_offset_from_lsb rtl.h
poly_uint64 mtcs_mode_subreg_offset_from_lsb (MtcsMode *self,machine_mode outer_mode,
         machine_mode inner_mode,poly_uint64 lsb_shift);
//原型 targetm.c.mode_for_floating_type (TI_DOUBLE_TYPE); #define TARGET_C_MODE_FOR_FLOATING_TYPE default_mode_for_floating_type
machine_mode mtcs_mode_for_floating_type(MtcsMode *self,enum tree_index ti);
//原型 push_rounding 平台定义
poly_int64 mtcs_mode_push_rounding(MtcsMode *self,poly_int64 bytes);
#endif

