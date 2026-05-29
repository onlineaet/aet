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
#include "mtcsmode.h"
#include "mtcstarget.h"

typedef struct _MtcsModeBackup
{
   scalar_int_mode back_byte_mode;
   scalar_int_mode back_word_mode;
   scalar_int_mode back_ptr_mode;
}MtcsModeBackup;

static   void backup_cb(MtcsBackupRestore *iface);
static   void restore_cb(MtcsBackupRestore *iface);

void     mtcs_mode_init(MtcsMode *self)
{
    self->modeNameHashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);
    self->mtcsBackupRestore.backup=backup_cb;
    self->mtcsBackupRestore.restore=restore_cb;
    self->mtcsBackupRestore.impl=(npointer)self;
    self->backup=(void*) n_slice_alloc0 (sizeof(MtcsModeBackup));
    //原型 #define CASE_VECTOR_SHORTEN_MODE(MIN, MAX, BODY) 平台定义 nvptx无，无缺省
    self->case_vector_shorten_mode = NULL;
}

/**
 * machmode.h GET_MODE_BITSIZE
 */
unsigned short mtcs_mode_get_bitsize(MtcsMode *self,mtcs_mode mode)
{
    return self->modeSize[mode].coeffs[0]*BITS_PER_UNIT;
}

poly_uint16    mtcs_mode_get_bitsize_poly(MtcsMode *self,mtcs_mode mode)
{
    return self->modeSize[mode];
}

/* Return the alignment of MODE. This will be bounded by 1 and
   BIGGEST_ALIGNMENT.  */
/**
 * machmode.h GET_MODE_ALIGNMENT
 */
unsigned int mtcs_mode_get_alignment (MtcsMode *self,mtcs_mode mode)
{
    MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
    MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
    return MIN (mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign), MAX (1, self->modeBaseAlign[mode]*BITS_PER_UNIT));
}

unsigned short    mtcs_mode_get_precision(MtcsMode *self,mtcs_mode mode)
{
    return self->modePrecision[mode].coeffs[0];
}

poly_uint16     mtcs_mode_get_precision_poly(MtcsMode *self,mtcs_mode mode)
{
    return self->modePrecision[mode];
}

unsigned short mtcs_mode_get_size(MtcsMode *self,mtcs_mode mode)
{
    gcc_assert (mode >= 0 && mode < self->maxNumber);
    return self->modeSize[mode].coeffs[0];
}

poly_uint16    mtcs_mode_get_size_poly(MtcsMode *self,mtcs_mode mode)
{
    return self->modeSize[mode];
}

mtcs_mode mtcs_mode_get_inner(MtcsMode *self,mtcs_mode mode)
{
    return self->modeInner[mode];
}

/* Get the number of fractional bits of an object of mode MODE.  */
//原型 GET_MODE_FBIT machmode.h
//extern CONST_MODE_FBIT unsigned char mode_fbit[NUM_MACHINE_MODES];
//#define GET_MODE_FBIT(MODE) mode_fbit[MODE]
nuchar mtcs_mode_get_fbit(MtcsMode *self,mtcs_mode mode)
{
    return self->modeFBit[mode];
}

//原型 #define GET_MODE_IBIT(MODE) mode_ibit[MODE]
nuchar   mtcs_mode_get_ibit(MtcsMode *self,mtcs_mode mode)
{
   return self->modeIBit[mode];
}

void     mtcs_mode_set_ibit(MtcsMode *self ,nuchar *modeIBit)
{
   self->modeIBit=modeIBit;
}

//原型 GET_MODE_CLASS
unsigned char  mtcs_mode_get_class(MtcsMode *self,mtcs_mode mode)
{
    return self->modeClass[mode];
}

nuint   mtcs_mode_get_number(MtcsMode *self)
{
    return self->number;
}
void  mtcs_mode_set_number(MtcsMode *self,nuint number)
{
    self->number=number;
}
//原型 MAX_MACHINE_MODE insn-modes.h
int    mtcs_mode_get_max_number(MtcsMode *self)
{
    return self->maxNumber;

}
void   mtcs_mode_set_max_number(MtcsMode *self,int maxNumber)
{
     self->maxNumber=maxNumber;
}

mtcs_mode mtcs_mode_get_Pmode(MtcsMode *self)
{
   return self->get_Pmode(self);
}

const char    *mtcs_mode_get_name(MtcsMode *self,mtcs_mode mode)
{
    //printf("mtcs_mode_get_name --- %d %d\n",mode, self->maxNumber);
    gcc_assert (mode >= 0 && mode < self->maxNumber);
    return self->modeName[mode];
}



/**
 * 原型 toplev.cc do_compile()
 */
//void mtcs_mode_init_int(MtcsMode *self,int_n_data_t *intData,int mtcs_NUM_INT_N_ENTS)
//{
//    self->intData=intData;
//    self->mtcs_NUM_INT_N_ENTS=mtcs_NUM_INT_N_ENTS;
//    MtcsTarget *target=(MtcsTarget *)self->target;
//    self->int_n_enabled_p=xmalloc(sizeof(bool)*mtcs_NUM_INT_N_ENTS);
//    int i;
//    for (i = 0; i < mtcs_NUM_INT_N_ENTS; i ++)
//        if (target->scalar_mode_supported_p (target,(mtcs_mode)intData[i].m)  && !self->standard_type_bitsize (self,intData[i].bitsize))
//            self->int_n_enabled_p[i] = true;
//        else
//            self->int_n_enabled_p[i] = false;
//}

void mtcs_mode_init_int(MtcsMode *self)
{
    self->init_int(self);
}

/**
 * 返回属于mode_class的所有mode
 */
nuint  mtcs_mode_get_modes_by_class(MtcsMode *self,enum mode_class cl,nuint *modes,int len)
{
      // const unsigned char modeClass[PTX_NUM_MACHINE_MODES] =
       int i;
       int count=0;
       for(i=0;i<self->number;i++){
          if(self->modeClass[i]==cl){
              modes[count++]=(mtcs_mode)i;
          }
       }
       if(count>=len){
           n_error("modes溢出 %d %d",count,len);
       }
       return count;
}

const char    *mtcs_mode_get_type(MtcsMode *self,mtcs_mode mode, nboolean promote)
{
    return self->get_type(self,mode,promote);
}

/* Nonzero if MODE is a scalar/vector accum mode.  */
//原型
//#define ACCUM_MODE_P(MODE)      \
//  (GET_MODE_CLASS (MODE) == MODE_ACCUM  \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_ACCUM)
//machmode.h
nboolean mtcs_mode_is_accum_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char modeClass=mtcs_mode_get_class(self,mode);
    return (modeClass==MODE_ACCUM || modeClass==MODE_VECTOR_ACCUM);
}
/* Nonzero if MODE is a scalar/vector fract mode.  */
//原型
//#define FRACT_MODE_P(MODE)      \
//  (GET_MODE_CLASS (MODE) == MODE_FRACT  \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_FRACT)
//machmode.h
nboolean mtcs_mode_is_fract_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char modeClass=mtcs_mode_get_class(self,mode);
    return (modeClass==MODE_FRACT || modeClass==MODE_VECTOR_FRACT);
}
/* Nonzero if MODE is a scalar/vector fract or accum mode.  */
//原型
//#define SIGNED_FIXED_POINT_MODE_P(MODE)     \
//  (FRACT_MODE_P (MODE) || ACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_signed_fixed_point_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char modeClass=mtcs_mode_get_class(self,mode);
    return (mtcs_mode_is_fract_p(self,mode) || mtcs_mode_is_accum_p(self,mode));
}

/* Nonzero if MODE is a scalar/vector ufract mode.  */
//原型
//#define UFRACT_MODE_P(MODE)     \
//  (GET_MODE_CLASS (MODE) == MODE_UFRACT \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UFRACT)
//machmode.h
nboolean mtcs_mode_is_ufract_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char modeClass=mtcs_mode_get_class(self,mode);
    return (modeClass==MODE_UFRACT || modeClass==MODE_VECTOR_UFRACT);
}

/* Nonzero if MODE is a scalar/vector uaccum mode.  */
//原型
//#define UACCUM_MODE_P(MODE)     \
//  (GET_MODE_CLASS (MODE) == MODE_UACCUM \
//   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UACCUM)
//machmode.h
nboolean mtcs_mode_is_uaccum_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char modeClass=mtcs_mode_get_class(self,mode);
    return (modeClass==MODE_UACCUM || modeClass==MODE_VECTOR_UACCUM);
}

/* Nonzero if MODE is a scalar/vector ufract or uaccum mode.  */
//原型
//#define UNSIGNED_FIXED_POINT_MODE_P(MODE)   \
//  (UFRACT_MODE_P (MODE) || UACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_unsigned_fixed_point_p(MtcsMode *self,mtcs_mode mode)
{
    return (mtcs_mode_is_ufract_p(self,mode) || mtcs_mode_is_uaccum_p(self,mode));
}

/* Nonzero if MODE is a scalar/vector fract, ufract, accum or uaccum mode.  */
//原型
//#define ALL_FIXED_POINT_MODE_P(MODE)        \
//  (SIGNED_FIXED_POINT_MODE_P (MODE)     \
//   || UNSIGNED_FIXED_POINT_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_all_fixed_point_p(MtcsMode *self,mtcs_mode mode)
{
    return (mtcs_mode_is_signed_fixed_point_p(self,mode) || mtcs_mode_is_unsigned_fixed_point_p(self,mode));
}

