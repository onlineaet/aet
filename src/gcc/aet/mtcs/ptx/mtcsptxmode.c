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
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "mtcsptxmode.h"
#include "ptx-common.h"
#include "gen/ptx-insn-modes.h"
#include "gen/ptx-optionsitem.h"

static mtcs_mode      getPmode_cb(MtcsMode *mtcsMode);//返回Pmode
const char           *getType_cb(MtcsMode *mtcsMode,mtcs_mode mode, bool promote);
static bool           standardTypeBitSize_cb (MtcsMode *mtcsMode,int bitsize);
static void           initInt_cb (MtcsMode *mtcsMode);
static mtcs_mode      promoteInner_cb(MtcsMode *mtcsMode,mtcs_mode mode,int unsignedp,const_tree type);//原型是宏PROMOTE_MODE
static unsigned HOST_WIDE_INT  getMask_cb(MtcsMode *mtcsMode,mtcs_mode mode);
static struct real_format *getRealFormat_cb(MtcsMode *mtcsMode,mtcs_mode mode);
static int            clzDefinedValueAtZero_cb(MtcsMode *mtcsMode,mtcs_mode mode,int *value);
static int            ctzDefinedValueAtZero_cb(MtcsMode *mtcsMode,mtcs_mode mode,int *value);
static nushort        getReversibleCCMode_cb(MtcsMode *mtcsMode,mtcs_mode mode); //原型 REVERSIBLE_CC_MODE
//原型 STACK_SAVEAREA_MODE default.h
static mtcs_mode      getStackSaveareaMode_cb(MtcsMode *mtcsMode,enum save_level level);
//原型 REGMODE_NATURAL_SIZE regs.h
static int            getRegmodeNaturalSize_cb(MtcsMode *mtcsMode,mtcs_mode mode);
//原型 init_adjust_machine_modes machmode.h insn-modes.cc
static void initAdjustMachineModes_cb(MtcsMode *mtcsMode);
//原型 LOAD_EXTEND_OP(M) defaults.h
static rtx_code loadExtendOp_cb(MtcsMode *mtcsMode,mtcs_mode m);
//原型 targetm.c.mode_for_floating_type (TI_DOUBLE_TYPE); #define TARGET_C_MODE_FOR_FLOATING_TYPE default_mode_for_floating_type
static  machine_mode modeForFloatingType_cb(MtcsMode *self,enum tree_index ti);

