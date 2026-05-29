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

#ifndef __GCC_MTCS_TREE__
#define __GCC_MTCS_TREE__

#include "../nlib.h"
#include "mtcscomponent.h"

//原型 builtin_structptr_type tree.h tree& node;c++引用型类成员变量不能赋值 所以改为结构体
typedef struct _mtcs_builtin_structptr_type
{
  tree  node;
  tree  base;
  const char *str;
}mtcs_builtin_structptr_type;

struct mtcs_int_cst_hasher : ggc_cache_ptr_hash<tree_node>
{
  static hashval_t hash (tree t);
  static bool equal (tree x, tree y);
};

/* Hasher for general trees, based on their TREE_HASH.  */
struct GTY((for_user)) mtcs_type_hash {
  unsigned long hash;
  tree type;
};

//struct mtcs_type_cache_hasher;
struct mtcs_type_cache_hasher : ggc_cache_ptr_hash<mtcs_type_hash>
{
  static hashval_t hash (mtcs_type_hash *t)
  {
     return t->hash;
  }
  static bool equal (mtcs_type_hash *a, mtcs_type_hash *b);
  static int
  keep_cache_entry (mtcs_type_hash *&t)
  {
    return ggc_marked_p (t->type);
  }
};

struct mtcs_poly_int_cst_hasher : ggc_cache_ptr_hash<tree_node>
{
  typedef std::pair<tree, const poly_wide_int *> compare_type;
  static hashval_t hash (tree t);
  static bool equal (tree x, const compare_type &y);
};

#define mtcs_error_mark_node        mtcsTree->global_trees[TI_ERROR_MARK]

#define mtcs_intQI_type_node        mtcsTree->global_trees[TI_INTQI_TYPE]
#define mtcs_intHI_type_node        mtcsTree->global_trees[TI_INTHI_TYPE]
#define mtcs_intSI_type_node        mtcsTree->global_trees[TI_INTSI_TYPE]
#define mtcs_intDI_type_node        mtcsTree->global_trees[TI_INTDI_TYPE]
#define mtcs_intTI_type_node        mtcsTree->global_trees[TI_INTTI_TYPE]

#define mtcs_unsigned_intQI_type_node  mtcsTree->global_trees[TI_UINTQI_TYPE]
#define mtcs_unsigned_intHI_type_node  mtcsTree->global_trees[TI_UINTHI_TYPE]
#define mtcs_unsigned_intSI_type_node  mtcsTree->global_trees[TI_UINTSI_TYPE]
#define mtcs_unsigned_intDI_type_node  mtcsTree->global_trees[TI_UINTDI_TYPE]
#define mtcs_unsigned_intTI_type_node  mtcsTree->global_trees[TI_UINTTI_TYPE]

#define mtcs_atomicQI_type_node  mtcsTree->global_trees[TI_ATOMICQI_TYPE]
#define mtcs_atomicHI_type_node  mtcsTree->global_trees[TI_ATOMICHI_TYPE]
#define mtcs_atomicSI_type_node  mtcsTree->global_trees[TI_ATOMICSI_TYPE]
#define mtcs_atomicDI_type_node  mtcsTree->global_trees[TI_ATOMICDI_TYPE]
#define mtcs_atomicTI_type_node  mtcsTree->global_trees[TI_ATOMICTI_TYPE]

#define mtcs_uint16_type_node    mtcsTree->global_trees[TI_UINT16_TYPE]
#define mtcs_uint32_type_node    mtcsTree->global_trees[TI_UINT32_TYPE]
#define mtcs_uint64_type_node    mtcsTree->global_trees[TI_UINT64_TYPE]
#define mtcs_uint128_type_node      mtcsTree->global_trees[TI_UINT128_TYPE]

#define mtcs_void_node        mtcsTree->global_trees[TI_VOID]

#define mtcs_integer_zero_node      mtcsTree->global_trees[TI_INTEGER_ZERO]
#define mtcs_integer_one_node    mtcsTree->global_trees[TI_INTEGER_ONE]
#define mtcs_integer_minus_one_node    mtcsTree->global_trees[TI_INTEGER_MINUS_ONE]
#define mtcs_size_zero_node         mtcsTree->global_trees[TI_SIZE_ZERO]
#define mtcs_size_one_node       mtcsTree->global_trees[TI_SIZE_ONE]
#define mtcs_bitsize_zero_node      mtcsTree->global_trees[TI_BITSIZE_ZERO]
#define mtcs_bitsize_one_node    mtcsTree->global_trees[TI_BITSIZE_ONE]
#define mtcs_bitsize_unit_node      mtcsTree->global_trees[TI_BITSIZE_UNIT]

/* Base access nodes.  */
#define mtcs_access_public_node     mtcsTree->global_trees[TI_PUBLIC]
#define mtcs_access_protected_node          mtcsTree->global_trees[TI_PROTECTED]
#define mtcs_access_private_node    mtcsTree->global_trees[TI_PRIVATE]

#define mtcs_null_pointer_node      mtcsTree->global_trees[TI_NULL_POINTER]