//原型 #define SCALAR_FRACT_MODE_P(MODE)   (GET_MODE_CLASS (MODE) == MODE_FRACT) machmode.h
nboolean mtcs_mode_is_scalar_fract_p(MtcsMode *self,mtcs_mode mode)
{
   return mtcs_mode_get_class(self,mode)==MODE_FRACT;
}

//原型 #define SCALAR_UFRACT_MODE_P(MODE)    (GET_MODE_CLASS (MODE) == MODE_UFRACT)
nboolean mtcs_mode_is_scalar_ufract_p(MtcsMode *self,mtcs_mode mode)
{
   return mtcs_mode_get_class(self,mode)==MODE_UFRACT;
}

//原型 #define SCALAR_ACCUM_MODE_P(MODE)   (GET_MODE_CLASS (MODE) == MODE_ACCUM)
nboolean mtcs_mode_is_scalar_accum_p(MtcsMode *self,mtcs_mode mode)
{
   return mtcs_mode_get_class(self,mode)==MODE_ACCUM;
}

//原型 #define SCALAR_UACCUM_MODE_P(MODE)  (GET_MODE_CLASS (MODE) == MODE_UACCUM)
nboolean mtcs_mode_is_scalar_uaccum_p(MtcsMode *self,mtcs_mode mode)
{
   return mtcs_mode_get_class(self,mode)==MODE_UACCUM;
}

/**
 * 替换宏MINIMUM_ALIGNMENT
 */
unsigned int mtcs_mode_get_mininum_alignment(MtcsMode *self,tree exp,mtcs_mode mode,unsigned int align)
{
    return align;
}


/**
 * machine_mode从主机到设备
 */
mtcs_mode mtcs_mode_host2device(MtcsMode *self,machine_mode hostMode)
{
   const char *modeName=GET_MODE_NAME(hostMode);//这是主机的mode名字
   //查找在不在
   nboolean exists=n_hash_table_contains(self->modeNameHashTable,modeName);
   if(!exists){
      n_error("主机 machine_mode %d %s 在nptx中并不存在!",hostMode,modeName);
      return -1;
   }
   int pm=NPOINTER_TO_INT(n_hash_table_lookup(self->modeNameHashTable,modeName));
   return pm;
}

/**
 * machine_mode从主机到设备
 */
mtcs_mode mtcs_mode_host2device_by_tree(MtcsMode *self,tree declorType,machine_mode mode)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
   MtcsTraverseTree *mtcsTraverseTree=mtcs_target_get_traverse_tree(mtcsTarget);
   nboolean beReplaced=mtcs_traverse_tree_be_replaced(mtcsTraverseTree,declorType);
   if(beReplaced){
      return mode;
   }else{
      return mtcs_mode_host2device(self,mode);
   }
}

//原型 #define SCALAR_TYPE_MODE(NODE)  (as_a <scalar_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_mode mtcs_mode_host2device_scalar(MtcsMode *self,tree value)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
   MtcsTraverseTree *mtcsTraverseTree=mtcs_target_get_traverse_tree(mtcsTarget);
   nboolean beReplaced=mtcs_traverse_tree_be_replaced(mtcsTraverseTree,value);
   machine_mode mode=TYPE_CHECK (value)->type_common.mode;
   if(!beReplaced){
      mode= mtcs_mode_host2device(self,mode);
   }
   return mtcs_mode_as_a<scalar_mode>(self,mode);
}

scalar_mode mtcs_mode_scalar_type_mode(MtcsMode *self,tree value)
{
   machine_mode mode=TYPE_CHECK (value)->type_common.mode;
   return mtcs_mode_as_a<scalar_mode>(self,mode);
}

//原型 #define SCALAR_FLOAT_TYPE_MODE(NODE)  (as_a <scalar_float_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_float_mode mtcs_mode_host2device_scalar_float(MtcsMode *self,tree value)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
   MtcsTraverseTree *mtcsTraverseTree=mtcs_target_get_traverse_tree(mtcsTarget);
   nboolean beReplaced=mtcs_traverse_tree_be_replaced(mtcsTraverseTree,value);
   machine_mode mode=TYPE_CHECK (value)->type_common.mode;
   if(!beReplaced){
      mode= mtcs_mode_host2device(self,mode);
   }
   return mtcs_mode_as_a<scalar_float_mode>(self,mode);
}

//原型 #define SCALAR_FLOAT_TYPE_MODE(NODE)  (as_a <scalar_float_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_float_mode mtcs_mode_scalar_float_type_mode(MtcsMode *self,tree value)
{
   machine_mode mode=TYPE_CHECK (value)->type_common.mode;
   return mtcs_mode_as_a<scalar_float_mode>(self,mode);
}

//原型 #define SCALAR_INT_TYPE_MODE(NODE) (as_a <scalar_int_mode> (TYPE_CHECK (NODE)->type_common.mode))
scalar_int_mode mtcs_mode_host2device_scalar_int(MtcsMode *self,tree value)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
   MtcsTraverseTree *mtcsTraverseTree=mtcs_target_get_traverse_tree(mtcsTarget);
   nboolean beReplaced=mtcs_traverse_tree_be_replaced(mtcsTraverseTree,value);
   machine_mode mode=TYPE_CHECK (value)->type_common.mode;
   if(!beReplaced){
      mode= mtcs_mode_host2device(self,mode);
   }
   return mtcs_mode_as_a<scalar_int_mode>(self,mode);
}

scalar_int_mode mtcs_mode_scalar_int_type_mode(MtcsMode *self,tree value)
{
   machine_mode mode=TYPE_CHECK (value)->type_common.mode;
   return mtcs_mode_as_a<scalar_int_mode>(self,mode);
}


/**
 * machine_mode从设备到主机
 */
machine_mode mtcs_mode_device2host(MtcsMode *self,mtcs_mode deviceMode)
{
   const char *modeName=mtcs_mode_get_name(self,deviceMode);//这是设备的mode名字
   //查找在不在
   int i;
   for(i=0;i<NUM_MACHINE_MODES;i++){
      if(!strcmp(GET_MODE_NAME((machine_mode)i),modeName)){
         return (machine_mode)i;
      }
   }
   n_error("严重错误 mtcs_mode");
   return Pmode;
}

/* Nonzero if MODE is a scalar integral mode.  */
//#define SCALAR_INT_MODE_P(MODE)         \
//  (GET_MODE_CLASS (MODE) == MODE_INT        \
//   || GET_MODE_CLASS (MODE) == MODE_PARTIAL_INT)
//原型是 SCALAR_INT_MODE_P
nboolean mtcs_mode_is_int(MtcsMode *self,mtcs_mode mode)
{
    return mtcs_mode_get_class(self,mode)==MODE_INT || mtcs_mode_get_class(self,mode)==MODE_PARTIAL_INT;
}
/* Truncate and perhaps sign-extend C as appropriate for MODE.  */
//原型  trunc_int_for_mode rtl.h explow.cc
HOST_WIDE_INT mtcs_mode_trunc_int_for_mode (MtcsMode *self,HOST_WIDE_INT c, mtcs_mode mode)
{
  /* Not scalar_int_mode because we also allow pointer bound modes.  */
  scalar_mode smode =mtcs_mode_as_a/*!as_a*/<scalar_mode> (self,(machine_mode)mode);
  int width = mtcs_mode_get_precision(self,(mtcs_mode)smode);/*GET_MODE_PRECISION (smode);*/
  /* You want to truncate to a _what_?  */
  gcc_assert (mtcs_mode_is_int(self,mode)/*SCALAR_INT_MODE_P (mode)*/);
  n_debug("mtcsmode.c mtcs_mode_trunc_int_for_mode 00 c:"HOST_WIDE_INT_PRINT_DEC" mode:%d width:%d HOST_BITS_PER_WIDE_INT:%d\n",
        c,mode,width,HOST_BITS_PER_WIDE_INT);
  /* Canonicalize BImode to 0 and STORE_FLAG_VALUE.  */
  if (smode == self->modes.M_BImode)
    return c & 1 ? STORE_FLAG_VALUE : 0;
  n_debug("mtcsmode.c mtcs_mode_trunc_int_for_mode 11 c:"HOST_WIDE_INT_PRINT_DEC" mode:%d width:%d HOST_BITS_PER_WIDE_INT:%d\n",
        c,mode,width,HOST_BITS_PER_WIDE_INT);
  /* Sign-extend for the requested mode.  */

  if (width < HOST_BITS_PER_WIDE_INT){
      HOST_WIDE_INT sign = 1;
      sign <<= width - 1;
      c &= (sign << 1) - 1;
      c ^= sign;
      c -= sign;
      n_debug("mtcsmode.c mtcs_mode_trunc_int_for_mode 22 c:"HOST_WIDE_INT_PRINT_DEC" mode:%d width:%d HOST_BITS_PER_WIDE_INT:%d\n",
            c,mode,width,HOST_BITS_PER_WIDE_INT);
  }

  return c;
}

poly_int64 mtcs_mode_trunc_int_for_mode_with_poly_int64 (MtcsMode *self,poly_int64 x, mtcs_mode mode)
{
   for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
      x.coeffs[i] = mtcs_mode_trunc_int_for_mode (self,x.coeffs[i], mode);
   return x;
}

/**
 * 重载 mtcs_mode_trunc_int_for_mode
 */
poly_int64     mtcs_mode_trunc_int_for_mode (MtcsMode *self,poly_int64 x, mtcs_mode mode)
{
   for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
      x.coeffs[i] = mtcs_mode_trunc_int_for_mode (self,x.coeffs[i], mode);
   return x;
}


/* Return the mode to use to pass or return a scalar of TYPE and MODE.
   PUNSIGNEDP points to the signedness of the type and may be adjusted
   to show what signedness to use on extension operations.

   FOR_RETURN is nonzero if the caller is promoting the return value
   of FNDECL, else it is for promoting args.  */