static void setModeValue(MtcsPtxMode *self)
{
    MtcsMode *mtcsMode=(MtcsMode *)self;
    mtcsMode->modes.M_VOIDmode=PTX_VOIDmode;            /* machmode.def:194 */
    mtcsMode->modes.M_BLKmode=PTX_BLKmode;               /* machmode.def:198 */
    mtcsMode->modes.M_CCmode=PTX_CCmode;                /* machmode.def:236 */
    mtcsMode->modes.M_BImode=PTX_BImode;                /* machmode.def:201 */
    mtcsMode->modes.M_QImode=PTX_QImode;                /* machmode.def:209 */
    mtcsMode->modes.M_HImode=PTX_HImode;                /* machmode.def:210 */
    mtcsMode->modes.M_SImode=PTX_SImode;                /* machmode.def:211 */
    mtcsMode->modes.M_DImode=PTX_DImode;                /* machmode.def:212 */
    mtcsMode->modes.M_TImode=PTX_TImode;                /* machmode.def:213 */
    mtcsMode->modes.M_QQmode=PTX_QQmode;                /* machmode.def:239 */
    mtcsMode->modes.M_HQmode=PTX_HQmode;                /* machmode.def:240 */
    mtcsMode->modes.M_SQmode=PTX_SQmode;                /* machmode.def:241 */
    mtcsMode->modes.M_DQmode=PTX_DQmode;                /* machmode.def:242 */
    mtcsMode->modes.M_TQmode=PTX_TQmode;                /* machmode.def:243 */
    mtcsMode->modes.M_UQQmode=PTX_UQQmode;               /* machmode.def:245 */
    mtcsMode->modes.M_UHQmode=PTX_UHQmode;               /* machmode.def:246 */
    mtcsMode->modes.M_USQmode=PTX_USQmode;               /* machmode.def:247 */
    mtcsMode->modes.M_UDQmode=PTX_UDQmode;               /* machmode.def:248 */
    mtcsMode->modes.M_UTQmode=PTX_UTQmode;               /* machmode.def:249 */
    mtcsMode->modes.M_HAmode=PTX_HAmode;                /* machmode.def:251 */
    mtcsMode->modes.M_SAmode=PTX_SAmode;                /* machmode.def:252 */
    mtcsMode->modes.M_DAmode=PTX_DAmode;                /* machmode.def:253 */
    mtcsMode->modes.M_TAmode=PTX_TAmode;                /* machmode.def:254 */
    mtcsMode->modes.M_UHAmode=PTX_UHAmode;               /* machmode.def:256 */
    mtcsMode->modes.M_USAmode=PTX_USAmode;               /* machmode.def:257 */
    mtcsMode->modes.M_UDAmode=PTX_UDAmode;               /* machmode.def:258 */
    mtcsMode->modes.M_UTAmode=PTX_UTAmode;               /* machmode.def:259 */
    mtcsMode->modes.M_HFmode=PTX_HFmode;                /* config/nvptx/nvptx-modes.def:1 */
    mtcsMode->modes.M_SFmode=PTX_SFmode;                /* machmode.def:231 */
    mtcsMode->modes.M_DFmode=PTX_DFmode;                /* machmode.def:232 */
    mtcsMode->modes.M_SDmode=PTX_SDmode;                /* machmode.def:272 */
    mtcsMode->modes.M_DDmode=PTX_DDmode;                /* machmode.def:273 */
    mtcsMode->modes.M_TDmode=PTX_TDmode;                /* machmode.def:274 */
    mtcsMode->modes.M_CQImode=PTX_CQImode;               /* machmode.def:267 */
    mtcsMode->modes.M_CHImode=PTX_CHImode;               /* machmode.def:267 */
    mtcsMode->modes.M_CSImode=PTX_CSImode;               /* machmode.def:267 */
    mtcsMode->modes.M_CDImode=PTX_CDImode;               /* machmode.def:267 */
    mtcsMode->modes.M_CTImode=PTX_CTImode;               /* machmode.def:267 */
    mtcsMode->modes.M_HCmode=PTX_HCmode;                /* machmode.def:269 */
    mtcsMode->modes.M_SCmode=PTX_SCmode;                /* machmode.def:269 */
    mtcsMode->modes.M_DCmode=PTX_DCmode;                /* machmode.def:269 */
    mtcsMode->modes.M_V2SImode=PTX_V2SImode;              /* config/nvptx/nvptx-modes.def:3 */
    mtcsMode->modes.M_V2DImode=PTX_V2DImode;              /* config/nvptx/nvptx-modes.def:5 */
}