#define mtcs_float_type_node        mtcsTree->global_trees[TI_FLOAT_TYPE]
#define mtcs_double_type_node    mtcsTree->global_trees[TI_DOUBLE_TYPE]
#define mtcs_long_double_type_node     mtcsTree->global_trees[TI_LONG_DOUBLE_TYPE]
#define mtcs_bfloat16_type_node     mtcsTree->global_trees[TI_BFLOAT16_TYPE]

/* Nodes for particular _FloatN and _FloatNx types in sequence.  */
#define mtcs_FLOATN_TYPE_NODE(IDX)     mtcsTree->global_trees[TI_FLOATN_TYPE_FIRST + (IDX)]
#define mtcs_FLOATN_NX_TYPE_NODE(IDX)  mtcsTree->global_trees[TI_FLOATN_NX_TYPE_FIRST + (IDX)]
#define mtcs_FLOATNX_TYPE_NODE(IDX)    mtcsTree->global_trees[TI_FLOATNX_TYPE_FIRST + (IDX)]

/* Names for individual types (code should normally iterate over all
   such types; these are only for back-end use, or in contexts such as
   *.def where iteration is not possible).  */
#define mtcs_float16_type_node      mtcsTree->global_trees[TI_FLOAT16_TYPE]
#define mtcs_float32_type_node      mtcsTree->global_trees[TI_FLOAT32_TYPE]
#define mtcs_float64_type_node      mtcsTree->global_trees[TI_FLOAT64_TYPE]
#define mtcs_float128_type_node     mtcsTree->global_trees[TI_FLOAT128_TYPE]
#define mtcs_float32x_type_node     mtcsTree->global_trees[TI_FLOAT32X_TYPE]
#define mtcs_float64x_type_node     mtcsTree->global_trees[TI_FLOAT64X_TYPE]
#define mtcs_float128x_type_node    mtcsTree->global_trees[TI_FLOAT128X_TYPE]

/* Type used by certain backends for __float128, which in C++ should be
   distinct type from _Float128 for backwards compatibility reasons.  */
#define mtcs_float128t_type_node    mtcsTree->global_trees[TI_FLOAT128T_TYPE]

#define mtcs_float_ptr_type_node    mtcsTree->global_trees[TI_FLOAT_PTR_TYPE]
#define mtcs_double_ptr_type_node      mtcsTree->global_trees[TI_DOUBLE_PTR_TYPE]
#define mtcs_long_double_ptr_type_node mtcsTree->global_trees[TI_LONG_DOUBLE_PTR_TYPE]
#define mtcs_integer_ptr_type_node     mtcsTree->global_trees[TI_INTEGER_PTR_TYPE]

#define mtcs_complex_integer_type_node mtcsTree->global_trees[TI_COMPLEX_INTEGER_TYPE]
#define mtcs_complex_float_type_node      mtcsTree->global_trees[TI_COMPLEX_FLOAT_TYPE]
#define mtcs_complex_double_type_node  mtcsTree->global_trees[TI_COMPLEX_DOUBLE_TYPE]
#define mtcs_complex_long_double_type_node   mtcsTree->global_trees[TI_COMPLEX_LONG_DOUBLE_TYPE]

#define mtcs_COMPLEX_FLOATN_NX_TYPE_NODE(IDX)   mtcsTree->global_trees[TI_COMPLEX_FLOATN_NX_TYPE_FIRST + (IDX)]

#define mtcs_void_type_node         mtcsTree->global_trees[TI_VOID_TYPE]
/* The C type `void *'.  */
#define mtcs_ptr_type_node       mtcsTree->global_trees[TI_PTR_TYPE]
/* The C type `const void *'.  */
#define mtcs_const_ptr_type_node    mtcsTree->global_trees[TI_CONST_PTR_TYPE]
/* The C type `size_t'.  */
#define mtcs_size_type_node                  mtcsTree->global_trees[TI_SIZE_TYPE]
#define mtcs_pid_type_node                   mtcsTree->global_trees[TI_PID_TYPE]
#define mtcs_ptrdiff_type_node      mtcsTree->global_trees[TI_PTRDIFF_TYPE]
#define mtcs_va_list_type_node      mtcsTree->global_trees[TI_VA_LIST_TYPE]
#define mtcs_va_list_gpr_counter_field mtcsTree->global_trees[TI_VA_LIST_GPR_COUNTER_FIELD]
#define mtcs_va_list_fpr_counter_field mtcsTree->global_trees[TI_VA_LIST_FPR_COUNTER_FIELD]
/* The C type `FILE *'.  */
#define mtcs_fileptr_type_node      mtcsTree->global_trees[TI_FILEPTR_TYPE]
/* The C type `const struct tm *'.  */
#define mtcs_const_tm_ptr_type_node    mtcsTree->global_trees[TI_CONST_TM_PTR_TYPE]
/* The C type `fenv_t *'.  */
#define mtcs_fenv_t_ptr_type_node      mtcsTree->global_trees[TI_FENV_T_PTR_TYPE]
#define mtcs_const_fenv_t_ptr_type_node   mtcsTree->global_trees[TI_CONST_FENV_T_PTR_TYPE]
/* The C type `fexcept_t *'.  */
#define mtcs_fexcept_t_ptr_type_node      mtcsTree->global_trees[TI_FEXCEPT_T_PTR_TYPE]
#define mtcs_const_fexcept_t_ptr_type_node   mtcsTree->global_trees[TI_CONST_FEXCEPT_T_PTR_TYPE]
#define mtcs_pointer_sized_int_node    mtcsTree->global_trees[TI_POINTER_SIZED_TYPE]