//原型 promote_function_mode explow.h explow.cc
mtcs_mode  mtcs_mode_promote_function_mode (MtcsMode *self,const_tree type, mtcs_mode mode, int *punsignedp,
               const_tree funtype, int for_return)
{
   /* Called without a type node for a libcall.  */
   MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   if (type == NULL_TREE){
      if (mtcs_mode_is_integral_p(self,mode))
         return target_calls_promote_function_mode/*!targetm.calls.promote_function_mode*/(mtcsMachine->calls,
               NULL_TREE, mode,punsignedp, funtype,for_return);
      else
         return mode;
   }

   switch (TREE_CODE (type)){
      case INTEGER_TYPE:   case ENUMERAL_TYPE:   case BOOLEAN_TYPE:
      case REAL_TYPE:      case OFFSET_TYPE:     case FIXED_POINT_TYPE:
      case POINTER_TYPE:   case REFERENCE_TYPE:
         return target_calls_promote_function_mode/*!targetm.calls.promote_function_mode*/(mtcsMachine->calls,
                     type, mode, punsignedp, funtype,for_return);

      default:
         return mode;
   }
}
/* Return the mode to use to store a scalar of TYPE and MODE.
   PUNSIGNEDP points to the signedness of the type and may be adjusted
   to show what signedness to use on extension operations.
   原型 explow.cc promote_mode
    */

/*
#define PROMOTE_MODE(MODE, UNSIGNEDP, TYPE)     \
  if ((MODE) == QImode || (MODE) == HImode)     \
    {                           \
      (MODE) = SImode;                  \
      (void)(UNSIGNEDP);                \//void仅仅是去掉编译器对没有使用的局部变量的警告。
      (void)(TYPE);                 \//void仅仅是去掉编译器对没有使用的局部变量的警告。
    }
*/
mtcs_mode mtcs_mode_promote_mode (MtcsMode *self,const_tree type ATTRIBUTE_UNUSED, mtcs_mode mode,int *punsignedp ATTRIBUTE_UNUSED)
{
//#ifdef PROMOTE_MODE
  enum tree_code code;
  int unsignedp;
  scalar_mode smode;
//#endif

  /* For libcalls this is invoked without TYPE from the backends
     TARGET_PROMOTE_FUNCTION_MODE hooks.  Don't do anything in that
     case.  */
  if (type == NULL_TREE)
    return mode;

  /* FIXME: this is the same logic that was there until GCC 4.4, but we
     probably want to test POINTERS_EXTEND_UNSIGNED even if PROMOTE_MODE
     is not defined.  The affected targets are M32C, S390, SPARC.  */
//#ifdef PROMOTE_MODE
  code = TREE_CODE (type);
  unsignedp = *punsignedp;

  switch (code)
    {
    case INTEGER_TYPE:   case ENUMERAL_TYPE:   case BOOLEAN_TYPE:
    case REAL_TYPE:      case OFFSET_TYPE:     case FIXED_POINT_TYPE:
      /* Values of these types always have scalar mode.  */
      smode = mtcs_mode_as_a <scalar_mode> (self,(machine_mode)mode);
      self->promote_inner/*PROMOTE_MODE*/ (self,(mtcs_mode)smode, unsignedp, type);
      *punsignedp = unsignedp;
      return smode;

      //POINTERS_EXTEND_UNSIGNED host=1 nvptx=0 gen没有定义
//#ifdef POINTERS_EXTEND_UNSIGNED
//    case REFERENCE_TYPE:
//    case POINTER_TYPE:
//      *punsignedp = POINTERS_EXTEND_UNSIGNED;
//      return targetm.addr_space.address_mode
//           (TYPE_ADDR_SPACE (TREE_TYPE (type)));
//#endif

    default:
      return mode;
    }
//#else
  //return mode;
//#endif
}


/* Use one of promote_mode or promote_function_mode to find the promoted
   mode of DECL.  If PUNSIGNEDP is not NULL, store there the unsignedness
   of DECL after promotion.  */

mtcs_mode mtcs_mode_promote_decl_mode (MtcsMode *self,const_tree decl, int *punsignedp)
{
   tree type = TREE_TYPE (decl);
   int unsignedp = TYPE_UNSIGNED (type);
   //machine_mode mode = DECL_MODE (decl);
   machine_mode mmode = DECL_MODE (decl);
   mtcs_mode mode=mtcs_mode_host2device_by_tree(self,decl,mmode);
   n_debug("mtcsmode.c mtcs_mode_promote_decl_mode 00 mmode:%d %d\n",mmode,mode);
   mtcs_mode pmode;

   if (TREE_CODE (decl) == RESULT_DECL && !DECL_BY_REFERENCE (decl))
      pmode = mtcs_mode_promote_function_mode (self,type, mode, &unsignedp,TREE_TYPE (current_function_decl), 1);
   else if (TREE_CODE (decl) == RESULT_DECL || TREE_CODE (decl) == PARM_DECL)
      pmode = mtcs_mode_promote_function_mode (self,type, mode, &unsignedp,TREE_TYPE (current_function_decl), 2);
   else
      pmode = mtcs_mode_promote_mode (self,type, mode, &unsignedp);

   if (punsignedp)
      *punsignedp = unsignedp;
   n_debug("mtcsmode.c mtcs_mode_promote_decl_mode 11 mmode:%d %d pmode:%d unsignedp:%d\n",mmode,mode,pmode,unsignedp);

   return pmode;
}

/* Return the promoted mode for name.  If it is a named SSA_NAME, it
   is the same as promote_decl_mode.  Otherwise, it is the promoted
   mode of a temp decl of same type as the SSA_NAME, if we had created
   one.  */

mtcs_mode mtcs_mode_promote_ssa_mode (MtcsMode *self,const_tree name, int *punsignedp)
{
   gcc_assert (TREE_CODE (name) == SSA_NAME);

   /* Partitions holding parms and results must be promoted as expected
   by function.cc.  */
   if (SSA_NAME_VAR (name) && (TREE_CODE (SSA_NAME_VAR (name)) == PARM_DECL || TREE_CODE (SSA_NAME_VAR (name)) == RESULT_DECL)){
      mtcs_mode mode = mtcs_mode_promote_decl_mode (self,SSA_NAME_VAR (name), punsignedp);
      n_debug("mtcsmode.cmtcs_mode_promote_ssa_mode 00 mode:%d %p blkmode:%d\n",mode,SSA_NAME_VAR (name),self->modes.M_BLKmode);
      //aet_print_tree(SSA_NAME_VAR (name));
      if (mode !=self->modes.M_BLKmode)
         return mode;
   }

   tree type = TREE_TYPE (name);
   n_debug("mtcsmode.c mtcs_mode_promote_ssa_mode 11 mode:%d type:%s\n",TYPE_MODE (type),get_tree_code_name(TREE_CODE(type)));
   //aet_print_tree(type);
   int unsignedp = TYPE_UNSIGNED (type);
   machine_mode typeMode=TYPE_MODE (type);//转成mtcs_mode
   mtcs_mode mode=typeMode;//mtcs_mode_host2device(self,typeMode);
   mtcs_mode pmode = mtcs_mode_promote_mode (self,type, mode, &unsignedp);
   if (punsignedp)
      *punsignedp = unsignedp;
   return pmode;
}

/*
 * machine_mode.h
#define INTEGRAL_MODE_P(MODE)           \
  (GET_MODE_CLASS (MODE) == MODE_INT        \
   || GET_MODE_CLASS (MODE) == MODE_PARTIAL_INT \
   || GET_MODE_CLASS (MODE) == MODE_COMPLEX_INT \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_BOOL \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_INT)
*/
//原型 #define INTEGRAL_MODE_P(MODE)   machmode.h
nboolean mtcs_mode_is_integral_p(MtcsMode *self,mtcs_mode mode)
{
   unsigned char cl= mtcs_mode_get_class(self,mode);
   return (cl==MODE_INT ||  cl==MODE_PARTIAL_INT ||  cl==MODE_COMPLEX_INT ||  cl==MODE_VECTOR_BOOL ||  cl==MODE_VECTOR_INT);
}

/* Nonzero if MODE is a scalar floating point mode.  */
#define SCALAR_FLOAT_MODE_P(MODE)       \
  (GET_MODE_CLASS (MODE) == MODE_FLOAT      \
   || GET_MODE_CLASS (MODE) == MODE_DECIMAL_FLOAT)

//原型 FLOAT_MODE_P
nboolean mtcs_mode_is_float_p(MtcsMode *self,mtcs_mode mode)
{
   unsigned char cl= mtcs_mode_get_class(self,mode);
   return (cl==MODE_FLOAT ||  cl==MODE_DECIMAL_FLOAT);
}

/* Nonzero if MODE is a complex mode.  */
//原型 #define COMPLEX_MODE_P(MODE)            \
//  (GET_MODE_CLASS (MODE) == MODE_COMPLEX_INT    \
//   || GET_MODE_CLASS (MODE) == MODE_COMPLEX_FLOAT)
nboolean  mtcs_mode_is_complex_p(MtcsMode *self,mtcs_mode mode)
{
   unsigned char cl= mtcs_mode_get_class(self,mode);
   return (cl==MODE_COMPLEX_INT ||  cl==MODE_COMPLEX_FLOAT);
}

