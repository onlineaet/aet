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

#ifndef __GCC_MTCS_MICRO__
#define __GCC_MTCS_MICRO__

#include "../nlib.h"
#include "bitmap.h"

extern int test_abort;//用来测试的
/**
 * I386:
 * #define HARD_REGNO_NREGS_HAS_PADDING(REGNO, MODE)            \
  (TARGET_128BIT_LONG_DOUBLE && !TARGET_64BIT               \
   && GENERAL_REGNO_P (REGNO)                       \
   && ((MODE) == XFmode || (MODE) == XCmode))
 */
#define MTCS_HARD_REGNO_NREGS_HAS_PADDING(REGNO, MODE) 0
#define MTCS_HARD_REGNO_NREGS_WITH_PADDING(REGNO, MODE) -1

typedef int mtcs_reg_class;

//原型 HARD_REG_SET hard-reg-set.h
//typedef struct _HardRegSet
//{
//    nuint count;
//    //#define HARD_REG_SET_LONGS  ((FIRST_PSEUDO_REGISTER + HOST_BITS_PER_WIDEST_FAST_INT - 1) / HOST_BITS_PER_WIDEST_FAST_INT)
//    //HARD_REG_SET_LONGS 不会很大
//    unsigned long /*HARD_REG_ELT_TYPE*/ elts[10/*!HARD_REG_SET_LONGS*/];
//}HardRegSet;

typedef struct _HardRegSet HardRegSet;

typedef struct _HardRegSet
{
    int count;
    //#define HARD_REG_SET_LONGS  ((FIRST_PSEUDO_REGISTER + HOST_BITS_PER_WIDEST_FAST_INT - 1) / HOST_BITS_PER_WIDEST_FAST_INT)
       //HARD_REG_SET_LONGS 不会很大
    HARD_REG_ELT_TYPE elts[10/*!HARD_REG_SET_LONGS*/];

    HardRegSet  operator~ () const{
        HardRegSet res;
        res.count=count;
        for (unsigned int i = 0; i <count; ++i)
          res.elts[i] = ~elts[i];
        return res;
    }

    HardRegSet   operator& (const HardRegSet &other) const {
        HardRegSet res;
        res.count=count;
        for (unsigned int i = 0; i <count; ++i)
          res.elts[i] = elts[i] & other.elts[i];
        return res;
    }

    HardRegSet &  operator&= (const HardRegSet &other){
        for (unsigned int i = 0; i < count; ++i)
          elts[i] &= other.elts[i];
        return *this;
    }

    HardRegSet  operator| (const HardRegSet &other) const {
        HardRegSet res;
        res.count=count;
        for (unsigned int i = 0; i < count; ++i)
          res.elts[i] = elts[i] | other.elts[i];
        return res;
    }

    HardRegSet &  operator|= (const HardRegSet &other)  {
        for (unsigned int i = 0; i < count; ++i)
          elts[i] |= other.elts[i];
        return *this;
    }

    bool  operator== (const HardRegSet &other) const {
        HARD_REG_ELT_TYPE bad = 0;
        for (unsigned int i = 0; i < count; ++i)
          bad |= (elts[i] ^ other.elts[i]);
        return bad == 0;
    }

    bool  operator!= (const HardRegSet &other) const   {
        return !operator== (other);
    }
};


template<typename T> struct mtcs_array_traits/*!array_traits*/;

/* Provides a read-only bitmap view of a single integer bitmask or an
   array of integer bitmasks, or of a wrapper around such bitmasks.  */
template<typename T, typename MtcsTraits = mtcs_array_traits/*!array_traits*/<T>,
    bool has_constant_size = MtcsTraits::has_constant_size>
class mtcs_bitmap_view;

template<>
struct mtcs_array_traits<HardRegSet>
{
  typedef HARD_REG_ELT_TYPE element_type;
  static const bool has_constant_size = true;
  static const size_t constant_size = 10;//HARD_REG_SET_LONGS;
  static const element_type *base (const HardRegSet &x) { return x.elts; }
  static size_t size (const HardRegSet &x) { return x.count/*!HARD_REG_SET_LONGS*/; }
};

template<>
struct array_traits<HardRegSet>
{
  typedef HARD_REG_ELT_TYPE element_type;
  static const bool has_constant_size = true;
  static const size_t constant_size = 10;//HARD_REG_SET_LONGS;
  static const element_type *base (const HardRegSet &x) { return x.elts; }
  static size_t size (const HardRegSet &x) { return x.count/*!HARD_REG_SET_LONGS*/; }
};