#define mtcs_boolean_type_node      mtcsTree->global_trees[TI_BOOLEAN_TYPE]
#define mtcs_boolean_false_node     mtcsTree->global_trees[TI_BOOLEAN_FALSE]
#define mtcs_boolean_true_node      mtcsTree->global_trees[TI_BOOLEAN_TRUE]

/* The decimal floating point types. */
#define mtcs_dfloat32_type_node              mtcsTree->global_trees[TI_DFLOAT32_TYPE]
#define mtcs_dfloat64_type_node              mtcsTree->global_trees[TI_DFLOAT64_TYPE]
#define mtcs_dfloat128_type_node             mtcsTree->global_trees[TI_DFLOAT128_TYPE]

/* The fixed-point types.  */
#define mtcs_sat_short_fract_type_node       mtcsTree->global_trees[TI_SAT_SFRACT_TYPE]
#define mtcs_sat_fract_type_node             mtcsTree->global_trees[TI_SAT_FRACT_TYPE]
#define mtcs_sat_long_fract_type_node        mtcsTree->global_trees[TI_SAT_LFRACT_TYPE]
#define mtcs_sat_long_long_fract_type_node   mtcsTree->global_trees[TI_SAT_LLFRACT_TYPE]
#define mtcs_sat_unsigned_short_fract_type_node    mtcsTree->global_trees[TI_SAT_USFRACT_TYPE]
#define mtcs_sat_unsigned_fract_type_node    mtcsTree->global_trees[TI_SAT_UFRACT_TYPE]
#define mtcs_sat_unsigned_long_fract_type_node  mtcsTree->global_trees[TI_SAT_ULFRACT_TYPE]
#define mtcs_sat_unsigned_long_long_fract_type_node mtcsTree->global_trees[TI_SAT_ULLFRACT_TYPE]
#define mtcs_short_fract_type_node           mtcsTree->global_trees[TI_SFRACT_TYPE]
#define mtcs_fract_type_node                 mtcsTree->global_trees[TI_FRACT_TYPE]
#define mtcs_long_fract_type_node            mtcsTree->global_trees[TI_LFRACT_TYPE]
#define mtcs_long_long_fract_type_node       mtcsTree->global_trees[TI_LLFRACT_TYPE]
#define mtcs_unsigned_short_fract_type_node  mtcsTree->global_trees[TI_USFRACT_TYPE]
#define mtcs_unsigned_fract_type_node        mtcsTree->global_trees[TI_UFRACT_TYPE]
#define mtcs_unsigned_long_fract_type_node   mtcsTree->global_trees[TI_ULFRACT_TYPE]
#define mtcs_unsigned_long_long_fract_type_node    mtcsTree->global_trees[TI_ULLFRACT_TYPE]
#define mtcs_sat_short_accum_type_node       mtcsTree->global_trees[TI_SAT_SACCUM_TYPE]
#define mtcs_sat_accum_type_node             mtcsTree->global_trees[TI_SAT_ACCUM_TYPE]
#define mtcs_sat_long_accum_type_node        mtcsTree->global_trees[TI_SAT_LACCUM_TYPE]
#define mtcs_sat_long_long_accum_type_node   mtcsTree->global_trees[TI_SAT_LLACCUM_TYPE]
#define mtcs_sat_unsigned_short_accum_type_node    mtcsTree->global_trees[TI_SAT_USACCUM_TYPE]
#define mtcs_sat_unsigned_accum_type_node    mtcsTree->global_trees[TI_SAT_UACCUM_TYPE]
#define mtcs_sat_unsigned_long_accum_type_node  mtcsTree->global_trees[TI_SAT_ULACCUM_TYPE]
#define mtcs_sat_unsigned_long_long_accum_type_node mtcsTree->global_trees[TI_SAT_ULLACCUM_TYPE]
#define mtcs_short_accum_type_node           mtcsTree->global_trees[TI_SACCUM_TYPE]
#define mtcs_accum_type_node                 mtcsTree->global_trees[TI_ACCUM_TYPE]
#define mtcs_long_accum_type_node            mtcsTree->global_trees[TI_LACCUM_TYPE]
#define mtcs_long_long_accum_type_node       mtcsTree->global_trees[TI_LLACCUM_TYPE]
#define mtcs_unsigned_short_accum_type_node  mtcsTree->global_trees[TI_USACCUM_TYPE]
#define mtcs_unsigned_accum_type_node        mtcsTree->global_trees[TI_UACCUM_TYPE]
#define mtcs_unsigned_long_accum_type_node   mtcsTree->global_trees[TI_ULACCUM_TYPE]
#define mtcs_unsigned_long_long_accum_type_node    mtcsTree->global_trees[TI_ULLACCUM_TYPE]
#define mtcs_qq_type_node                    mtcsTree->global_trees[TI_QQ_TYPE]
#define mtcs_hq_type_node                    mtcsTree->global_trees[TI_HQ_TYPE]
#define mtcs_sq_type_node                    mtcsTree->global_trees[TI_SQ_TYPE]
#define mtcs_dq_type_node                    mtcsTree->global_trees[TI_DQ_TYPE]
#define mtcs_tq_type_node                    mtcsTree->global_trees[TI_TQ_TYPE]
#define mtcs_uqq_type_node                   mtcsTree->global_trees[TI_UQQ_TYPE]
#define mtcs_uhq_type_node                   mtcsTree->global_trees[TI_UHQ_TYPE]
#define mtcs_usq_type_node                   mtcsTree->global_trees[TI_USQ_TYPE]
#define mtcs_udq_type_node                   mtcsTree->global_trees[TI_UDQ_TYPE]
#define mtcs_utq_type_node                   mtcsTree->global_trees[TI_UTQ_TYPE]
#define mtcs_sat_qq_type_node                mtcsTree->global_trees[TI_SAT_QQ_TYPE]
#define mtcs_sat_hq_type_node                mtcsTree->global_trees[TI_SAT_HQ_TYPE]
#define mtcs_sat_sq_type_node                mtcsTree->global_trees[TI_SAT_SQ_TYPE]
#define mtcs_sat_dq_type_node                mtcsTree->global_trees[TI_SAT_DQ_TYPE]
#define mtcs_sat_tq_type_node                mtcsTree->global_trees[TI_SAT_TQ_TYPE]
#define mtcs_sat_uqq_type_node               mtcsTree->global_trees[TI_SAT_UQQ_TYPE]
#define mtcs_sat_uhq_type_node               mtcsTree->global_trees[TI_SAT_UHQ_TYPE]
#define mtcs_sat_usq_type_node               mtcsTree->global_trees[TI_SAT_USQ_TYPE]
#define mtcs_sat_udq_type_node               mtcsTree->global_trees[TI_SAT_UDQ_TYPE]
#define mtcs_sat_utq_type_node               mtcsTree->global_trees[TI_SAT_UTQ_TYPE]
#define mtcs_ha_type_node                    mtcsTree->global_trees[TI_HA_TYPE]
#define mtcs_sa_type_node                    mtcsTree->global_trees[TI_SA_TYPE]
#define mtcs_da_type_node                    mtcsTree->global_trees[TI_DA_TYPE]
#define mtcs_ta_type_node                    mtcsTree->global_trees[TI_TA_TYPE]
#define mtcs_uha_type_node                   mtcsTree->global_trees[TI_UHA_TYPE]
#define mtcs_usa_type_node                   mtcsTree->global_trees[TI_USA_TYPE]
#define mtcs_uda_type_node                   mtcsTree->global_trees[TI_UDA_TYPE]
#define mtcs_uta_type_node                   mtcsTree->global_trees[TI_UTA_TYPE]
#define mtcs_sat_ha_type_node                mtcsTree->global_trees[TI_SAT_HA_TYPE]
#define mtcs_sat_sa_type_node                mtcsTree->global_trees[TI_SAT_SA_TYPE]
#define mtcs_sat_da_type_node                mtcsTree->global_trees[TI_SAT_DA_TYPE]
#define mtcs_sat_ta_type_node                mtcsTree->global_trees[TI_SAT_TA_TYPE]
#define mtcs_sat_uha_type_node               mtcsTree->global_trees[TI_SAT_UHA_TYPE]
#define mtcs_sat_usa_type_node               mtcsTree->global_trees[TI_SAT_USA_TYPE]
#define mtcs_sat_uda_type_node               mtcsTree->global_trees[TI_SAT_UDA_TYPE]
#define mtcs_sat_uta_type_node               mtcsTree->global_trees[TI_SAT_UTA_TYPE]