/* Return an integer mode of exactly the same size as MODE, if one exists.  */
//原型 int_mode_for_mode stor-layout.cc
opt_scalar_int_mode mtcs_mode_int_mode_for_mode (MtcsMode *self,mtcs_mode mode)
{
   switch (mtcs_mode_get_class (self,mode)){
      case MODE_INT:
      case MODE_PARTIAL_INT:
         return mtcs_mode_as_a <scalar_int_mode> (self,(machine_mode)mode);

      case MODE_COMPLEX_INT:
      case MODE_COMPLEX_FLOAT:
      case MODE_FLOAT:
      case MODE_DECIMAL_FLOAT:
      case MODE_FRACT:
      case MODE_ACCUM:
      case MODE_UFRACT:
      case MODE_UACCUM:
      case MODE_VECTOR_BOOL:
      case MODE_VECTOR_INT:
      case MODE_VECTOR_FLOAT:
      case MODE_VECTOR_FRACT:
      case MODE_VECTOR_ACCUM:
      case MODE_VECTOR_UFRACT:
      case MODE_VECTOR_UACCUM:
         return mtcs_mode_int_mode_for_size/*int_mode_for_size*/ (self,mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/ (self,mode), 0);

      case MODE_OPAQUE:
         return opt_scalar_int_mode ();

      case MODE_RANDOM:
         if (mode ==self->modes.M_BLKmode)
            return opt_scalar_int_mode ();

      /* fall through */
      case MODE_CC:
      default:
         gcc_unreachable ();
   }
}

//原型#define MAX_FIXED_MODE_SIZE GET_MODE_BITSIZE (DImode)
nuint mtcs_mode_get_max_fixed_size(MtcsMode *self)
{
    return mtcs_mode_get_bitsize(self,self->modes.M_DImode);
}

/* Return a machine mode of class MCLASS with SIZE bits of precision,
   if one exists.  The mode may have padding bits as well the SIZE
   value bits.  If LIMIT is nonzero, disregard modes wider than
   MAX_FIXED_MODE_SIZE.  */
//原型 mode_for_size stor-layout.cc
opt_machine_mode mtcs_mode_mode_for_size (MtcsMode *self,poly_uint64 size, enum mode_class mclass, int limit)
{
   machine_mode mode;
   int i;
   if (limit && maybe_gt (size, mtcs_mode_get_max_fixed_size/*MAX_FIXED_MODE_SIZE*/(self)))
      return opt_machine_mode ();
   /* Get the first mode which has this size, in the specified class.  */
   MTCS_FOR_EACH_MODE_IN_CLASS (self,mode, mclass){
      if (known_eq (mtcs_mode_get_precision/*GET_MODE_PRECISION*/ (self,mode), size)){
         return mode;
      }
   }
   if (mclass == MODE_INT || mclass == MODE_PARTIAL_INT)
      for (i = 0; i < self->mtcs_NUM_INT_N_ENTS/*NUM_INT_N_ENTS*/; i ++)
         if (known_eq (self->intData/*int_n_data*/[i].bitsize, size) && self->int_n_enabled_p/*int_n_enabled_p*/[i])
            return self->intData/*int_n_data*/[i].m;
   return opt_machine_mode ();
}

//原型 machmode.h int_mode_for_size
opt_scalar_int_mode mtcs_mode_int_mode_for_size (MtcsMode *self,poly_uint64 size, int limit)
{
   return mtcs_mode_dyn_cast <scalar_int_mode> (self,mtcs_mode_mode_for_size (self,size, MODE_INT, limit));
}


/* Nonzero if MODE is a vector mode.  */
//原型 #define VECTOR_MODE_P(MODE)             \
  (GET_MODE_CLASS (MODE) == MODE_VECTOR_BOOL        \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_INT      \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_FLOAT    \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_FRACT    \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UFRACT   \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_ACCUM    \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UACCUM)
nboolean mtcs_mode_is_vector_p(MtcsMode *self,mtcs_mode mode)
{
   unsigned char cl=mtcs_mode_get_class(self,mode);
   return (cl==MODE_VECTOR_BOOL || cl==MODE_VECTOR_INT || cl==MODE_VECTOR_FLOAT || cl==MODE_VECTOR_FRACT ||
         cl==MODE_VECTOR_UFRACT || cl==MODE_VECTOR_ACCUM || cl==MODE_VECTOR_UACCUM);
}

//原型 machmode.h mode_to_nunits
poly_uint16 mtcs_mode_get_nunits (MtcsMode *self,mtcs_mode  mode)
{
   //return self->get_nunits(self,mode);//(__builtin_constant_p (mode)? mode_nunits_inline (mode) : mode_nunits[mode]);
   return self->modeNunits[mode];
}

//原型 #define GET_MODE_UNIT_SIZE(MODE) mode_to_unit_size (MODE)
unsigned char  mtcs_mode_get_unit_size(MtcsMode *self,mtcs_mode mode)
{
   return self->modeUnitSize[mode];
}

//原型 paradoxical_subreg_p rtl.h
nboolean mtcs_mode_paradoxical_subreg_p (MtcsMode *self,mtcs_mode outermode, mtcs_mode innermode)
{
   /* Modes involved in a subreg must be ordered.  In particular, we must
   always know at compile time whether the subreg is paradoxical.  */
   poly_int64 outer_prec = mtcs_mode_get_precision (self,outermode);
   poly_int64 inner_prec = mtcs_mode_get_precision (self,innermode);
   gcc_checking_assert (ordered_p (outer_prec, inner_prec));
   return maybe_lt (outer_prec, inner_prec);
}

/* Return the SUBREG_BYTE for an OUTERMODE lowpart of an INNERMODE value.  */
//原型 subreg_lowpart_offset rtl.h
poly_uint64 mtcs_mode_subreg_lowpart_offset (MtcsMode *self,machine_mode outermode, machine_mode innermode)
{
   //subreg_size_lowpart_offset emit-rtl.cc rtl.h
   return subreg_size_lowpart_offset (mtcs_mode_get_size(self,outermode),
         mtcs_mode_get_size (self,innermode));
}

//原型 byte_lowpart_offset rtl.h emit-rtl.cc
poly_int64 mtcs_mode_byte_lowpart_offset (MtcsMode *self,machine_mode outer_mode, machine_mode inner_mode)
{
   if (mtcs_mode_paradoxical_subreg_p (self,outer_mode, inner_mode))
      return -mtcs_mode_subreg_lowpart_offset (self,inner_mode, outer_mode);
   else
      return mtcs_mode_subreg_lowpart_offset (self,outer_mode, inner_mode);
}

//原型 subreg_highpart_offset rtl.h
poly_uint64 mtcs_mode_subreg_highpart_offset (MtcsMode *self,machine_mode outermode, machine_mode innermode)
{
  return subreg_size_highpart_offset (mtcs_mode_get_size(self,outermode),
          mtcs_mode_get_size (self,innermode));
}

//原型 SCALAR_FLOAT_MODE_P machmode.h
nboolean mtcs_mode_is_scalar_float_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char cl=mtcs_mode_get_class(self,mode);
    return (cl==MODE_FLOAT || cl==MODE_DECIMAL_FLOAT);
}

//原型SCALAR_INT_MODE_P machmode.h
nboolean  mtcs_mode_is_scalar_int_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char cl=mtcs_mode_get_class(self,mode);
    return (cl==MODE_INT || cl==MODE_PARTIAL_INT);
}
//原型 GET_MODE_MASK machmode.h
unsigned HOST_WIDE_INT mtcs_mode_get_mask(MtcsMode *self,mtcs_mode mode)
{
    return self->get_mask(self,mode);
}

nushort mtcs_mode_get_unit_precision(MtcsMode *self,mtcs_mode mode)
{
    return self->modeUnitPrecision[mode];
}

struct real_format *mtcs_mode_get_real_format(MtcsMode *self,mtcs_mode mode)
{
    return self->get_real_format(self,mode);
}


//#define REAL_MODE_FORMAT(MODE)                      \
//  (real_format_for_mode[DECIMAL_FLOAT_MODE_P (MODE)         \
//            ? (((MODE) - MIN_MODE_DECIMAL_FLOAT)        \
//               + NUM_MODE_FLOAT)                \
//            : GET_MODE_CLASS (MODE) == MODE_FLOAT       \
//            ? ((MODE) - MIN_MODE_FLOAT)         \
//            : (gcc_unreachable (), 0)])
//
//#define FLOAT_MODE_FORMAT(MODE) \
//  (REAL_MODE_FORMAT (as_a <scalar_float_mode> (GET_MODE_INNER (MODE))))
//
///* The following macro determines whether the floating point format is
//   composite, i.e. may contain non-consecutive mantissa bits, in which
//   case compile-time FP overflow may not model run-time overflow.  */
//#define MODE_COMPOSITE_P(MODE) \
//  (FLOAT_MODE_P (MODE) \
//   && FLOAT_MODE_FORMAT (MODE)->pnan < FLOAT_MODE_FORMAT (MODE)->p)

//原型 MODE_COMPOSITE_P real.h
nboolean  mtcs_mode_is_composite_p(MtcsMode *self,mtcs_mode mode)
{
    nboolean isFloatP=mtcs_mode_is_float_p(self,mode);
    if(isFloatP){
        scalar_float_mode fm=mtcs_mode_as_a <scalar_float_mode> (self,(machine_mode)mtcs_mode_get_inner(self,mode));
        struct real_format *rf=mtcs_mode_get_real_format(self,fm);
        return rf->pnan<rf->p;
    }
    return FALSE;
}

nboolean       mtcs_mode_is_decimal_float_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char cl=mtcs_mode_get_class(self,mode);
    return cl==MODE_DECIMAL_FLOAT;
}