static void setModesMinMax(MtcsPtxMode *self)
{
    MtcsMode *mtcsMode=(MtcsMode *)self;
    mtcsMode->modesMinMax.min_RANDOM=PTX_MIN_MODE_RANDOM;
    mtcsMode->modesMinMax.max_RANDOM=PTX_MAX_MODE_RANDOM;

    mtcsMode->modesMinMax.min_CC=PTX_MIN_MODE_CC;
    mtcsMode->modesMinMax.max_CC=PTX_MAX_MODE_CC;

    mtcsMode->modesMinMax.min_BOOL=PTX_MIN_MODE_BOOL;
    mtcsMode->modesMinMax.max_BOOL=PTX_MAX_MODE_BOOL;

    mtcsMode->modesMinMax.min_INT=PTX_MIN_MODE_INT;
    mtcsMode->modesMinMax.max_INT=PTX_MAX_MODE_INT;

    mtcsMode->modesMinMax.min_PARTIAL_INT=PTX_MIN_MODE_PARTIAL_INT;
    mtcsMode->modesMinMax.max_PARTIAL_INT=PTX_MAX_MODE_PARTIAL_INT;

    mtcsMode->modesMinMax.min_FRACT=PTX_MIN_MODE_FRACT;
    mtcsMode->modesMinMax.max_FRACT=PTX_MAX_MODE_FRACT;

    mtcsMode->modesMinMax.min_UFRACT=PTX_MIN_MODE_UFRACT;
    mtcsMode->modesMinMax.max_UFRACT=PTX_MAX_MODE_UFRACT;

    mtcsMode->modesMinMax.min_ACCUM=PTX_MIN_MODE_ACCUM;
    mtcsMode->modesMinMax.max_ACCUM=PTX_MAX_MODE_ACCUM;

    mtcsMode->modesMinMax.min_FLOAT=PTX_MIN_MODE_FLOAT;
    mtcsMode->modesMinMax.max_FLOAT=PTX_MAX_MODE_FLOAT;

    mtcsMode->modesMinMax.min_DECIMAL_FLOAT=PTX_MIN_MODE_DECIMAL_FLOAT;
    mtcsMode->modesMinMax.max_DECIMAL_FLOAT=PTX_MAX_MODE_DECIMAL_FLOAT;

    mtcsMode->modesMinMax.min_COMPLEX_INT=PTX_MIN_MODE_COMPLEX_INT;
    mtcsMode->modesMinMax.max_COMPLEX_INT=PTX_MAX_MODE_COMPLEX_INT;

    mtcsMode->modesMinMax.min_COMPLEX_FLOAT=PTX_MIN_MODE_COMPLEX_FLOAT;
    mtcsMode->modesMinMax.max_COMPLEX_FLOAT=PTX_MAX_MODE_COMPLEX_FLOAT;

    mtcsMode->modesMinMax.min_VECTOR_BOOL=PTX_MIN_MODE_VECTOR_BOOL;
    mtcsMode->modesMinMax.max_VECTOR_BOOL=PTX_MAX_MODE_VECTOR_BOOL;

    mtcsMode->modesMinMax.min_VECTOR_INT=PTX_MIN_MODE_VECTOR_INT;
    mtcsMode->modesMinMax.max_VECTOR_INT=PTX_MAX_MODE_VECTOR_INT;

    mtcsMode->modesMinMax.min_VECTOR_FRACT=PTX_MIN_MODE_VECTOR_FRACT;
    mtcsMode->modesMinMax.max_VECTOR_FRACT=PTX_MAX_MODE_VECTOR_FRACT;

    mtcsMode->modesMinMax.min_VECTOR_UFRACT=PTX_MIN_MODE_VECTOR_UFRACT;
    mtcsMode->modesMinMax.max_VECTOR_UFRACT=PTX_MAX_MODE_VECTOR_UFRACT;

    mtcsMode->modesMinMax.min_VECTOR_ACCUM=PTX_MIN_MODE_VECTOR_ACCUM;
    mtcsMode->modesMinMax.max_VECTOR_ACCUM=PTX_MAX_MODE_VECTOR_ACCUM;

    mtcsMode->modesMinMax.min_VECTOR_UACCUM=PTX_MIN_MODE_VECTOR_UACCUM;
    mtcsMode->modesMinMax.max_VECTOR_UACCUM=PTX_MAX_MODE_VECTOR_UACCUM;

    mtcsMode->modesMinMax.min_VECTOR_FLOAT=PTX_MIN_MODE_VECTOR_FLOAT;
    mtcsMode->modesMinMax.max_VECTOR_FLOAT=PTX_MAX_MODE_VECTOR_FLOAT;

    mtcsMode->modesMinMax.min_OPAQUE=PTX_MIN_MODE_OPAQUE;
    mtcsMode->modesMinMax.max_OPAQUE=PTX_MAX_MODE_OPAQUE;
}