/* The node that should be placed at the end of a parameter list to
   indicate that the function does not take a variable number of
   arguments.  The TREE_VALUE will be void_type_node and there will be
   no TREE_CHAIN.  Language-independent code should not assume
   anything else about this node.  */
#define mtcs_void_list_node                  mtcsTree->global_trees[TI_VOID_LIST_NODE]

#define mtcs_main_identifier_node      mtcsTree->global_trees[TI_MAIN_IDENTIFIER]


/* Optimization options (OPTIMIZATION_NODE) to use for default and current
   functions.  */
#define mtcs_optimization_default_node mtcsTree->global_trees[TI_OPTIMIZATION_DEFAULT]
#define mtcs_optimization_current_node mtcsTree->global_trees[TI_OPTIMIZATION_CURRENT]

/* Default/current target options (TARGET_OPTION_NODE).  */
#define mtcs_target_option_default_node   mtcsTree->global_trees[TI_TARGET_OPTION_DEFAULT]
#define mtcs_target_option_current_node   mtcsTree->global_trees[TI_TARGET_OPTION_CURRENT]

/* Default tree list option(), optimize() pragmas to be linked into the
   attribute list.  */
#define mtcs_current_target_pragma     mtcsTree->global_trees[TI_CURRENT_TARGET_PRAGMA]
#define mtcs_current_optimize_pragma      mtcsTree->global_trees[TI_CURRENT_OPTIMIZE_PRAGMA]