//原型 real.h MODE_HAS_INFINITIES
//#define MODE_HAS_INFINITIES(MODE) \
//  (FLOAT_MODE_P (MODE) && FLOAT_MODE_FORMAT (MODE)->has_inf)

nboolean       mtcs_mode_has_infinities(MtcsMode *self,mtcs_mode mode)
{
    nboolean isFloatP=mtcs_mode_is_float_p(self,mode);
    if(isFloatP){
      scalar_float_mode fm=mtcs_mode_as_a <scalar_float_mode> (self,(machine_mode)mtcs_mode_get_inner(self,mode));
      struct real_format *rf=mtcs_mode_get_real_format(self,fm);
      return rf->has_inf;
    }
    return FALSE;
}


//原型 real.h MODE_HAS_NANS
//#define MODE_HAS_NANS(MODE) \
//  (FLOAT_MODE_P (MODE) && FLOAT_MODE_FORMAT (MODE)->has_nans)
nboolean       mtcs_mode_has_nans(MtcsMode *self,mtcs_mode mode)
{
    nboolean isFloatP=mtcs_mode_is_float_p(self,mode);
    if(isFloatP){
      scalar_float_mode fm=mtcs_mode_as_a <scalar_float_mode> (self,(machine_mode)mtcs_mode_get_inner(self,mode));
      struct real_format *rf=mtcs_mode_get_real_format(self,fm);
      return rf->has_nans;
    }
    return FALSE;
}

//原型 MODE_HAS_SIGNED_ZEROS real.h
nboolean  mtcs_mode_has_signed_zeros(MtcsMode *self,mtcs_mode mode)
{
    nboolean isFloatP=mtcs_mode_is_float_p(self,mode);
    if(isFloatP){
      scalar_float_mode fm=mtcs_mode_as_a <scalar_float_mode> (self,(machine_mode)mtcs_mode_get_inner(self,mode));
      struct real_format *rf=mtcs_mode_get_real_format(self,fm);
      return rf->has_signed_zero;
    }
    return FALSE;
}

//原型 HONOR_NANS real.h real.cc
nboolean       mtcs_mode_honor_nans(MtcsMode *self,mtcs_mode mode)
{
    return mtcs_mode_has_nans(self,mode) && !flag_finite_math_only;
}

//原型 extern bool HONOR_NANS (const_tree); real.h real.cc
nboolean  mtcs_mode_honor_nans(MtcsMode *self,const_tree t)
{
   return mtcs_mode_honor_nans(self,(mtcs_mode)element_mode (t));
}
//原型 extern bool HONOR_NANS (const_rtx); real.h real.cc
nboolean  mtcs_mode_honor_nans(MtcsMode *self,const_rtx rtx)
{
   return mtcs_mode_honor_nans(self,(mtcs_mode)GET_MODE(rtx));
}
//原型 HONOR_SNANS real.h real.cc
nboolean  mtcs_mode_honor_snans(MtcsMode *self,mtcs_mode mode)
{
    return flag_signaling_nans && mtcs_mode_honor_nans (self,mode);
}

//原型 HONOR_SNANS real.h real.cc
nboolean  mtcs_mode_honor_snans(MtcsMode *self,const_tree t)
{
   return mtcs_mode_honor_snans(self,(mtcs_mode)element_mode (t));
}
//原型 HONOR_SNANS real.h real.cc
nboolean  mtcs_mode_honor_snans(MtcsMode *self,const_rtx rtx)
{
   return mtcs_mode_honor_snans(self,(mtcs_mode)GET_MODE(rtx));
}

/* As for HONOR_NANS, but true if the mode can represent infinity and
   the treatment of infinite values is important.  */
//原型 HONOR_INFINITIES (machine_mode m) real.h real.cc
nboolean mtcs_mode_honor_infinities (MtcsMode *self, mtcs_mode m)
{
  return mtcs_mode_has_infinities/*!MODE_HAS_INFINITIES*/(self,m) && !flag_finite_math_only;
}

//原型 HONOR_INFINITIES (const_tree t) real.h real.cc
nboolean mtcs_mode_honor_infinities (MtcsMode *self, const_tree t)
{
   return mtcs_mode_honor_infinities(self,(mtcs_mode)element_mode(t));

}
//原型 HONOR_INFINITIES (const_rtx rtx) real.h real.cc
nboolean mtcs_mode_honor_infinities (MtcsMode *self, const_rtx rtx)
{
   return mtcs_mode_honor_infinities(self,(mtcs_mode)GET_MODE(rtx));
}

//原型 HONOR_SIGNED_ZEROS real.h real.cc
nboolean       mtcs_mode_honor_signed_zeros(MtcsMode *self,mtcs_mode mode)
{
    return mtcs_mode_has_signed_zeros(self,mode) && flag_signed_zeros;
}

//原型 HONOR_SIGNED_ZEROS real.h real.cc
nboolean  mtcs_mode_honor_signed_zeros(MtcsMode *self,const_tree t)
{
   return mtcs_mode_honor_signed_zeros(self,(mtcs_mode)element_mode(t));
}

//原型 HONOR_SIGNED_ZEROS real.h real.cc
nboolean  mtcs_mode_honor_signed_zeros(MtcsMode *self,const_rtx rtx)
{
   return mtcs_mode_honor_signed_zeros(self,(mtcs_mode)GET_MODE(rtx));
}

//原型  MODE_HAS_SIGN_DEPENDENT_ROUNDING real.h
//#define MODE_HAS_SIGN_DEPENDENT_ROUNDING(MODE) \
//  (FLOAT_MODE_P (MODE) \
//   && FLOAT_MODE_FORMAT (MODE)->has_sign_dependent_rounding)
nboolean  mtcs_mode_has_sign_dependent_rounding(MtcsMode *self,mtcs_mode mode)
{
   nboolean isFloatP=mtcs_mode_is_float_p(self,mode);
   if(isFloatP){
     scalar_float_mode fm=mtcs_mode_as_a <scalar_float_mode> (self,(machine_mode)mtcs_mode_get_inner(self,mode));
     struct real_format *rf=mtcs_mode_get_real_format(self,fm);
     return rf->has_sign_dependent_rounding;
   }
   return FALSE;
}

//原型 HONOR_SIGN_DEPENDENT_ROUNDING real.h
nboolean  mtcs_mode_honor_sign_dependent_rounding(MtcsMode *self,mtcs_mode mode)
{
    MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    return mtcs_mode_has_sign_dependent_rounding (self,mode) && mtcsOptionsItem->x_flag_rounding_math;
}

nboolean  mtcs_mode_honor_sign_dependent_rounding(MtcsMode *self,const_tree t)
{
  return mtcs_mode_honor_sign_dependent_rounding(self,(mtcs_mode)element_mode(t));
}

nboolean  mtcs_mode_honor_sign_dependent_rounding(MtcsMode *self,const_rtx x)
{
   return mtcs_mode_honor_sign_dependent_rounding (self,(mtcs_mode)GET_MODE (x));
}

int mtcs_mode_get_max_bitsize_mode_any_int(MtcsMode *self)
{
    return self->max_bitsize_mode_any_int;
}

void  mtcs_mode_set_max_bitsize_mode_any_int(MtcsMode *self,int max)
{
    self->max_bitsize_mode_any_int=max;
}

//原型#define CLZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
int       mtcs_mode_clz_defined_value_at_zero(MtcsMode *self,mtcs_mode mode,int *value)
{
    return self->clz_defined_value_at_zero(self,mode,value);
}
//原型#define CTZ_DEFINED_VALUE_AT_ZERO(MODE, VALUE)
int       mtcs_mode_ctz_defined_value_at_zero(MtcsMode *self,mtcs_mode mode,int *value)
{
    return self->ctz_defined_value_at_zero(self,mode,value);
}

//原型 HWI_COMPUTABLE_MODE_P machmode.h
nboolean mtcs_mode_is_hwi_computable_p(MtcsMode *self,machine_mode mode)
{
  machine_mode mme = mode;
  return (mtcs_mode_is_scalar_int_p (self,mme)
      && mtcs_mode_get_precision (self,mme) <= HOST_BITS_PER_WIDE_INT);
}

//原型 #define GET_MODE_UNIT_BITSIZE(MODE) \
//  ((unsigned short) (GET_MODE_UNIT_SIZE (MODE) * BITS_PER_UNIT)) machmode.h
unsigned short   mtcs_mode_get_unit_bitsize(MtcsMode *self,mtcs_mode mode)
{
    return (mtcs_mode_get_unit_size(self,mode)* BITS_PER_UNIT);
}

//原型 related_vector_mode machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_related_vector_mode (MtcsMode *self,machine_mode vector_mode, scalar_mode element_mode,
        poly_uint64 nunits)
{
      gcc_assert (mtcs_mode_is_vector_p(self,vector_mode));
      MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
      MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

      return target_vectorize_related_mode/*targetm.vectorize.related_mode*/(mtcsMachine->vectorize,vector_mode, element_mode, nunits);


}