static void setModesNum(MtcsPtxMode *self)
{
    MtcsMode *mtcsMode=(MtcsMode *)self;
    mtcsMode->modesNum.num_RANDOM=PTX_MAX_MODE_RANDOM-PTX_MIN_MODE_RANDOM+1;
    mtcsMode->modesNum.num_CC=PTX_MAX_MODE_CC-PTX_MIN_MODE_CC+1;
    mtcsMode->modesNum.num_INT=PTX_MAX_MODE_INT-PTX_MIN_MODE_INT+1;
    mtcsMode->modesNum.num_PARTIAL_INT=0;
    mtcsMode->modesNum.num_FRACT=PTX_MAX_MODE_FRACT-PTX_MIN_MODE_FRACT+1;
    mtcsMode->modesNum.num_UFRACT=PTX_MAX_MODE_UFRACT-PTX_MIN_MODE_UFRACT+1;
    mtcsMode->modesNum.num_ACCUM=PTX_MAX_MODE_ACCUM-PTX_MIN_MODE_ACCUM+1;
    mtcsMode->modesNum.num_UACCUM=PTX_MAX_MODE_UACCUM-PTX_MIN_MODE_UACCUM+1;
    mtcsMode->modesNum.num_FLOAT=PTX_MAX_MODE_FLOAT-PTX_MIN_MODE_FLOAT+1;
    mtcsMode->modesNum.num_DECIMAL_FLOAT=PTX_MAX_MODE_DECIMAL_FLOAT-PTX_MIN_MODE_DECIMAL_FLOAT+1;
    mtcsMode->modesNum.num_COMPLEX_INT=PTX_MAX_MODE_COMPLEX_INT-PTX_MIN_MODE_COMPLEX_INT+1;
    mtcsMode->modesNum.num_COMPLEX_FLOAT=PTX_MAX_MODE_COMPLEX_FLOAT-PTX_MIN_MODE_COMPLEX_FLOAT+1;
    mtcsMode->modesNum.num_VECTOR_BOOL=0;
    mtcsMode->modesNum.num_VECTOR_INT=PTX_MAX_MODE_VECTOR_INT-PTX_MIN_MODE_VECTOR_INT+1;
    mtcsMode->modesNum.num_VECTOR_FRACT=0;
    mtcsMode->modesNum.num_VECTOR_UFRACT=0;
    mtcsMode->modesNum.num_VECTOR_ACCUM=0;
    mtcsMode->modesNum.num_VECTOR_UACCUM=0;
    mtcsMode->modesNum.num_VECTOR_FLOAT=0;
    mtcsMode->modesNum.num_OPAQUE=0;

}