/* SCEV analyzer global shared trees.  */
#define mtcs_chrec_not_analyzed_yet    NULL_TREE
#define mtcs_chrec_dont_know        mtcsTree->global_trees[TI_CHREC_DONT_KNOW]
#define mtcs_chrec_known         mtcsTree->global_trees[TI_CHREC_KNOWN]

#define mtcs_char_type_node         mtcsTree->integer_types[itk_char]
#define mtcs_signed_char_type_node     mtcsTree->integer_types[itk_signed_char]
#define mtcs_unsigned_char_type_node      mtcsTree->integer_types[itk_unsigned_char]
#define mtcs_short_integer_type_node      mtcsTree->integer_types[itk_short]
#define mtcs_short_unsigned_type_node          mtcsTree->integer_types[itk_unsigned_short]
#define mtcs_integer_type_node              mtcsTree->integer_types[itk_int]
#define mtcs_unsigned_type_node             mtcsTree->integer_types[itk_unsigned_int]
#define mtcs_long_integer_type_node    mtcsTree->integer_types[itk_long]
#define mtcs_long_unsigned_type_node      mtcsTree->integer_types[itk_unsigned_long]
#define mtcs_long_long_integer_type_node           mtcsTree->integer_types[itk_long_long]
#define mtcs_long_long_unsigned_type_node mtcsTree->integer_types[itk_unsigned_long_long]

#define mtcs_sizetype mtcsTree->sizetype_tab[(int) stk_sizetype]
#define mtcs_bitsizetype mtcsTree->sizetype_tab[(int) stk_bitsizetype]
#define mtcs_ssizetype mtcsTree->sizetype_tab[(int) stk_ssizetype]
#define mtcs_sbitsizetype mtcsTree->sizetype_tab[(int) stk_sbitsizetype]


typedef struct _MtcsTree MtcsTree;
struct _MtcsTree
{
    MtcsComponent parent;
    MtcsBackupRestore mtcsBackupRestore;//备份和恢复接口

    //原型 floatn_nx_types tree.h
    //不需要改变 floatn_type_info floatn_nx_types[NUM_FLOATN_NX_TYPES];
    /* Vector of standard trees used by the C compiler.  */
    //原型 global_trees tree.h
    GTY(()) tree global_trees[TI_MAX];
    /* The standard C integer types.  Use integer_type_kind to index into
       this array.  */
    //原型 integer_types tree.h
    GTY(()) tree integer_types[itk_none];

    /* Types used to represent sizes.  */
    //原型 sizetype_tab tree.h
    GTY(()) tree sizetype_tab[(int) stk_type_kind_last];
    //原型 builtin_structptr_types tree.h
    mtcs_builtin_structptr_type builtin_structptr_types[6];
    /* This is also in machmode.h */
    //原型 int_n_trees tree.h
    GTY(()) struct int_n_trees_t int_n_trees[50/*!NUM_INT_N_ENTS 50足够大*/];

    int intTypeSize;//原型 INT_TYPE_SIZE; default.h 平台定义 nvptx.h
    int longTypeSize;//原型 LONG_TYPE_SIZE; default.h 平台定义 nvptx.h
    int longLongTypeSize;//原型 LONG_LONG_TYPE_SIZE; default.h 平台定义 nvptx.h
    int shortTypeSize;//原型 SHORT_TYPE_SIZE; default.h 平台定义 nvptx.h
    char *sizeTypeStr;//原型 #define SIZETYPE SIZE_TYPE  defaults.h SIZE_TYPE 每个平台定义不一样
    char *ptrDiffTypeStr;//#define PTRDIFF_TYPE (TARGET_ABI64 ? "long int" : "int")


    //原型 lto-lang.cc
     GTY(()) tree mtcs_string_type_node;
     GTY(()) tree mtcs_const_string_type_node;
     GTY(()) tree mtcs_wint_type_node;
     GTY(()) tree mtcs_intmax_type_node;
     GTY(()) tree mtcs_uintmax_type_node;
     GTY(()) tree mtcs_signed_size_type_node;

     //原型 GTY(()) tree builtin_types[(int) BT_LAST + 1]; lto-lang.cc
     GTY(()) tree builtin_types[1000/*!(int) BT_LAST + 1 1000足够大*/];
     GTY(()) tree built_in_attributes[1000/*!(int) ATTR_LAST 100足够大*/];
     /* Names of all the built_in functions.  */
     //原型  tree-core.h extern GTY(()) builtin_info_type builtin_info[(int)END_BUILTINS];
     builtin_info_type builtin_info[(int)END_BUILTINS];//存内建函数的声明 decl
     char * built_in_names[(int) END_BUILTINS];