//原型 init_derived_machine_modes rtl.h emit-rtl.cc
void mtcs_mode_init_derived_machine_modes (MtcsMode *self)
{
    /*
    opt_scalar_int_mode  opt_byte_mode, opt_word_mode;
    int i;
    int count=0;

    for(i=0;i<self->number;i++){
        mtcs_mode mode=(mtcs_mode)i;
        if(mtcs_mode_get_class(self,mode)==MODE_INT){
            fprintf(stderr,"init_derived_machine_modes 00 count:%d mode:%d name:%s  opt_byte_mode.exists:%d opt_word_mode.exist:%d bitSize:%d BITS_PER_WORD:%d BITS_PER_WORD:%d\n",
                         count,mode,mtcs_mode_get_name(self,mode),
                         opt_byte_mode.exists (),opt_word_mode.exists (),mtcs_mode_get_bitsize (self,mode),BITS_PER_UNIT,BITS_PER_WORD);
            if(mtcs_mode_get_bitsize(self,mode) == BITS_PER_UNIT && opt_byte_mode.exists ()){
                opt_byte_mode=scalar_int_mode::from_int (mode);
                fprintf(stderr,"init_derived_machine_modes 11 count:%d :%d %s BITS_PER_UNIT\n",count,mode,mtcs_mode_get_name(self,mode));

            }
            if(mtcs_mode_get_bitsize(self,mode) == BITS_PER_WORD && opt_word_mode.exists ()){
                opt_word_mode=scalar_int_mode::from_int (mode);
                fprintf(stderr,"init_derived_machine_modes 22 count:%d :%d %s \n",count,mode,mtcs_mode_get_name(self,mode));
            }
            count++;
        }
    }
    */


    opt_scalar_int_mode mode_iter, opt_byte_mode, opt_word_mode;
     int count=0;
     MTCS_FOR_EACH_MODE_IN_CLASS (self,mode_iter, MODE_INT){
         scalar_int_mode mode = mode_iter.require ();
         n_debug("mtcsmode.c init_derived_machine_modes 00 count:%d mode:%d name:%s  opt_byte_mode.exists:%d opt_word_mode.exist:%d bitSize:%d BITS_PER_WORD:%d BITS_PER_WORD:%d\n",
                 count,mode,mtcs_mode_get_name(self,mode),
                 opt_byte_mode.exists (),opt_word_mode.exists (),mtcs_mode_get_bitsize(self,mode),BITS_PER_UNIT,BITS_PER_WORD);

         if (mtcs_mode_get_bitsize (self,mode) == BITS_PER_UNIT && !opt_byte_mode.exists ()){
           opt_byte_mode = mode;
             n_debug("mtcsmode.c  init_derived_machine_modes 11 count:%d :%d %s BITS_PER_UNIT\n",count,mode,mtcs_mode_get_name(self,mode));

         }
         if (mtcs_mode_get_bitsize (self,mode) == BITS_PER_WORD  && !opt_word_mode.exists ()){
           opt_word_mode = mode;
           n_debug("mtcsmode.c  init_derived_machine_modes 22 count:%d :%d %s BITS_PER_WORD\n",count,mode,mtcs_mode_get_name(self,mode));

         }
         count++;
    }


    self->byte_mode = opt_byte_mode.require ();
    self->word_mode = opt_word_mode.require ();
    self->ptr_mode = mtcs_mode_as_a <scalar_int_mode>
      (self,(mtcs_mode)mtcs_mode_for_size (self,POINTER_SIZE, mtcs_mode_get_class(self,mtcs_mode_get_Pmode(self))/*!GET_MODE_CLASS (Pmode)*/, 0).require ());
    n_debug("mtcsmode.c  init_derived_machine_modes 44 byte:%d :word:%d ptr:%d\n",self->byte_mode,self->word_mode,self->ptr_mode);

}

/* Return a machine mode of class MCLASS with SIZE bits of precision,
   if one exists.  The mode may have padding bits as well the SIZE
   value bits.  If LIMIT is nonzero, disregard modes wider than
   MAX_FIXED_MODE_SIZE.  */
//原型 mode_fro_size machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_for_size (MtcsMode *self,poly_uint64 size, enum mode_class mclass, int limit)
{
  machine_mode mode;
  int i;

  if (limit && maybe_gt (size, (unsigned int)mtcs_mode_get_max_fixed_size/*!MAX_FIXED_MODE_SIZE*/(self)))
    return opt_machine_mode ();

  /* Get the first mode which has this size, in the specified class.  */
  MTCS_FOR_EACH_MODE_IN_CLASS (self,mode, mclass)
    if (known_eq (mtcs_mode_get_precision(self,mode), size))
      return mode;

  if (mclass == MODE_INT || mclass == MODE_PARTIAL_INT)
    for (i = 0; i < self->mtcs_NUM_INT_N_ENTS; i ++)
      if (known_eq (self->intData[i].bitsize, size) && self->int_n_enabled_p[i])
          return self->intData[i].m;
  return opt_machine_mode ();
}

//原型
//#define TRULY_NOOP_TRUNCATION_MODES_P(MODE1, MODE2) \
//  (targetm.truly_noop_truncation (GET_MODE_PRECISION (MODE1), \
//                  GET_MODE_PRECISION (MODE2)))
//machmode.h
nboolean mtcs_mode_truly_noop_truncation_p(MtcsMode *self,mtcs_mode out,mtcs_mode in)
{
    MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
    return mtcsTarget->truly_noop_truncation(mtcsTarget,mtcs_mode_get_precision(self,out),mtcs_mode_get_precision(self,in));
}

//原型  GET_CLASS_NARROWEST_MODE machmode.h
mtcs_mode mtcs_mode_get_class_narrowest(MtcsMode *self,enum mode_class cl)
{
     return (mtcs_mode)self->classNarrowestMode[cl];
}

//原型 FLOAT_LIB_COMPARE_RETURNS_BOOL (mode, comparison) default.h
nboolean mtcs_mode_float_lib_compare_return_bool(MtcsMode *self,mtcs_mode mode,enum rtx_code code)
{
    return FALSE;
}

//原型
///* Nonzero if MODE is a scalar fract or accum mode.  */
//#define SIGNED_SCALAR_FIXED_POINT_MODE_P(MODE)  \
//  (SCALAR_FRACT_MODE_P (MODE) || SCALAR_ACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_signed_scalar_fixed_point_p(MtcsMode *self,mtcs_mode mode)
{
    unsigned char cl= mtcs_mode_get_class(self,mode);
    return cl==MODE_FRACT || cl==MODE_ACCUM;
}


//原型
///* Nonzero if MODE is a scalar ufract or uaccum mode.  */
//#define UNSIGNED_SCALAR_FIXED_POINT_MODE_P(MODE)    \
//  (SCALAR_UFRACT_MODE_P (MODE) || SCALAR_UACCUM_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_unsigned_scalar_fixed_point_p(MtcsMode *self,mtcs_mode mode)
{
     unsigned char cl= mtcs_mode_get_class(self,mode);
     return cl==MODE_UFRACT || cl==MODE_UACCUM;
}

//原型 REVERSIBLE_CC_MODE default.h host =1 nvptx=0
nushort mtcs_mode_get_reversible_cc_mode(MtcsMode *self,mtcs_mode mode)
{
   return self->get_reversible_cc_mode(self,mode);
}

//#define GET_MODE_COMPLEX_MODE(MODE) ((machine_mode) mode_complex[MODE])
nushort mtcs_mode_get_complex(MtcsMode *self,mtcs_mode mode)
{
    return self->modeComplex[mode];
}

/* Nonzero if MODE is a scalar fract, ufract, accum or uaccum mode.  */
//原型
//#define ALL_SCALAR_FIXED_POINT_MODE_P(MODE) \
//  (SIGNED_SCALAR_FIXED_POINT_MODE_P (MODE)  \
//   || UNSIGNED_SCALAR_FIXED_POINT_MODE_P (MODE))
//machmode.h
nboolean mtcs_mode_is_all_scalar_fixed_point_p(MtcsMode *self,mtcs_mode mode)
{
   return mtcs_mode_is_signed_scalar_fixed_point_p(self,mode) ||
           mtcs_mode_is_unsigned_scalar_fixed_point_p(self,mode);
}

//原型 #define FUNCTION_MODE QImode nvptx
void mtcs_mode_set_function_mode(MtcsMode *self,mtcs_mode functionMode)
{
    self->functionMode=functionMode;
}

mtcs_mode  mtcs_mode_get_function_mode(MtcsMode *self)
{
    return self->functionMode;
}

//原型 #define STACK_SIZE_MODE Pmode nvptx.h
void mtcs_mode_set_stack_size_mode(MtcsMode *self,mtcs_mode functionMode)
{
    self->stackSizeMode=functionMode;

}

mtcs_mode  mtcs_mode_get_stack_size_mode(MtcsMode *self)
{
    return self->stackSizeMode;
}


void mtcs_mode_set_name_data(MtcsMode *self,char **modeName)
{
    self->modeName=modeName;
}

void mtcs_mode_set_size_data(MtcsMode *self,poly_uint16 *modeSize)
{
    self->modeSize=modeSize;
}

void mtcs_mode_set_class_data(MtcsMode *self,uchar *modeClass)
{
    self->modeClass=modeClass;
}

void mtcs_mode_set_base_align_data(MtcsMode *self,nushort *modeBaseAlign)
{
    self->modeBaseAlign=modeBaseAlign;
}

void mtcs_mode_set_precision_data(MtcsMode *self,poly_uint16 *modePrecision)
{
    self->modePrecision=modePrecision;
}

void mtcs_mode_set_inner_data(MtcsMode *self,nushort *modeInner)
{
    self->modeInner=modeInner;
}

void mtcs_mode_set_nunits_data(MtcsMode *self,poly_uint16 *modeNunits)
{
    self->modeNunits=modeNunits;
}

void mtcs_mode_set_unit_size_data(MtcsMode *self,nuchar *modeUnitSize)
{
    self->modeUnitSize=modeUnitSize;
}

void mtcs_mode_set_unit_precision_data(MtcsMode *self,nushort *modeUnitPrecision)
{
    self->modeUnitPrecision=modeUnitPrecision;
}