static void mtcsPtxModeInit(MtcsPtxMode *self)
{
    MtcsMode *mtcsMode=(MtcsMode *)self;
    const char ** ptxModeName=ptx_get_modes_name();
    int i;
    for(i=0;i<PTX_NUM_MACHINE_MODES;i++)
        n_hash_table_insert(mtcsMode->modeNameHashTable,n_strdup(ptxModeName[i]),NINT_TO_POINTER(i));//ptxModeName来自ptx-insn-modes.h
    mtcsMode->get_Pmode=getPmode_cb;
    mtcsMode->get_type=getType_cb;
    mtcsMode->standard_type_bitsize=standardTypeBitSize_cb;
    mtcsMode->init_int=initInt_cb;
    mtcsMode->promote_inner=promoteInner_cb;//原型是宏PROMOTE_MODE
    mtcsMode->get_mask=getMask_cb;
    mtcsMode->get_real_format=getRealFormat_cb;
    mtcsMode->clz_defined_value_at_zero=clzDefinedValueAtZero_cb;
    mtcsMode->ctz_defined_value_at_zero=ctzDefinedValueAtZero_cb;
    mtcsMode->get_reversible_cc_mode=getReversibleCCMode_cb; //原型 REVERSIBLE_CC_MODE
    //原型 STACK_SAVEAREA_MODE default.h
    mtcsMode->get_stack_savearea_mode=getStackSaveareaMode_cb;
    //原型 REGMODE_NATURAL_SIZE regs.h
    mtcsMode->get_regmode_natural_size=getRegmodeNaturalSize_cb;
    //原型 init_adjust_machine_modes machmode.h insn-modes.cc
    mtcsMode->init_adjust_machine_modes=initAdjustMachineModes_cb;
    //原型 LOAD_EXTEND_OP(M) defaults.h
    mtcsMode->load_extend_op=loadExtendOp_cb;
    //原型 targetm.c.mode_for_floating_type (TI_DOUBLE_TYPE); #define TARGET_C_MODE_FOR_FLOATING_TYPE default_mode_for_floating_type
    mtcsMode->mode_for_floating_type=modeForFloatingType_cb;
    //原型 NUM_MACHINE_MODES insn-modes.h
    mtcs_mode_set_number(mtcsMode,PTX_NUM_MACHINE_MODES);
    //原型 MAX_MACHINE_MODE insn-modes.h
    mtcs_mode_set_max_number(mtcsMode,PTX_MAX_MACHINE_MODE);
    mtcs_mode_set_name_data(mtcsMode,ptxModeName);
    mtcs_mode_set_size_data(mtcsMode,ptx_get_mode_size());
    mtcs_mode_set_class_data(mtcsMode,ptx_get_mode_class());//ptx_get_mode_class来自ptx-insn-modes.h
    mtcs_mode_set_base_align_data(mtcsMode,ptx_get_mode_base_align());
    mtcs_mode_set_precision_data(mtcsMode,ptx_get_mode_precision());
    mtcs_mode_set_inner_data(mtcsMode,ptx_get_mode_inner());
    mtcs_mode_set_nunits_data(mtcsMode,ptx_get_mode_nunits());
    mtcs_mode_set_unit_size_data(mtcsMode,ptx_get_mode_unit_size());
    mtcs_mode_set_unit_precision_data(mtcsMode,ptx_get_mode_unit_precision());
    mtcs_mode_set_class_narrowest_data(mtcsMode,ptx_get_class_narrowest_mode());
    mtcs_mode_set_wider_data(mtcsMode,ptx_get_mode_wider());
    mtcs_mode_set_next_data(mtcsMode ,ptx_get_mode_next());
    mtcs_mode_set_2xwider_data(mtcsMode ,ptx_get_mode_2xwider());
    mtcs_mode_set_complex_data(mtcsMode,ptx_get_mode_complex());
    //原型 mode_fbit insn-modes.cc
    mtcs_mode_set_fbit(mtcsMode,ptx_get_mode_fbit());
    //原型 mode_ibit insn-modes.cc
    mtcs_mode_set_ibit(mtcsMode,ptx_get_mode_ibit());
    //原型 MAX_BITSIZE_MODE_ANY_INT insn-modes.h
    mtcs_mode_set_max_bitsize_mode_any_int(mtcsMode,PTX_MAX_BITSIZE_MODE_ANY_INT);
    //原型 #define FUNCTION_MODE QImode nvptx
    mtcs_mode_set_function_mode(mtcsMode,PTX_FUNCTION_MODE);
    mtcs_mode_set_stack_size_mode(mtcsMode,PTX_Pmode);
    setModeValue(self);
    setModesMinMax(self);
    setModesNum(self);
    mtcs_mode_init_derived_machine_modes(mtcsMode);
}

/**
 * 返回Pmode
 */
static mtcs_mode getPmode_cb(MtcsMode *mtcsMode)
{
   return (mtcs_mode)PTX_Pmode;
}