/* Base class for bitmap_view; see there for details.  */
template<typename T, typename MtcsTraits = mtcs_array_traits<T> >
class mtcs_base_bitmap_view
{
public:
  typedef typename MtcsTraits::element_type array_element_type;

  mtcs_base_bitmap_view (const T &, bitmap_element *);
  operator const_bitmap () const { return &m_head; }

private:
  mtcs_base_bitmap_view (const mtcs_base_bitmap_view &);

  bitmap_head m_head;
};


/* Provides a read-only bitmap view of a single integer bitmask or a
   constant-sized array of integer bitmasks, or of a wrapper around such
   bitmasks.  */
template<typename T, typename MtcsTraits>
class mtcs_bitmap_view<T, MtcsTraits, true> : public mtcs_base_bitmap_view<T, MtcsTraits>
{
public:
   mtcs_bitmap_view (const T &array)
    : mtcs_base_bitmap_view<T, MtcsTraits> (array, m_bitmap_elements) {}

private:
  /* How many bitmap_elements we need to hold a full T.  */
  static const size_t num_bitmap_elements
    = CEIL (CHAR_BIT
       * sizeof (typename MtcsTraits::element_type)
       * MtcsTraits::constant_size,
       BITMAP_ELEMENT_ALL_BITS);
  bitmap_element m_bitmap_elements[num_bitmap_elements];
};


/* Initialize the view for array ARRAY, using the array of bitmap
   elements in BITMAP_ELEMENTS (which is known to contain enough
   entries).  */
template<typename T, typename MtcsTraits>
mtcs_base_bitmap_view<T, MtcsTraits>::mtcs_base_bitmap_view (const T &array,
                      bitmap_element *bitmap_elements)
{
  m_head.obstack = NULL;
  /* The code currently assumes that each element of ARRAY corresponds
     to exactly one bitmap_element.  */
  const size_t array_element_bits = CHAR_BIT * sizeof (array_element_type);
  STATIC_ASSERT (BITMAP_ELEMENT_ALL_BITS % array_element_bits == 0);
  size_t array_step = BITMAP_ELEMENT_ALL_BITS / array_element_bits;
  size_t array_size = MtcsTraits::size (array);

  /* Process each potential bitmap_element in turn.  The loop is written
     this way rather than per array element because usually there are
     only a small number of array elements per bitmap element (typically
     two or four).  The inner loops should therefore unroll completely.  */
  const array_element_type *array_elements = MtcsTraits::base (array);
  unsigned int indx = 0;
  for (size_t array_base = 0;
       array_base < array_size;
       array_base += array_step, indx += 1)
    {
      /* How many array elements are in this particular bitmap_element.  */
      unsigned int array_count
   = (STATIC_CONSTANT_P (array_size % array_step == 0)
      ? array_step : MIN (array_step, array_size - array_base));

      /* See whether we need this bitmap element.  */
      array_element_type ior = array_elements[array_base];
      for (size_t i = 1; i < array_count; ++i)
   ior |= array_elements[array_base + i];
      if (ior == 0)
   continue;

      /* Grab the next bitmap element and chain it.  */
      bitmap_element *bitmap_element = bitmap_elements++;
      if (m_head.current)
   m_head.current->next = bitmap_element;
      else
   m_head.first = bitmap_element;
      bitmap_element->prev = m_head.current;
      bitmap_element->next = NULL;
      bitmap_element->indx = indx;
      m_head.current = bitmap_element;
      m_head.indx = indx;

      /* Fill in the bits of the bitmap element.  */
      if (array_element_bits < BITMAP_WORD_BITS)
   {
     /* Multiple array elements fit in one element of
        bitmap_element->bits.  */
     size_t array_i = array_base;
     for (unsigned int word_i = 0; word_i < BITMAP_ELEMENT_WORDS;
          ++word_i)
       {
         BITMAP_WORD word = 0;
         for (unsigned int shift = 0;
         shift < BITMAP_WORD_BITS && array_i < array_size;
         shift += array_element_bits)
      word |= array_elements[array_i++] << shift;
         bitmap_element->bits[word_i] = word;
       }
   }
      else
   {
     /* Array elements are the same size as elements of
        bitmap_element->bits, or are an exact multiple of that size.  */
     unsigned int word_i = 0;
     for (unsigned int i = 0; i < array_count; ++i)
       for (unsigned int shift = 0; shift < array_element_bits;
       shift += BITMAP_WORD_BITS)
         bitmap_element->bits[word_i++]
      = array_elements[array_base + i] >> shift;
     while (word_i < BITMAP_ELEMENT_WORDS)
       bitmap_element->bits[word_i++] = 0;
   }
    }
}