     //原型 registered_builtin_types lto-lang.cc
     //初始化在 lto_register_builtin_type #define LANG_HOOKS_REGISTER_BUILTIN_TYPE lto_register_builtin_type
     //没找到调用 lto_register_builtin_type的地方 调用方式 lang_hooks.types.register_builtin_type
     GTY(()) tree registered_builtin_types;

     /* Fnspec of each internal function, indexed by function number.  */
     //原型 internal_fn_fnspec_array internal-fn.h internal-fn.cc
     const_tree internal_fn_fnspec_array[IFN_LAST + 1];

     //原型 type_hash_table tree.cc
     GTY ((cache)) hash_table<mtcs_type_cache_hasher> *type_hash_table;
     //原型 next_type_uid tree.cc
     GTY(()) unsigned next_type_uid;
     //原型 int_cst_hash_table tree.cc
     GTY ((cache)) hash_table<mtcs_int_cst_hasher> *int_cst_hash_table;
     //原型 poly_int_cst_hash_table tree.cc
     GTY ((cache)) hash_table<mtcs_poly_int_cst_hasher> *poly_int_cst_hash_table;
     //原型 nonstandard_boolean_type_cache tree.cc
     GTY(()) tree nonstandard_boolean_type_cache[200/*!MAX_BOOL_CACHED_PREC + 1 200足够大*/];


     uint64_t tree_code_counts[MAX_TREE_CODES];
     uint64_t tree_node_counts[(int) all_kinds];
     uint64_t tree_node_sizes[(int) all_kinds];
     /* Hash table and temporary node for larger integer const values.  */
     //原型 int_cst_node tree.cc
     GTY (()) tree int_cst_node;


    //原型 #define POINTER_SIZE BITS_PER_WORD
    int pointerSize;
    //原型 #define FLOAT_TYPE_SIZE BITS_PER_WORD
    int floatTypeSize;
    //原型 #define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
    int doubleTypeSize;
    //原型 #define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
    int longDoubleTypeSize;
    //原型 #define POINTER_SIZE_UNITS ((POINTER_SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT)
    int pointerSizeUnits;
    //原型 nonstandard_integer_type_cache tree.cc
    tree nonstandard_integer_type_cache[200/*!2 * MAX_INT_CACHED_PREC + 2*/];

    //#ifdef HAVE_BFmode
    void (*createTreeForBFmode)(MtcsTree *self);
    //原型  struct lang_hooks...tree (*builtin_function) (tree decl);#define LANG_HOOKS_BUILTIN_FUNCTION lhd_builtin_function
    tree (*builtin_function)(MtcsTree *self,tree decl);
    void *backup;//备份主机的rtl
};

//原型 #define size_int(L) size_int_kind (L, stk_sizetype) tree.h
#define mtcs_tree_size_int(SELF,L) mtcs_tree_size_int_kind (SELF,L, stk_sizetype)
//原型  #define ssize_int(L) size_int_kind (L, stk_ssizetype) tree.h
#define mtcs_tree_ssize_int(SELF,L) mtcs_tree_size_int_kind (SELF,L, stk_ssizetype)
//原型 #define bitsize_int(L) size_int_kind (L, stk_bitsizetype) tree.h
#define mtcs_tree_bitsize_int(SELF,L) mtcs_tree_size_int_kind (SELF,L, stk_bitsizetype)
//原型 #define sbitsize_int(L) size_int_kind (L, stk_sbitsizetype) tree.h
#define mtcs_tree_sbitsize_int(SELF,L) mtcs_tree_size_int_kind (SELF,L, stk_sbitsizetype)

//延迟到 开始编译时在初始化,早了会被gcc 释放。引起 ggc_freed 问题
void mtcs_tree_init(MtcsTree *self);
void mtcs_tree_set_int_type_size(MtcsTree *self,int size);
void mtcs_tree_set_long_type_size(MtcsTree *self,int size);
void mtcs_tree_set_long_long_type_size(MtcsTree *self,int size);
void mtcs_tree_set_short_type_size(MtcsTree *self,int size);
//原型 #define SIZETYPE SIZE_TYPE  defaults.h SIZE_TYPE 每个平台定义不一样
void mtcs_tree_set_sizetype_string(MtcsTree *self, const char *sizeTypeStr);
//#define PTRDIFF_TYPE (TARGET_ABI64 ? "long int" : "int")
void mtcs_tree_set_prt_diff_type_string(MtcsTree *self, const char *prtDiffTypeStr);
//原型 #define POINTER_SIZE BITS_PER_WORD
int mtcs_tree_get_pointer_size(MtcsTree *self);
void mtcs_tree_set_pointer_size(MtcsTree *self, int size);
//原型 #define FLOAT_TYPE_SIZE BITS_PER_WORD
int mtcs_tree_get_float_type_size(MtcsTree *self);
void mtcs_tree_set_float_type_size(MtcsTree *self, int size);
//原型 #define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
int mtcs_tree_get_double_type_size(MtcsTree *self);
void mtcs_tree_set_double_type_size(MtcsTree *self, int size);
//原型 #define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
int mtcs_tree_get_long_double_type_size(MtcsTree *self);
void mtcs_tree_set_long_double_type_size(MtcsTree *self, int size);
//原型 #define POINTER_SIZE_UNITS ((POINTER_SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT)
int mtcs_tree_get_pointer_size_units(MtcsTree *self);
void mtcs_tree_set_pointer_size_units(MtcsTree *self, int size);
//原型 build_common_tree_nodes tree.h tree.cc
void mtcs_tree_build_common_tree_nodes (MtcsTree *self,bool signed_char);
//原型 lto_build_c_type_nodes lto-lang.cc
void mtcs_tree_build_c_type_nodes (MtcsTree *self);
//原型 lto_define_builtins lto-lang.cc
void mtcs_tree_define_builtins (MtcsTree *self,tree va_list_ref_type_node ATTRIBUTE_UNUSED,
           tree va_list_arg_type_node ATTRIBUTE_UNUSED);