static bool standardTypeBitSize_cb (MtcsMode *mtcsMode,int bitsize)
{
  /* As a special exception, we always want __int128 enabled if possible.  */
  if (bitsize == 128)
    return false;
  if (bitsize == CHAR_TYPE_SIZE
      || bitsize == PTX_SHORT_TYPE_SIZE
      || bitsize == PTX_INT_TYPE_SIZE
      || bitsize == PTX_LONG_TYPE_SIZE
      || bitsize == PTX_LONG_LONG_TYPE_SIZE)
    return true;
  return false;
}

static void  initInt_cb (MtcsMode *mtcsMode)
{
   // mtcs_mode_init_int(mtcsMode,ptx_int_n_data,PTX_NUM_INT_N_ENTS);
    mtcsMode->intData=ptx_int_n_data;
    mtcsMode->mtcs_NUM_INT_N_ENTS=PTX_NUM_INT_N_ENTS;
    MtcsTarget *target=(MtcsTarget *)mtcsMode->target;
    mtcsMode->int_n_enabled_p=xmalloc(sizeof(bool)*PTX_NUM_INT_N_ENTS);
    int i;
    for (i = 0; i < PTX_NUM_INT_N_ENTS; i ++)
        if (target->scalar_mode_supported_p (target,(mtcs_mode)ptx_int_n_data[i].m)
                && !mtcsMode->standard_type_bitsize (mtcsMode,ptx_int_n_data[i].bitsize))
            mtcsMode->int_n_enabled_p[i] = true;
        else
            mtcsMode->int_n_enabled_p[i] = false;
}

/**
 * 从外部调用
 */
bool mtcs_ptx_mode_libgcc_floating_mode_supported_p(MtcsPtxMode *self,scalar_mode smode)
{
   mtcs_mode  mode=smode;//mtcs_mode_host2device((MtcsMode *)self,hostMode);
   switch (mode){
      case PTX_SFmode:
      case PTX_DFmode:
         return true;
      default:
         return false;
   }
}

/* Return a ptx type for MODE.  If PROMOTE, then use .u32 for QImode to
   deal with ptx ideosyncracies.  */
//原型 const char *nvptx_ptx_type_from_mode (machine_mode mode, bool promote) nvptx-protols.h
const char  *getType_cb(MtcsMode *mtcsMode,mtcs_mode mode, bool promote)
{
   n_debug("mtcsptxmode.c-----nvptx.cc -----2-- nvptx_ptx_type_from_mode (MtcsPtx *self,mtcs_mode mode, bool promote) mode:%d \n",mode);
   switch (mode){
      case PTX_BLKmode:
         return ".b8";
      case PTX_BImode:
         return ".pred";
      case PTX_QImode:
         if (promote)
            return ".u32";
         else
            return ".u8";
      case PTX_HImode:
         return ".u16";
      case PTX_SImode:
         return ".u32";
      case PTX_DImode:
         return ".u64";
      case PTX_HFmode:
         return ".f16";
      case PTX_SFmode:
         return ".f32";
      case PTX_DFmode:
         return ".f64";
      case PTX_V2SImode:
         return ".v2.u32";
      case PTX_V2DImode:
         return ".v2.u64";
      default:
         gcc_unreachable ();
   }
}

/**
 * 原型
 *  if ((MODE) == QImode || (MODE) == HImode)      \
       {                           \
         (MODE) = SImode;                  \
         (void)(UNSIGNEDP);                \
         (void)(TYPE);                 \
       }
 */
static mtcs_mode  promoteInner_cb(MtcsMode *self,mtcs_mode mode,int unsignedp,const_tree type)
{
   if(mode==PTX_QImode || mode==PTX_HImode)
      return PTX_SImode;
   return mode;
}