void mtcs_mode_set_class_narrowest_data(MtcsMode *self,nushort *classNarrowestMode)
{
    self->classNarrowestMode=classNarrowestMode;
}

void mtcs_mode_set_wider_data(MtcsMode *self,nushort *modeWider)
{
    self->modeWider=modeWider;
}

void mtcs_mode_set_next_data(MtcsMode *self,nushort *modeNext)
{
    self->modeNext=modeNext;

}

void mtcs_mode_set_2xwider_data(MtcsMode *self,nushort *mode2xwider)
{
    self->mode2xwider=mode2xwider;
}

void mtcs_mode_set_complex_data(MtcsMode *self,nushort *modeComplex)
{
    self->modeComplex=modeComplex;
}

//原型 mode_fbit insn-modes.cc
void mtcs_mode_set_fbit(MtcsMode *self ,nuchar *modeFBit)
{
    self->modeFBit=modeFBit;
}


/* Return the narrowest mode of class MCLASS that contains at least
   SIZE bits.  Abort if no such mode exists.  */
//原型 extern machine_mode smallest_mode_for_size (poly_uint64, enum mode_class); machmode.h stor-layout.cc
mtcs_mode mtcs_mode_smallest_mode_for_size (MtcsMode *self,poly_uint64 size, enum mode_class mclass)
{
  machine_mode mode = VOIDmode;
  int i;

  /* Get the first mode which has at least this size, in the
     specified class.  */
  MTCS_FOR_EACH_MODE_IN_CLASS (self,mode, mclass)
    if (known_ge (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(self,mode), size))
      break;

  gcc_assert (mode != VOIDmode);
  //NUM_INT_N_ENTS host=1 nvptx=1
  if (mclass == MODE_INT || mclass == MODE_PARTIAL_INT)
    for (i = 0; i < NUM_INT_N_ENTS; i ++)
      if (known_ge (self->intData/*int_n_data*/[i].bitsize, size)
      && known_lt (self->intData/*int_n_data*/[i].bitsize, mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(self,mode))
      && self->int_n_enabled_p[i])
    mode =self->intData/*int_n_data*/[i].m;

  return mode;
}



/* Construct an iterator for a bitfield that spans BITSIZE bits,
   starting at BITPOS.

   BITREGION_START is the bit position of the first bit in this
   sequence of bit fields.  BITREGION_END is the last bit in this
   sequence.  If these two fields are non-zero, we should restrict the
   memory access to that range.  Otherwise, we are allowed to touch
   any adjacent non bit-fields.

   ALIGN is the alignment of the underlying object in bits.
   VOLATILEP says whether the bitfield is volatile.  */

mtcs_bit_field_mode_iterator::mtcs_bit_field_mode_iterator (MtcsMode *mode,HOST_WIDE_INT bitsize, HOST_WIDE_INT bitpos,
               poly_int64 bitregion_start,
               poly_int64 bitregion_end,
               unsigned int align, bool volatilep)
: mtcsMode (mode), m_bitsize (bitsize),
  m_bitpos (bitpos), m_bitregion_start (bitregion_start),
  m_bitregion_end (bitregion_end), m_align (align),
  m_volatilep (volatilep), m_count (0)
{
  m_mode=mtcs_mode_get_narrowest_int_mode(mtcsMode);
  if (known_eq (m_bitregion_end, 0)){
      /* We can assume that any aligned chunk of ALIGN bits that overlaps
     the bitfield is mapped and won't trap, provided that ALIGN isn't
     too large.  The cap is the biggest required alignment for data,
     or at least the word size.  And force one such chunk at least.  */
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
      unsigned HOST_WIDE_INT units = MIN (align, MAX (mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign), BITS_PER_WORD));
      if (bitsize <= 0)
          bitsize = 1;
      HOST_WIDE_INT end = bitpos + bitsize + units - 1;
      m_bitregion_end = end - end % units - 1;
  }
}

/* Calls to this function return successively larger modes that can be used
   to represent the bitfield.  Return true if another bitfield mode is
   available, storing it in *OUT_MODE if so.  */

bool mtcs_bit_field_mode_iterator::next_mode (scalar_int_mode *out_mode)
{
  scalar_int_mode mode;
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  for (; m_mode.exists (&mode); m_mode = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,mode)){
      unsigned int unit =mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode);

      /* Skip modes that don't have full precision.  */
      if (unit != mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode))
          continue;

      /* Stop if the mode is too wide to handle efficiently.  */
      if (unit > MAX_FIXED_MODE_SIZE)
          break;

      /* Don't deliver more than one multiword mode; the smallest one
     should be used.  */
      if (m_count > 0 && unit > BITS_PER_WORD)
          break;

      /* Skip modes that are too small.  */
      unsigned HOST_WIDE_INT substart = (unsigned HOST_WIDE_INT) m_bitpos % unit;
      unsigned HOST_WIDE_INT subend = substart + m_bitsize;
      if (subend > unit)
          continue;

      /* Stop if the mode goes outside the bitregion.  */
      HOST_WIDE_INT start = m_bitpos - substart;
      if (maybe_ne (m_bitregion_start, 0)  && maybe_lt (start, m_bitregion_start))
          break;
      HOST_WIDE_INT end = start + unit;
      if (maybe_gt (end, m_bitregion_end + 1))
          break;

      /* Stop if the mode requires too much alignment.  */
      if (mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) > m_align
              &&mtcsTarget/*!targetm.slow_unaligned_access*/->slow_unaligned_access(mtcsTarget,mode, m_align))
          break;

      *out_mode = mode;
      m_mode = mtcs_mode_get_wider/*!GET_MODE_WIDER_MODE*/(mtcsMode,mode);
      m_count++;
      return true;
  }
  return false;
}

/* Return true if smaller modes are generally preferred for this kind
   of bitfield.  */
//SLOW_BYTE_ACCESS 定义在 nvptx.h i386.h gcn.h SLOW_BYTE_ACCESS=0
bool mtcs_bit_field_mode_iterator::prefer_smaller_modes ()
{
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  return (m_volatilep ? mtcsTarget->/*!targetm.narrow_volatile_bitfield*/narrow_volatile_bitfield(mtcsTarget) : !SLOW_BYTE_ACCESS);
}

/* Find the best machine mode to use when referencing a bit field of length
   BITSIZE bits starting at BITPOS.

   BITREGION_START is the bit position of the first bit in this
   sequence of bit fields.  BITREGION_END is the last bit in this
   sequence.  If these two fields are non-zero, we should restrict the
   memory access to that range.  Otherwise, we are allowed to touch
   any adjacent non bit-fields.

   The chosen mode must have no more than LARGEST_MODE_BITSIZE bits.
   INT_MAX is a suitable value for LARGEST_MODE_BITSIZE if the caller
   doesn't want to apply a specific limit.

   If no mode meets all these conditions, we return VOIDmode.

   The underlying object is known to be aligned to a boundary of ALIGN bits.

   If VOLATILEP is false and SLOW_BYTE_ACCESS is false, we return the
   smallest mode meeting these conditions.

   If VOLATILEP is false and SLOW_BYTE_ACCESS is true, we return the
   largest mode (but a mode no wider than UNITS_PER_WORD) that meets
   all the conditions.

   If VOLATILEP is true the narrow_volatile_bitfields target hook is used to
   decide which of the above modes should be used.  */
//原型 get_best_mode machmode.h stor-layout.cc
bool mtcs_mode_get_best_mode(MtcsMode *self,int bitsize, int bitpos, poly_uint64 bitregion_start, poly_uint64 bitregion_end,
           unsigned int align, unsigned HOST_WIDE_INT largest_mode_bitsize, bool volatilep, scalar_int_mode *best_mode)
{
  mtcs_bit_field_mode_iterator iter (self,bitsize, bitpos, bitregion_start,bitregion_end, align, volatilep);
  scalar_int_mode mode;
  bool found = false;
  while (iter.next_mode (&mode)
     /* ??? For historical reasons, reject modes that would normally
        receive greater alignment, even if unaligned accesses are
        acceptable.  This has both advantages and disadvantages.
        Removing this check means that something like:

           struct s { unsigned int x; unsigned int y; };
           int f (struct s *s) { return s->x == 0 && s->y == 0; }

        can be implemented using a single load and compare on
        64-bit machines that have no alignment restrictions.
        For example, on powerpc64-linux-gnu, we would generate:

            ld 3,0(3)
            cntlzd 3,3
            srdi 3,3,6
            blr

        rather than:

            lwz 9,0(3)
            cmpwi 7,9,0
            bne 7,.L3
            lwz 3,4(3)
            cntlzw 3,3
            srwi 3,3,5
            extsw 3,3
            blr
            .p2align 4,,15
        .L3:
            li 3,0
            blr

        However, accessing more than one field can make life harder
        for the gimple optimizers.  For example, gcc.dg/vect/bb-slp-5.c
        has a series of unsigned short copies followed by a series of
        unsigned short comparisons.  With this check, both the copies
        and comparisons remain 16-bit accesses and FRE is able
        to eliminate the latter.  Without the check, the comparisons
        can be done using 2 64-bit operations, which FRE isn't able
        to handle in the same way.

        Either way, it would probably be worth disabling this check
        during expand.  One particular example where removing the
        check would help is the get_best_mode call in store_bit_field.
        If we are given a memory bitregion of 128 bits that is aligned
        to a 64-bit boundary, and the bitfield we want to modify is
        in the second half of the bitregion, this check causes
        store_bitfield to turn the memory into a 64-bit reference
        to the _first_ half of the region.  We later use
        adjust_bitfield_address to get a reference to the correct half,
        but doing so looks to adjust_bitfield_address as though we are
        moving past the end of the original object, so it drops the
        associated MEM_EXPR and MEM_OFFSET.  Removing the check
        causes store_bit_field to keep a 128-bit memory reference,
        so that the final bitfield reference still has a MEM_EXPR
        and MEM_OFFSET.  */
     && mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(self,mode) <= align
     && mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(self,mode) <= largest_mode_bitsize){
      *best_mode = mode;
      found = true;
      if (iter.prefer_smaller_modes ())
          break;
  }
  return found;
}