//原型 add_builtin_function langhooks.h langhooks.cc
tree mtcs_tree_add_builtin_function (MtcsTree *self,const char *name, tree type, int function_code,
            enum built_in_class cl, const char *library_name,tree attrs);
//原型 set_builtin_decl tree.h
void mtcs_tree_set_builtin_decl (MtcsTree *self,enum built_in_function fncode, tree decl, bool implicit_p);
//原型 build_common_builtin_nodes tree.h tree.cc
void mtcs_tree_build_common_builtin_nodes (MtcsTree *self);
//原型 builtin_decl_explicit_p tree.h
bool mtcs_tree_builtin_decl_explicit_p (MtcsTree *self,enum built_in_function fncode);
//原型 size_int_kind fold-const.h fold-const.hh
tree mtcs_tree_size_int_kind(MtcsTree *self,poly_int64 number, enum size_type_kind kind);
//原型 size_int fold-const.h fold-const.hh
tree  mtcs_tree_get_size_int(MtcsTree *self,int size);
tree  mtcs_tree_get_size_int(MtcsTree *self,poly_int64 size);
//原型 bitsize_int fold-const.h fold-const.hh
tree  mtcs_tree_get_bitsize_int(MtcsTree *self,int size);

//原型 lto_type_for_mode lto-lang.cc #define LANG_HOOKS_TYPE_FOR_MODE lto_type_for_mode
tree mtcs_tree_type_for_mode(MtcsTree *self,machine_mode mode, int unsigned_p);
//原型 #define LANG_HOOKS_UNIT_SIZE_WITHOUT_REUSABLE_PADDING lhd_unit_size_without_reusable_padding
tree mtcs_tree_unit_size_without_reusable_padding(MtcsTree *self,tree t);


//原型 get_qualified_type tree.h tree.cc
tree mtcs_tree_get_qualified_type (MtcsTree *self,tree type, int type_quals);
//原型 check_qualified_type tree.h tree.cc
bool mtcs_tree_check_qualified_type (MtcsTree *self,const_tree cand, const_tree base, int type_quals);
//原型 check_base_type tree.h tree.cc
bool mtcs_tree_check_base_type (MtcsTree *self,const_tree cand, const_tree base);
//原型 check_lang_type tree.h tree.cc
bool mtcs_tree_check_lang_type (MtcsTree *self,const_tree cand, const_tree base);
//原型 build_decl tree.h tree.cc
tree mtcs_tree_build_decl(MtcsTree *self,location_t loc, enum tree_code code, tree name,
          tree type MEM_STAT_DECL);
//原型 build_reference_type_for_mode tree.h tree.cc
tree mtcs_tree_build_reference_type_for_mode (MtcsTree *self,tree to_type, machine_mode mode,bool can_alias_all);
//原型 build_reference_type tree.h tree.cc
tree mtcs_tree_build_reference_type (MtcsTree *self,tree to_type);
//原型 build_pointer_type tree.h tree.cc
tree mtcs_tree_build_pointer_type (MtcsTree *self,tree to_type);
//原型 build_pointer_type_for_mode tree.h tree.cc
tree mtcs_tree_build_pointer_type_for_mode (MtcsTree *self,tree to_type, machine_mode mode,
              bool can_alias_all);
//原型 build_type_variant tree.h
tree mtcs_tree_build_type_variant(MtcsTree *self,tree type,int const_p,int volatile_p);
//原型 build_qualified_type tree.h tree.cc
tree mtcs_tree_build_qualified_type(MtcsTree *self,tree type, int type_quals MEM_STAT_DECL);
//原型 build_function_type_array tree.h tree.cc
tree mtcs_tree_build_function_type_array (MtcsTree *self,tree return_type, int n, tree *arg_types);
//原型 build_varargs_function_type_array tree.h tree.cc
tree mtcs_tree_build_varargs_function_type_array (MtcsTree *self,tree return_type, int n, tree *arg_types);
//原型 build_function_type tree.h tree.cc
tree mtcs_tree_build_function_type (MtcsTree *self,tree value_type, tree arg_types,
      bool no_named_args_stdarg_p=false);