const unsigned HOST_WIDE_INT ptx_mode_mask_array[PTX_NUM_MACHINE_MODES] =
{
#define MODE_MASK(m)                          \
  ((m) >= HOST_BITS_PER_WIDE_INT)             \
   ? HOST_WIDE_INT_M1U                        \
   : (HOST_WIDE_INT_1U << (m)) - 1

  MODE_MASK (0),           /* VOID */
  MODE_MASK (0),           /* BLK */
  MODE_MASK (4*BITS_PER_UNIT),   /* CC */
  MODE_MASK (1),           /* BI */
  MODE_MASK (1*BITS_PER_UNIT),   /* QI */
  MODE_MASK (2*BITS_PER_UNIT),   /* HI */
  MODE_MASK (4*BITS_PER_UNIT),   /* SI */
  MODE_MASK (8*BITS_PER_UNIT),   /* DI */
  MODE_MASK (16*BITS_PER_UNIT),    /* TI */
  MODE_MASK (1*BITS_PER_UNIT),   /* QQ */
  MODE_MASK (2*BITS_PER_UNIT),   /* HQ */
  MODE_MASK (4*BITS_PER_UNIT),   /* SQ */
  MODE_MASK (8*BITS_PER_UNIT),   /* DQ */
  MODE_MASK (16*BITS_PER_UNIT),    /* TQ */
  MODE_MASK (1*BITS_PER_UNIT),   /* UQQ */
  MODE_MASK (2*BITS_PER_UNIT),   /* UHQ */
  MODE_MASK (4*BITS_PER_UNIT),   /* USQ */
  MODE_MASK (8*BITS_PER_UNIT),   /* UDQ */
  MODE_MASK (16*BITS_PER_UNIT),    /* UTQ */
  MODE_MASK (2*BITS_PER_UNIT),   /* HA */
  MODE_MASK (4*BITS_PER_UNIT),   /* SA */
  MODE_MASK (8*BITS_PER_UNIT),   /* DA */
  MODE_MASK (16*BITS_PER_UNIT),    /* TA */
  MODE_MASK (2*BITS_PER_UNIT),   /* UHA */
  MODE_MASK (4*BITS_PER_UNIT),   /* USA */
  MODE_MASK (8*BITS_PER_UNIT),   /* UDA */
  MODE_MASK (16*BITS_PER_UNIT),    /* UTA */
  MODE_MASK (2*BITS_PER_UNIT),   /* HF */
  MODE_MASK (4*BITS_PER_UNIT),   /* SF */
  MODE_MASK (8*BITS_PER_UNIT),   /* DF */
  MODE_MASK (4*BITS_PER_UNIT),   /* SD */
  MODE_MASK (8*BITS_PER_UNIT),   /* DD */
  MODE_MASK (16*BITS_PER_UNIT),    /* TD */
  MODE_MASK (2*BITS_PER_UNIT),   /* CQI */
  MODE_MASK (4*BITS_PER_UNIT),   /* CHI */
  MODE_MASK (8*BITS_PER_UNIT),   /* CSI */
  MODE_MASK (16*BITS_PER_UNIT),    /* CDI */
  MODE_MASK (32*BITS_PER_UNIT),    /* CTI */
  MODE_MASK (4*BITS_PER_UNIT),   /* HC */
  MODE_MASK (8*BITS_PER_UNIT),   /* SC */
  MODE_MASK (16*BITS_PER_UNIT),    /* DC */
  MODE_MASK (8*BITS_PER_UNIT),   /* V2SI */
  MODE_MASK (16*BITS_PER_UNIT),    /* V2DI */
#undef MODE_MASK
};

static unsigned HOST_WIDE_INT getMask_cb(MtcsMode *self,mtcs_mode mode)
{
   gcc_assert (mode >= 0 && mode < PTX_NUM_MACHINE_MODES);
   return ptx_mode_mask_array[mode];
}