//原型 STACK_SAVEAREA_MODE default.h
mtcs_mode mtcs_mode_get_stack_savearea_mode(MtcsMode *self,enum save_level level)
{
    return self->get_stack_savearea_mode(self,level);
}

//原型 #define REGMODE_NATURAL_SIZE(MODE)  UNITS_PER_WORD regs.h
int mtcs_mode_get_regmode_natural_size(MtcsMode *self,mtcs_mode mode)
{
    return self->get_regmode_natural_size(self,mode);
}



//原型 MACRO_MODE machmode.h
mtcs_mode mtcs_mode_get_macro(MtcsMode *self,mtcs_mode mode)
{
#if NUM_POLY_INT_COEFFS == 1 //host=1 nvptx=1 insn-modes.h
    return mtcs_mode_as_a <fixed_size_mode> (self,mode);
#else
    return mode;
#endif
}

//原型 init_adjust_machine_modes machmode.h insn-modes.cc
void mtcs_mode_init_adjust_machine_modes(MtcsMode *self)
{
    self->init_adjust_machine_modes(self);
}


/* If a piece of code is using vector mode VECTOR_MODE and also wants
   to operate on integer vectors with the same element size and number
   of elements, return the vector mode it should use.  Return an empty
   opt_machine_mode if there is no supported vector mode with the
   required properties.

   Unlike mode_for_vector. any returned mode is guaranteed to satisfy
   both VECTOR_MODE_P and targetm.vector_mode_supported_p.  */
//原型 related_int_vector_mode machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_related_int_vector_mode (MtcsMode *self,machine_mode vector_mode)
{
  gcc_assert (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(self,vector_mode));
  scalar_int_mode int_mode;
  if (mtcs_mode_int_mode_for_mode/*!int_mode_for_mode*/(self,
          mtcs_mode_get_inner/*!GET_MODE_INNER*/(self,vector_mode)).exists (&int_mode))
    return mtcs_mode_related_vector_mode/*!related_vector_mode*/(self,vector_mode, int_mode,
                mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(self,vector_mode));
  return opt_machine_mode ();
}


/* Find a mode that is suitable for representing a vector with NUNITS
   elements of mode INNERMODE, if one exists.  The returned mode can be
   either an integer mode or a vector mode.  */
//原型 mode_for_vector machmode.h stor-layout.cc
opt_machine_mode mtcs_mode_mode_for_vector (MtcsMode *self,scalar_mode innermode, poly_uint64 nunits)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   machine_mode mode;

   /* First, look for a supported vector type.  */
   if (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(self,innermode))
      mode = self->modesMinMax.min_VECTOR_FLOAT/*!MIN_MODE_VECTOR_FLOAT*/;
   else if (mtcs_mode_is_scalar_fract_p/*!SCALAR_FRACT_MODE_P*/(self,innermode))
      mode = self->modesMinMax.min_VECTOR_FRACT/*!MIN_MODE_VECTOR_FRACT*/;
   else if (mtcs_mode_is_scalar_ufract_p/*!SCALAR_UFRACT_MODE_P*/(self,innermode))
      mode = self->modesMinMax.min_VECTOR_UFRACT/*!MIN_MODE_VECTOR_UFRACT*/;
   else if (mtcs_mode_is_scalar_accum_p/*!SCALAR_ACCUM_MODE_P*/(self,innermode))
      mode = self->modesMinMax.min_VECTOR_ACCUM/*!MIN_MODE_VECTOR_ACCUM*/;
   else if (mtcs_mode_is_scalar_uaccum_p/*!SCALAR_UACCUM_MODE_P*/(self,innermode))
      mode = self->modesMinMax.min_VECTOR_UACCUM/*!MIN_MODE_VECTOR_UACCUM*/;
   else
      mode = self->modesMinMax.min_VECTOR_INT/*!MIN_MODE_VECTOR_INT*/;

   /* Only check the broader vector_mode_supported_any_target_p here.
   We'll filter through target-specific availability and
   vector_mode_supported_p later in vector_type_mode.  */
   MTCS_FOR_EACH_MODE_FROM (self,mode, mode)
      if (known_eq (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(self,mode), nunits)
      && mtcs_mode_get_inner/*!GET_MODE_INNER*/(self,mode) == innermode
      && mtcsTarget/*!targetm.vector_mode_supported_any_target_p*/->vector_mode_supported_any_target_p(mtcsTarget,mode))
         return mode;

   /* For integers, try mapping it to a same-sized scalar mode.  */
   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(self,innermode) == MODE_INT){
      poly_uint64 nbits = nunits * mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(self,innermode);
      if (mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(self,nbits, 0).exists (&mode)
      && mtcsReg->hardRegs.x_have_regs_of_mode/*!have_regs_of_mode*/[mode])
         return mode;
   }

   return opt_machine_mode ();
}


//原型 LOAD_EXTEND_OP(M) defaults.h
rtx_code mtcs_mode_load_extend_op(MtcsMode *self,mtcs_mode m)
{
    return self->load_extend_op(self,m);
}
//原型 targetm.unwind_word_mode() #define TARGET_UNWIND_WORD_MODE default_unwind_word_mode
scalar_int_mode mtcs_mode_unwind_word_mode(MtcsMode *self)
{
    if(self->unwind_word_mode)
        return self->unwind_word_mode(self);
    return self->word_mode;
}
//原型 targetm.eh_return_filter_mode () #define TARGET_EH_RETURN_FILTER_MODE default_eh_return_filter_mode
scalar_int_mode mtcs_mode_eh_return_filter_mode(MtcsMode *self)
{
    if(self->eh_return_filter_mode)
        return self->eh_return_filter_mode(self);
    return mtcs_mode_unwind_word_mode(self);
}

//原型 #define IS_STACK_MODE(MODE)   (X87_FLOAT_MODE_P (MODE) && (!(SSE_FLOAT_MODE_P (MODE) && TARGET_SSE_MATH) || TARGET_MIX_SSE_I387))
//nvptx 未定义 IS_STACK_MODE
nboolean mtcs_mode_is_stack_mode(MtcsMode *self,machine_mode m)
{
   if(self->is_stack_mode)
     return self->is_stack_mode(self,m);
   else{
      n_error("平台未定义 is_stack_mode\n");
   }
   return false;
}

//原型 #define OPAQUE_MODE_P(MODE)    (GET_MODE_CLASS (MODE) == MODE_OPAQUE)
nboolean mtcs_mode_opaque_mode_p(MtcsMode *self,machine_mode m)
{
   return mtcs_mode_get_class(self,m)==MODE_OPAQUE;
}

/* Return the subreg byte offset for a subreg whose outer mode is
   OUTER_MODE, whose inner mode is INNER_MODE, and where there are
   LSB_SHIFT *bits* between the lsb of the outer value and the lsb of
   the inner value.  This is the inverse of subreg_lsb_1 (which converts
   byte offsets to bit shifts).  */
//原型 subreg_offset_from_lsb rtl.h
poly_uint64 mtcs_mode_subreg_offset_from_lsb (MtcsMode *self,machine_mode outer_mode,
         machine_mode inner_mode,poly_uint64 lsb_shift)
{
   //subreg_size_offset_from_lsb在rtlanal.cc实现
  return subreg_size_offset_from_lsb (mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(self,outer_mode),
        mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(self,inner_mode), lsb_shift);
}

//原型 targetm.c.mode_for_floating_type (TI_DOUBLE_TYPE); #define TARGET_C_MODE_FOR_FLOATING_TYPE default_mode_for_floating_type
machine_mode mtcs_mode_for_floating_type(MtcsMode *self,enum tree_index ti)
{
    return self->mode_for_floating_type(self,ti);
}

//原型 push_rounding 平台定义
poly_int64 mtcs_mode_push_rounding(MtcsMode *self,poly_int64 bytes)
{
   if(self->push_rounding)
      return self->push_rounding(self,bytes);
   else{
      printf("mtcs_mode_push_rounding mtcsconfig不应该配置 PUSH_ROUNDING\n");
      gcc_unreachable ();
   }
}
//实现接口MtcsBackupRestore 备分声明在machmode.h中的三个变量byte_mode word_mode ptr_mode
static   void backup_cb(MtcsBackupRestore *iface)
{
    MtcsMode *self=(MtcsMode *)iface->impl;
    MtcsModeBackup *backup=(MtcsModeBackup *)self->backup;
    backup->back_byte_mode=byte_mode;
    backup->back_word_mode=word_mode;
    backup->back_ptr_mode=ptr_mode;

    //设主机的mode为设备的mode
    byte_mode=self->byte_mode;
    word_mode=self->word_mode;
    ptr_mode=self->ptr_mode;
}

static   void restore_cb(MtcsBackupRestore *iface)
{
    MtcsMode *self=(MtcsMode *)iface->impl;
    MtcsModeBackup *backup=(MtcsModeBackup *)self->backup;
    byte_mode=backup->back_byte_mode;
    word_mode=backup->back_word_mode;
    ptr_mode= backup->back_ptr_mode;
}

void mtcs_mode_set_target(MtcsMode *self,npointer target)
{
    self->target=target;
}