//原型 type_hash_canon tree.h tree.cc
tree mtcs_tree_type_hash_canon (MtcsTree *self,unsigned int hashcode, tree type);
//原型 build_int_cst tree.h tree.cc
tree mtcs_tree_build_int_cst (MtcsTree *self,tree type, poly_int64 cst);
//原型 build_vector_type_for_mode tree.h tree.cc
tree mtcs_tree_build_vector_type_for_mode (MtcsTree *self,tree innertype, machine_mode mode);
//原型 build_vector_type tree.h tree.cc
tree mtcs_tree_build_vector_type (MtcsTree *self,tree innertype, poly_int64 nunits);
//原型 build_nonstandard_boolean_type tree.h tree.cc
tree mtcs_tree_build_nonstandard_boolean_type (MtcsTree *self,unsigned HOST_WIDE_INT precision);
//原型 build_complex_type tree.h tree.cc
tree mtcs_tree_build_complex_type (MtcsTree *self,tree component_type, bool named=false);
//原型 build_complex tree.h tree.cc
tree mtcs_tree_build_complex (MtcsTree *self,tree type, tree real, tree imag);
//原型 build_function_type_list tree.h tree.cc
tree mtcs_tree_build_function_type_list (MtcsTree *self,tree return_type, ...);

//原型 wide_int_to_tree tree.h tree.cc
tree mtcs_tree_wide_int_to_tree (MtcsTree *self,tree type, const poly_wide_int_ref &value);
//原型 build_poly_int_cst tree.h tree.cc
tree mtcs_tree_build_poly_int_cst (MtcsTree *self,tree type, const poly_wide_int_ref &values);
//原型 type_hash_canon_hash tree.h tree.cc
hashval_t mtcs_tree_type_hash_canon_hash (MtcsTree *self,tree type);
//原型 error_operand_p tree.h
bool mtcs_tree_error_operand_p (MtcsTree *self,const_tree t);
//原型 builtin_decl_explicit tree.h
tree mtcs_tree_builtin_decl_explicit (MtcsTree *self,enum built_in_function fncode);
//原型 force_fit_type tree.h tree.cc
tree mtcs_tree_force_fit_type (MtcsTree *self,tree type, const poly_wide_int_ref &cst,
      int overflowable, bool overflowed);
//原型 component_ref_field_offset tree.h tree.cc
tree mtcs_tree_component_ref_field_offset (MtcsTree *self,tree exp);
//原型 build_simple_mem_ref_loc fold-const.h tree.cc
tree mtcs_tree_build_simple_mem_ref_loc (MtcsTree *self,location_t loc, tree ptr);
//原型 build_simple_mem_ref fold-const.h
tree mtcs_tree_build_simple_mem_ref(MtcsTree *self,tree ptr);
//原型 double_int_to_tree tree.h tree.cc
tree mtcs_tree_double_int_to_tree (MtcsTree *self,tree type, double_int cst);
//原型 build_real tree.h tree.cc
tree mtcs_tree_build_real (MtcsTree *self,tree type, REAL_VALUE_TYPE d);
//原型 build_real_from_int_cst tree.h tree.cc
tree mtcs_tree_build_real_from_int_cst (MtcsTree *self,tree type, const_tree i);
//原型 array_ref_low_bound tree.h tree.cc
tree mtcs_tree_array_ref_low_bound (MtcsTree *self,tree exp);
//原型 array_ref_element_size tree.h tree.cc
tree mtcs_tree_array_ref_element_size (MtcsTree *self,tree exp);
//原型 build_nonstandard_integer_type tree.h tree.cc
tree mtcs_tree_build_nonstandard_integer_type (MtcsTree *self,unsigned HOST_WIDE_INT precision,
            int unsignedp);
//原型 type_has_mode_precision_p  tree.h 
bool mtcs_tree_type_has_mode_precision_p (MtcsTree *self,const_tree t);
//原型 builtin_decl_unreachable tree.h tree.cc
tree mtcs_tree_builtin_decl_unreachable(MtcsTree *self);
//重载函数 builtin_decl_unreachable
tree mtcs_tree_builtin_decl_unreachable(MtcsTree *self,location_t loc);

//原型 build_call_expr tree.h tree.cc
tree mtcs_tree_build_call_expr (MtcsTree *self,tree fndecl, int n, ...);
//原型 build_call_expr_loc_array tree.h tree.cc
tree mtcs_tree_build_call_expr_loc_array (MtcsTree *self,location_t loc, tree fndecl, int n, tree *argarray);
//原型 build_zero_cst tree.h tree.cc
tree mtcs_tree_build_zero_cst (MtcsTree *self,tree type);

void mtcs_tree_debug_tree(MtcsTree *self,int passType,char *name);
//备份主机的 builtin_info 只能在创建完mtcs的builtin后才能备份
void mtcs_tree_backup_builtin_info(MtcsTree *self);
//恢复主机的 builtin_info
void mtcs_tree_restore_builtin_info(MtcsTree *self);

#endif