//#define REAL_MODE_FORMAT(MODE)                      \
//  (real_format_for_mode[DECIMAL_FLOAT_MODE_P (MODE)         \
//            ? ( ( (MODE) - MIN_MODE_DECIMAL_FLOAT )        \
//               + NUM_MODE_FLOAT)                \
//            : GET_MODE_CLASS (MODE) == MODE_FLOAT       \
//            ? ((MODE) - MIN_MODE_FLOAT)         \
//            : (gcc_unreachable (), 0)])
//原型 real.h REAL_MODE_FORMAT
static struct real_format *getRealFormat_cb(MtcsMode *self,mtcs_mode mode)
{
   nboolean isDecimalFloat=  mtcs_mode_is_decimal_float_p(self,mode);
   const struct real_format **rf= ptx_get_real_format_for_mode();

   if(isDecimalFloat){
      int pos=mode - PTX_MIN_MODE_DECIMAL_FLOAT+PTX_NUM_MODE_FLOAT;
      return rf[pos];
   }else{
      if( mtcs_mode_get_class(self,mode)==MODE_FLOAT){
         return rf[mode-PTX_MIN_MODE_FLOAT];
      }else{
         gcc_unreachable ();
      }
   }
   return NULL;
}

//原型 nvptx.h
//#define CLZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE) \
//  ((VALUE) = GET_MODE_BITSIZE ((MODE)), 2)
//#define CTZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE) \
//  ((VALUE) = GET_MODE_BITSIZE ((MODE)), 2)
static int     clzDefinedValueAtZero_cb(MtcsMode *self,mtcs_mode mode,int *value)
{
    *value=mtcs_mode_get_bitsize(self,mode);
     return 2;
}
//原型#define CTZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
static int       ctzDefinedValueAtZero_cb(MtcsMode *self,mtcs_mode mode,int *value)
{
    *value=mtcs_mode_get_bitsize(self,mode);
    return 2;
}

//原型 REVERSIBLE_CC_MODE
static nushort getReversibleCCMode_cb(MtcsMode *mtcsMode,mtcs_mode mode)
{
    return 0;
}

//原型 STACK_SAVEAREA_MODE nvptx.h
static mtcs_mode  getStackSaveareaMode_cb(MtcsMode *self,enum save_level level)
{
    return TARGET_SOFT_STACK?(mtcs_mode)PTX_Pmode:(level == SAVE_FUNCTION ? (mtcs_mode)VOIDmode:(mtcs_mode)PTX_Pmode);
}

//原型 REGMODE_NATURAL_SIZE regs.h
static int getRegmodeNaturalSize_cb(MtcsMode *mtcsMode,mtcs_mode mode)
{
    return PTX_UNITS_PER_WORD;
}

//原型 init_adjust_machine_modes machmode.h insn-modes.cc
static void initAdjustMachineModes_cb(MtcsMode *mtcsMode)
{
   ptx_init_adjust_machine_modes();
}

//原型 LOAD_EXTEND_OP(M) defaults.h
static rtx_code loadExtendOp_cb(MtcsMode *mtcsMode,mtcs_mode m)
{
    return UNKNOWN;
}

//原型 targetm.c.mode_for_floating_type (TI_DOUBLE_TYPE); #define TARGET_C_MODE_FOR_FLOATING_TYPE default_mode_for_floating_type
static  machine_mode modeForFloatingType_cb(MtcsMode *self,enum tree_index ti)
{
   if (ti == TI_FLOAT_TYPE)
     return (machine_mode)PTX_SFmode;
   gcc_assert (ti == TI_DOUBLE_TYPE || ti == TI_LONG_DOUBLE_TYPE);
   return (machine_mode)PTX_DFmode;
}

MtcsPtxMode *mtcs_ptx_mode_new()
{
     MtcsPtxMode *self = n_slice_alloc0 (sizeof(MtcsPtxMode));
     mtcs_mode_init((MtcsMode*)self);
     mtcsPtxModeInit(self);
     return self;
}