typedef struct _MtcsCgraphRtlInfo {
  unsigned int preferred_incoming_stack_boundary;

  /* Which registers the function clobbers, either directly or by
     calling another function.  */
  HardRegSet function_used_regs;
}MtcsCgraphRtlInfo;

typedef struct _MtcsNode MtcsNode;
struct _MtcsNode{
};

//原型  #define OACC_FN_ATTRIB "oacc function" omp-general.h
#define MTCS_FN_ATTRIB "mtcs function"
#define MTCS_FN_KERNEL_ATTRIB "mtcs kernel"
#define MTCS_FN_DEVICE_ATTRIB "mtcs device"


/**
 * gcn 用
 * ptx 用 grid  cluster block
 * 如何把 gand worker vector 统一到 grid  cluster block?
 */
typedef enum{
    MTCS_HIER_GRID, //gand
    MTCS_HIER_CLUSTER,
    MTCS_HIER_BLOCK, //worker
    MTCS_HIER_THREAD, //vector
    MTCS_HIER_THREAD_COUNT
}MtcsHierThread;


typedef struct _MtcsCumulativeArgs MtcsCumulativeArgs;
struct _MtcsCumulativeArgs
{
   int magic;//暂时未使用
};

//原型 NUM_ABI_IDS function-abi.h
const size_t MTCS_NUM_ABI_IDS = 8;

/**
 * mtcsbuiltins.cc mtcs_builtins_builtin_memset_read_str回调函数的用户数据类型
 */

typedef struct _BuiltinReadStrData
{
   void *self;
   void *data;
}BuiltinReadStrData;

#define MAX_FIRST_PSEUDO_REGISTER 20 //原型 FIRST_PSEUDO_REGISTER  nvptx.h host =? nvptx=16
#define MAX_NUM_MODE_INT          10  //原型 NUM_MODE_INT insn-modes.h host =? nvptx=(MAX_MODE_INT - MIN_MODE_INT + 1)=5
#define MAX_NUM_MODE_PARTIAL_INT  16  //原型 NUM_MODE_PARTIAL_INT insn-modes.h host =? nvptx=0
#define MAX_NUM_MODE_VECTOR_INT   16  //原型 NUM_MODE_VECTOR_INT insn-modes.h host =? nvptx=(MAX_MODE_VECTOR_INT - MIN_MODE_VECTOR_INT + 1)=2

#define MAX_MAX_MODE_INT          16  //原型 MAX_MODE_INT insn-modes.h host =? nvptx=E_TImode=8
#define MAX_NUM_MODE_IP_INT  (MAX_NUM_MODE_INT + MAX_NUM_MODE_PARTIAL_INT)
#define MAX_NUM_MODE_IPV_INT (MAX_NUM_MODE_IP_INT + MAX_NUM_MODE_VECTOR_INT)

#define MAX_NUM_MACHINE_MODES 50  //host=? nvptx=43
#define MAX_MAX_MACHINE_MODE  MAX_NUM_MACHINE_MODES

#define MAX_MAX_RECOG_OPERANDS  30 //host=? nvptx=30

#define MAX_MAX_DUP_OPERANDS  2  //host=? nvptx=2

#define MAX_NUM_INSN_CODES  800 //host=? nvptx=737

#define MAX_N_REG_CLASSES 3  //host=? nvptx=3

#define MAX_MAX_REGS_PER_ADDRESS 3 //host=2 nvptx=1

//原型 rtx_mode_t rtl.h
typedef std::pair <rtx, nuint> mtcs_rtx_mode_t;

//原型 namespace wi ... struct int_traits <rtx_mode_t> rtl.h
namespace wi
{
  template <>
  struct int_traits <mtcs_rtx_mode_t>
  {
    static const enum precision_type precision_type = VAR_PRECISION;
    static const bool host_dependent_precision = false;
    /* This ought to be true, except for the special case that BImode
       is canonicalized to STORE_FLAG_VALUE, which might be 1.  */
    static const bool is_sign_extended = false;
    static const bool needs_write_val_arg = false;
    static unsigned int get_precision (const mtcs_rtx_mode_t &);
    static wi::storage_ref decompose (HOST_WIDE_INT *, unsigned int,
                  const mtcs_rtx_mode_t &);
  };
}

//原型 inline wi::rtx_to_poly_wide_ref wi::to_poly_wide (const_rtx x, machine_mode mode) rtl.h
inline wi::rtx_to_poly_wide_ref mtcs_rtl_to_poly_wide (const_rtx x, machine_mode mode)
{
  if (CONST_POLY_INT_P (x))
    return const_poly_int_value (x);
  return mtcs_rtx_mode_t (const_cast<rtx> (x), mode);
}


#endif

