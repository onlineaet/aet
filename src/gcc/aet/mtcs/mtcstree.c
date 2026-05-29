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
 * base on tree.cc
 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" // remap_debug_filename()
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"
#include "langhooks-def.h"
#include "gimple.h"
#include "dfp.h"
#include "ubsan.h"


#include "mtcstree.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"

static int flag_isoc2y=0;//原来声明在c-common.h mtcsbuiltins.c不能引入c-ommon.h。所以定义在这里供builtin.def引用


typedef struct _MtcsTreeBackup
{
   //备份主机的 global_trees
  GTY(()) tree back_global_trees[TI_MAX];
  //备份主机的 integer_types
  GTY(()) tree back_integer_types[itk_none];
  //备份主机的 sizetype_tab
  GTY(()) tree back_sizetype_tab[(int) stk_type_kind_last];
  //备分主机的 tree c_global_trees[CTI_MAX];只备份6个tree 它们是:
  //string_type_node; const_string_type_node; wint_type_node;
 //intmax_type_node;  uintmax_type_node; signed_size_type_node;
  GTY(()) tree back_c_global_trees[6];

  //原型 targetm.floatn_builtin_p
  void *back_targetm_floatn_builtin_p;
  //原型 targetm.libc_has_function
  void *back_targetm_libc_has_function;
  //原型 targetm.have_tls
  bool back_targetm_have_tls;
  //原型  targetm.emutls.get_address #define TARGET_EMUTLS_GET_ADDRESS "__builtin___emutls_get_address"
  char *back_targetm_emutls_get_address;
  //原型  targetm.emutls.register_common, #define TARGET_EMUTLS_REGISTER_COMMON "__builtin___emutls_register_common"
  char *back_targetm_emutls_register_common;
  //原型 targetm.unwind_word_mode
  void *back_targetm_unwind_word_mode;
  //原型 (*lang_hooks.types.type_for_mode)
  void *back_lang_hooks_types_type_for_mode;
  //为了在mtcs调用tree.cc中的build1 需要把recompute_tree_invariant_for_addr_expr中调用的方法
  //node = lang_hooks.expr_to_decl (node, &tc, &se); 指向mtcs中，所以在mtcstree.c的backup restore中加入对lang_hooks的操用
  void *back_lang_hooks_expr_to_decl;
  //备份主机的 extern GTY(()) builtin_info_type builtin_info[(int)END_BUILTINS];
  builtin_info_type back_builtin_info[(int)END_BUILTINS];
}MtcsTreeBackup;

//不需要layout_type layout_decl的方法
//1.build_variant_type_copy  tree.cc 调用 build_distinct_type_copy
//2.build_distinct_type_copy tree.cc
//3.build_atomic_base        tree.cc 调用 build_variant_type_copy
//4.build_tree_list

/* Returns true iff the types are equivalent.  */
bool mtcs_type_cache_hasher::equal (mtcs_type_hash *a, mtcs_type_hash *b)
{
   /* First test the things that are the same for all types.  */
   if (a->hash != b->hash
   || TREE_CODE (a->type) != TREE_CODE (b->type)
   || TREE_TYPE (a->type) != TREE_TYPE (b->type)
   || !attribute_list_equal (TYPE_ATTRIBUTES (a->type), TYPE_ATTRIBUTES (b->type))
   || (TREE_CODE (a->type) != COMPLEX_TYPE && TYPE_NAME (a->type) != TYPE_NAME (b->type)))
      return false;

   /* Be careful about comparing arrays before and after the element type
   has been completed; don't compare TYPE_ALIGN unless both types are
   complete.  */
   if (COMPLETE_TYPE_P (a->type) && COMPLETE_TYPE_P (b->type)
   && (TYPE_ALIGN (a->type) != TYPE_ALIGN (b->type)
   || TYPE_MODE (a->type) != TYPE_MODE (b->type)))
      return false;

   switch (TREE_CODE (a->type)){
      case VOID_TYPE:
      case OPAQUE_TYPE:
      case COMPLEX_TYPE:
      case POINTER_TYPE:
      case REFERENCE_TYPE:
      case NULLPTR_TYPE:
      return true;

      case VECTOR_TYPE:
         return known_eq (TYPE_VECTOR_SUBPARTS (a->type),TYPE_VECTOR_SUBPARTS (b->type));

      case ENUMERAL_TYPE:
         if (TYPE_VALUES (a->type) != TYPE_VALUES (b->type)
         && !(TYPE_VALUES (a->type)
         && TREE_CODE (TYPE_VALUES (a->type)) == TREE_LIST
         && TYPE_VALUES (b->type)
         && TREE_CODE (TYPE_VALUES (b->type)) == TREE_LIST
         && type_list_equal (TYPE_VALUES (a->type),
         TYPE_VALUES (b->type))))
         return false;

      /* fall through */
      case INTEGER_TYPE:
      case REAL_TYPE:
      case BOOLEAN_TYPE:
         if (TYPE_PRECISION (a->type) != TYPE_PRECISION (b->type))
            return false;
         return ((TYPE_MAX_VALUE (a->type) == TYPE_MAX_VALUE (b->type)
         || tree_int_cst_equal (TYPE_MAX_VALUE (a->type),TYPE_MAX_VALUE (b->type)))
         && (TYPE_MIN_VALUE (a->type) == TYPE_MIN_VALUE (b->type)
         || tree_int_cst_equal (TYPE_MIN_VALUE (a->type),TYPE_MIN_VALUE (b->type))));

      case BITINT_TYPE:
         if (TYPE_PRECISION (a->type) != TYPE_PRECISION (b->type))
            return false;
         return TYPE_UNSIGNED (a->type) == TYPE_UNSIGNED (b->type);

      case FIXED_POINT_TYPE:
         return TYPE_SATURATING (a->type) == TYPE_SATURATING (b->type);

      case OFFSET_TYPE:
         return TYPE_OFFSET_BASETYPE (a->type) == TYPE_OFFSET_BASETYPE (b->type);

      case METHOD_TYPE:
         if (TYPE_METHOD_BASETYPE (a->type) == TYPE_METHOD_BASETYPE (b->type)
         && (TYPE_ARG_TYPES (a->type) == TYPE_ARG_TYPES (b->type)
         || (TYPE_ARG_TYPES (a->type)
         && TREE_CODE (TYPE_ARG_TYPES (a->type)) == TREE_LIST
         && TYPE_ARG_TYPES (b->type)
         && TREE_CODE (TYPE_ARG_TYPES (b->type)) == TREE_LIST
         && type_list_equal (TYPE_ARG_TYPES (a->type),
         TYPE_ARG_TYPES (b->type)))))
            break;
         return false;
      case ARRAY_TYPE:
         /* Don't compare TYPE_TYPELESS_STORAGE flag on aggregates,
         where the flag should be inherited from the element type
         and can change after ARRAY_TYPEs are created; on non-aggregates
         compare it and hash it, scalars will never have that flag set
         and we need to differentiate between arrays created by different
         front-ends or middle-end created arrays.  */
         return (TYPE_DOMAIN (a->type) == TYPE_DOMAIN (b->type)
         && (AGGREGATE_TYPE_P (TREE_TYPE (a->type))
         || (TYPE_TYPELESS_STORAGE (a->type)== TYPE_TYPELESS_STORAGE (b->type))));

      case RECORD_TYPE:
      case UNION_TYPE:
      case QUAL_UNION_TYPE:
         return (TYPE_FIELDS (a->type) == TYPE_FIELDS (b->type)
         || (TYPE_FIELDS (a->type)
         && TREE_CODE (TYPE_FIELDS (a->type)) == TREE_LIST
         && TYPE_FIELDS (b->type)
         && TREE_CODE (TYPE_FIELDS (b->type)) == TREE_LIST
         && type_list_equal (TYPE_FIELDS (a->type), TYPE_FIELDS (b->type))));

      case FUNCTION_TYPE:
         if ((TYPE_ARG_TYPES (a->type) == TYPE_ARG_TYPES (b->type)
         && (TYPE_NO_NAMED_ARGS_STDARG_P (a->type) == TYPE_NO_NAMED_ARGS_STDARG_P (b->type)))
         || (TYPE_ARG_TYPES (a->type)
         && TREE_CODE (TYPE_ARG_TYPES (a->type)) == TREE_LIST
         && TYPE_ARG_TYPES (b->type)
         && TREE_CODE (TYPE_ARG_TYPES (b->type)) == TREE_LIST
         && type_list_equal (TYPE_ARG_TYPES (a->type),TYPE_ARG_TYPES (b->type))))
            break;
         return false;

      default:
         return false;
   }
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);
   if (mtcsLang/*!lang_hooks.types.type_hash_eq*/->types.type_hash_eq != NULL)
      return mtcsLang/*!lang_hooks.types.type_hash_eq*/->types.type_hash_eq(mtcsLang,a->type, b->type);

   return true;
}


/* Return the hash code X, an INTEGER_CST.  */
hashval_t mtcs_int_cst_hasher::hash (tree x)
{
  const_tree const t = x;
  hashval_t code = TYPE_UID (TREE_TYPE (t));
  int i;

  for (i = 0; i < TREE_INT_CST_NUNITS (t); i++)
    code = iterative_hash_host_wide_int (TREE_INT_CST_ELT(t, i), code);

  return code;
}

/* Return nonzero if the value represented by *X (an INTEGER_CST tree node)
   is the same as that given by *Y, which is the same.  */

bool mtcs_int_cst_hasher::equal (tree x, tree y)
{
  const_tree const xt = x;
  const_tree const yt = y;

  if (TREE_TYPE (xt) != TREE_TYPE (yt)
      || TREE_INT_CST_NUNITS (xt) != TREE_INT_CST_NUNITS (yt)
      || TREE_INT_CST_EXT_NUNITS (xt) != TREE_INT_CST_EXT_NUNITS (yt))
    return false;

  for (int i = 0; i < TREE_INT_CST_NUNITS (xt); i++)
    if (TREE_INT_CST_ELT (xt, i) != TREE_INT_CST_ELT (yt, i))
      return false;

  return true;
}

hashval_t mtcs_poly_int_cst_hasher::hash (tree t)
{
  inchash::hash hstate;

  hstate.add_int (TYPE_UID (TREE_TYPE (t)));
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
    hstate.add_wide_int (wi::to_wide (POLY_INT_CST_COEFF (t, i)));

  return hstate.end ();
}

bool mtcs_poly_int_cst_hasher::equal (tree x, const compare_type &y)
{
  if (TREE_TYPE (x) != y.first)
    return false;
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
    if (wi::to_wide (POLY_INT_CST_COEFF (x, i)) != y.second->coeffs[i])
      return false;
  return true;
}

#define TYPE_HASH_INITIAL_SIZE 1000

static   void backup_cb(MtcsBackupRestore *iface);
static   void restore_cb(MtcsBackupRestore *iface);

void  mtcs_tree_init(MtcsTree *self)
{
   self->mtcsBackupRestore.backup=backup_cb;
   self->mtcsBackupRestore.restore=restore_cb;
   self->mtcsBackupRestore.impl=(npointer)self;
   self->backup=(void*) n_slice_alloc0 (sizeof(MtcsTreeBackup));
//   floatn_type_info floatn_nx_types[NUM_FLOATN_NX_TYPES];
//   /* Vector of standard trees used by the C compiler.  */
//   GTY(()) tree global_trees[TI_MAX];
//
//   /* The standard C integer types.  Use integer_type_kind to index into
//      this array.  */
//   GTY(()) tree integer_types[itk_none];
//
//   /* Types used to represent sizes.  */
//   GTY(()) tree sizetype_tab[(int) stk_type_kind_last];
   n_debug("mtcstree.c mtcsTreeInit 00 NUM_FLOATN_NX_TYPES:%d TI_MAX:%d itk_none:%d stk_type_kind_last:%d\n",
         NUM_FLOATN_NX_TYPES,TI_MAX,itk_none,stk_type_kind_last);
   //原型 next_type_uid tree.cc
   self->next_type_uid=1;
   /* Initialize the hash table of types.  */
   self->type_hash_table = hash_table<mtcs_type_cache_hasher>::create_ggc (TYPE_HASH_INITIAL_SIZE);
   self->int_cst_hash_table = hash_table<mtcs_int_cst_hasher>::create_ggc (1024);
   self->poly_int_cst_hash_table = hash_table<mtcs_poly_int_cst_hasher>::create_ggc (64);
   self->int_cst_node = make_int_cst (1, 1);
}

/* Set the type qualifiers for TYPE to TYPE_QUALS, which is a bitmask
   of the various TYPE_QUAL values.  */
//原型 set_type_quals tree.cc
static void set_type_quals (tree type, int type_quals)
{
  TYPE_READONLY (type) = (type_quals & TYPE_QUAL_CONST) != 0;
  TYPE_VOLATILE (type) = (type_quals & TYPE_QUAL_VOLATILE) != 0;
  TYPE_RESTRICT (type) = (type_quals & TYPE_QUAL_RESTRICT) != 0;
  TYPE_ATOMIC (type) = (type_quals & TYPE_QUAL_ATOMIC) != 0;
  TYPE_ADDR_SPACE (type) = DECODE_QUAL_ADDR_SPACE (type_quals);
}


/* Create an atomic variant node for TYPE.  This routine is called
   during initialization of data types to create the 5 basic atomic
   types.  The generic build_variant_type function requires these to
   already be set up in order to function properly, so cannot be
   called from there.  If ALIGN is non-zero, then ensure alignment is
   overridden to this value.  */
//原型 build_atomic_base tree.cc
static tree build_atomic_base (MtcsTree *self,tree type, unsigned int align)
{
  tree t;
  /* Make sure its not already registered.  */
  if ((t = mtcs_tree_get_qualified_type/*!get_qualified_type*/(self,type, TYPE_QUAL_ATOMIC)))
    return t;
  t = build_variant_type_copy (type); //不需要layout_type layout_decl
  set_type_quals (t, TYPE_QUAL_ATOMIC);
  if (align)
    SET_TYPE_ALIGN (t, align);

  return t;
}

/* This function checks to see if TYPE matches the size one of the built-in
   atomic types, and returns that core atomic type.  */
//原型 find_atomic_core_type tree.cc
static tree find_atomic_core_type (MtcsTree *self,const_tree type)
{
   MtcsTree *mtcsTree=self;
   tree base_atomic_type;

   /* Only handle complete types.  */
   if (!tree_fits_uhwi_p (TYPE_SIZE (type)))
      return NULL_TREE;

   switch (tree_to_uhwi (TYPE_SIZE (type))){
      case 8:
         base_atomic_type = mtcs_atomicQI_type_node;
         break;
      case 16:
         base_atomic_type = mtcs_atomicHI_type_node;
         break;
      case 32:
         base_atomic_type = mtcs_atomicSI_type_node;
         break;
      case 64:
         base_atomic_type = mtcs_atomicDI_type_node;
         break;
      case 128:
         base_atomic_type = mtcs_atomicTI_type_node;
         break;
      default:
         base_atomic_type = NULL_TREE;
   }

   return base_atomic_type;
}

/* Returns true iff unqualified CAND and BASE are equivalent.  */
//原型 check_base_type tree.h tree.cc
bool mtcs_tree_check_base_type (MtcsTree *self,const_tree cand, const_tree base)
{
   if (TYPE_NAME (cand) != TYPE_NAME (base)
   /* Apparently this is needed for Objective-C.  */
   || TYPE_CONTEXT (cand) != TYPE_CONTEXT (base)
   || !attribute_list_equal (TYPE_ATTRIBUTES (cand),TYPE_ATTRIBUTES (base)))
      return false;
   /* Check alignment.  */
   if (TYPE_ALIGN (cand) == TYPE_ALIGN (base) && TYPE_USER_ALIGN (cand) == TYPE_USER_ALIGN (base))
      return true;
   /* Atomic types increase minimal alignment.  We must to do so as well
   or we get duplicated canonical types. See PR88686.  */
   if ((TYPE_QUALS (cand) & TYPE_QUAL_ATOMIC)){
      /* See if this object can map to a basic atomic type.  */
      tree atomic_type = find_atomic_core_type(self,cand);
      if (atomic_type && TYPE_ALIGN (atomic_type) == TYPE_ALIGN (cand))
         return true;
   }
   return false;
}

/* Returns true iff CAND and BASE have equivalent language-specific
   qualifiers.  */
//原型 check_lang_type tree.h tree.cc
bool mtcs_tree_check_lang_type (MtcsTree *self,const_tree cand, const_tree base)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsLang   *mtcsLang=mtcs_target_get_lang(mtcsTarget);

   if (mtcsLang/*!lang_hooks.types.type_hash_eq*/->types.type_hash_eq== NULL)
      return true;
   /* type_hash_eq currently only applies to these types.  */
   if (TREE_CODE (cand) != FUNCTION_TYPE  && TREE_CODE (cand) != METHOD_TYPE)
      return true;
   return mtcsLang/*!lang_hooks.types.type_hash_eq*/->types.type_hash_eq(mtcsLang,cand, base);
}


/* Returns true iff CAND is equivalent to BASE with TYPE_QUALS.  */
//原型 check_qualified_type tree.h tree.cc
bool mtcs_tree_check_qualified_type (MtcsTree *self,const_tree cand, const_tree base, int type_quals)
{
  return (TYPE_QUALS (cand) == type_quals
     && mtcs_tree_check_base_type/*!check_base_type*/(self,cand, base)
     && mtcs_tree_check_lang_type/*!check_lang_type*/(self,cand, base));
}

/* Return a version of the TYPE, qualified as indicated by the
   TYPE_QUALS, if one exists.  If no qualified version exists yet,
   return NULL_TREE.  */
//原型 get_qualified_type tree.h tree.cc
tree mtcs_tree_get_qualified_type (MtcsTree *self,tree type, int type_quals)
{
   if (TYPE_QUALS (type) == type_quals)
      return type;
   tree mv = TYPE_MAIN_VARIANT (type);
   if (mtcs_tree_check_qualified_type/*!check_qualified_type*/(self,mv, type, type_quals))
      return mv;

   /* Search the chain of variants to see if there is already one there just
   like the one we need to have.  If so, use that existing one.  We must
   preserve the TYPE_NAME, since there is code that depends on this.  */
   for (tree *tp = &TYPE_NEXT_VARIANT (mv); *tp; tp = &TYPE_NEXT_VARIANT (*tp))
      if (mtcs_tree_check_qualified_type/*!check_qualified_type*/(self,*tp, type, type_quals)){
         /* Put the found variant at the head of the variant list so
         frequently searched variants get found faster.  The C++ FE
         benefits greatly from this.  */
         tree t = *tp;
         *tp = TYPE_NEXT_VARIANT (t);
         TYPE_NEXT_VARIANT (t) = TYPE_NEXT_VARIANT (mv);
         TYPE_NEXT_VARIANT (mv) = t;
         return t;
      }

   return NULL_TREE;
}



/* Initialize sizetypes so layout_type can use them.  */
//原型 initialize_sizetypes stor-layout.h stor-layout.cc
//mtcs_stor_layout_make_signed_type中执行了layout_type
static void initializeSizeTypes (MtcsTree *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);

   int precision, bprecision;
   char *sizeTypeStr=self->sizeTypeStr;
   /* Get sizetypes precision from the SIZE_TYPE target macro.  */
   if (strcmp (sizeTypeStr/*!SIZETYPE*/, "unsigned int") == 0)
      precision =self->intTypeSize; //原型 INT_TYPE_SIZE; default.h 平台定义 nvptx.h
   else if (strcmp (sizeTypeStr/*!SIZETYPE*/, "long unsigned int") == 0)
      precision =self->longTypeSize; //原型 LONG_TYPE_SIZE; default.h 平台定义 nvptx.h  ;
   else if (strcmp (sizeTypeStr/*!SIZETYPE*/, "long long unsigned int") == 0)
      precision = self->longLongTypeSize; //原型 LONG_LONG_TYPE_SIZE; default.h 平台定义 nvptx.h  ;
   else if (strcmp (sizeTypeStr/*!SIZETYPE*/, "short unsigned int") == 0)
      precision = self->shortTypeSize; //原型 SHORT_TYPE_SIZE; default.h 平台定义 nvptx.h  ;
   else{
      int i;
      precision = -1;
      for (i = 0; i <mtcsMode->mtcs_NUM_INT_N_ENTS/*!NUM_INT_N_ENTS*/; i++)
         if (mtcsMode->int_n_enabled_p/*!int_n_enabled_p*/[i]){
            char name[50], altname[50];
            sprintf (name, "__int%d unsigned",mtcsMode->intData/*!int_n_data*/[i].bitsize);
            sprintf (altname, "__int%d__ unsigned", mtcsMode->intData/*!int_n_data*/[i].bitsize);
            if (strcmp (name, sizeTypeStr/*!SIZETYPE*/) == 0  || strcmp (altname, sizeTypeStr/*!SIZETYPE*/) == 0){
               precision = mtcsMode->intData/*!int_n_data*/[i].bitsize;
            }
         }
      if (precision == -1)
         gcc_unreachable ();
   }

   bprecision = MIN (precision + LOG2_BITS_PER_UNIT + 1, mtcs_mode_get_max_fixed_size/*!MAX_FIXED_MODE_SIZE*/(mtcsMode));
   bprecision = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,
         mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,bprecision));
   if (bprecision > HOST_BITS_PER_DOUBLE_INT)
      bprecision = HOST_BITS_PER_DOUBLE_INT;

   /* Create stubs for sizetype and bitsizetype so we can create constants.  */
   tree sizetype0 = make_node (INTEGER_TYPE);
   TYPE_NAME (sizetype0) = get_identifier ("sizetype");
   TYPE_PRECISION (sizetype0) = precision;
   TYPE_UNSIGNED (sizetype0) = 1;
   self->sizetype_tab[(int) stk_sizetype] = sizetype0;/*!sizetype tree.h中定义*/

   tree bitsizetype0 = make_node (INTEGER_TYPE);
   TYPE_NAME (bitsizetype0) = get_identifier ("bitsizetype");
   TYPE_PRECISION (bitsizetype0) = bprecision;
   TYPE_UNSIGNED (bitsizetype0) = 1;

   self->sizetype_tab[(int) stk_bitsizetype] = bitsizetype0;/*!bitsizetype tree.h中定义*/

   /* Now layout both types manually.  */
   scalar_int_mode mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,precision);
   SET_TYPE_MODE (sizetype0, mode);
   SET_TYPE_ALIGN (sizetype0, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,TYPE_MODE (sizetype0)));
   TYPE_SIZE (sizetype0) =mtcs_tree_bitsize_int/*!bitsize_int*/(self,precision);
   TYPE_SIZE_UNIT (sizetype0) = mtcs_tree_size_int/*!size_int*/(self,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
   set_min_and_max_values_for_integral_type (sizetype0, precision, UNSIGNED);
   n_debug("mtcstree.c initializeSizeTypes 00 sizetype:%p %d\n",sizetype0,mode);

   mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,bprecision);
   SET_TYPE_MODE (bitsizetype0, mode);
   SET_TYPE_ALIGN (bitsizetype0, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,TYPE_MODE (bitsizetype0)));
   TYPE_SIZE (bitsizetype0) = mtcs_tree_bitsize_int/*!bitsize_int*/(self,bprecision);
   TYPE_SIZE_UNIT (bitsizetype0) = mtcs_tree_size_int/*!size_int*/(self,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
   set_min_and_max_values_for_integral_type (bitsizetype0, bprecision, UNSIGNED);
   n_debug("mtcstree.c initializeSizeTypes 11 bitsizetype0:%p %d\n",bitsizetype0,mode);

   /* Create the signed variants of *sizetype.  */

   tree ssizetype0 = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,TYPE_PRECISION (sizetype0));
   TYPE_NAME (ssizetype0) = get_identifier ("ssizetype");
   self->sizetype_tab[(int) stk_ssizetype] = ssizetype0;/*!ssizetype tree.h中定义*/


   tree sbitsizetype0 = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,TYPE_PRECISION (bitsizetype0));
   TYPE_NAME (sbitsizetype0) = get_identifier ("sbitsizetype");
   self->sizetype_tab[(int) stk_sbitsizetype] = sbitsizetype0;/*!ssizetype tree.h中定义*/
}

//原型 make_or_reuse_type tree.cc
static tree make_or_reuse_type (MtcsTree *self,unsigned size, int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;

   int i;
   if (size == INT_TYPE_SIZE)
      return unsignedp ? mtcs_unsigned_type_node : mtcs_integer_type_node;
   if (size == CHAR_TYPE_SIZE)
      return unsignedp ? mtcs_unsigned_char_type_node : mtcs_signed_char_type_node;
   if (size == SHORT_TYPE_SIZE)
      return unsignedp ? mtcs_short_unsigned_type_node : mtcs_short_integer_type_node;
   if (size == LONG_TYPE_SIZE)
      return unsignedp ? mtcs_long_unsigned_type_node : mtcs_long_integer_type_node;
   if (size == LONG_LONG_TYPE_SIZE)
      return (unsignedp ? mtcs_long_long_unsigned_type_node : mtcs_long_long_integer_type_node);

   for (i = 0; i <mtcsMode->mtcs_NUM_INT_N_ENTS/*!NUM_INT_N_ENTS*/; i ++)
      if (size == mtcsMode->intData/*!int_n_data*/[i].bitsize  && mtcsMode->int_n_enabled_p/*!int_n_enabled_p*/[i])
         return (unsignedp ? self->int_n_trees[i].unsigned_type  : self->int_n_trees[i].signed_type);

   if (unsignedp)
      return mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,size);
   else
      return mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,size);
}

/* Create or reuse a fract type by SIZE, UNSIGNEDP, and SATP.  */
//原型 make_or_reuse_fract_type tree.cc
static tree make_or_reuse_fract_type (MtcsTree *self,unsigned size, int unsignedp, int satp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;

   if (satp){
      if (size == SHORT_FRACT_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_short_fract_type_node : mtcs_sat_short_fract_type_node;
      if (size == FRACT_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_fract_type_node : mtcs_sat_fract_type_node;
      if (size == LONG_FRACT_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_long_fract_type_node : mtcs_sat_long_fract_type_node;
      if (size == LONG_LONG_FRACT_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_long_long_fract_type_node : mtcs_sat_long_long_fract_type_node;
   }else{
      if (size == SHORT_FRACT_TYPE_SIZE)
         return unsignedp ?  mtcs_unsigned_short_fract_type_node :  mtcs_short_fract_type_node;
      if (size == FRACT_TYPE_SIZE)
         return unsignedp ?  mtcs_unsigned_fract_type_node :  mtcs_fract_type_node;
      if (size == LONG_FRACT_TYPE_SIZE)
         return unsignedp ?  mtcs_unsigned_long_fract_type_node :  mtcs_long_fract_type_node;
      if (size == LONG_LONG_FRACT_TYPE_SIZE)
         return unsignedp ?  mtcs_unsigned_long_long_fract_type_node :  mtcs_long_long_fract_type_node;
   }

   return mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,size, unsignedp, satp);
}

/* Create or reuse an accum type by SIZE, UNSIGNEDP, and SATP.  */
//原型 make_or_reuse_accum_type tree.cc
static tree make_or_reuse_accum_type (MtcsTree *self,unsigned size, int unsignedp, int satp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;//为了引用mtcstree.h 定义的tree类型的宏

   if (satp){
      if (size == SHORT_ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_short_accum_type_node  : mtcs_sat_short_accum_type_node;
      if (size == ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_accum_type_node : mtcs_sat_accum_type_node;
      if (size == LONG_ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_long_accum_type_node : mtcs_sat_long_accum_type_node;
      if (size == LONG_LONG_ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_sat_unsigned_long_long_accum_type_node  : mtcs_sat_long_long_accum_type_node;
   }else{
      if (size == SHORT_ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_unsigned_short_accum_type_node  : mtcs_short_accum_type_node;
      if (size == ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_unsigned_accum_type_node : mtcs_accum_type_node;
      if (size == LONG_ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_unsigned_long_accum_type_node: mtcs_long_accum_type_node;
      if (size == LONG_LONG_ACCUM_TYPE_SIZE)
         return unsignedp ? mtcs_unsigned_long_long_accum_type_node : mtcs_long_long_accum_type_node;
   }

   return mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,size, unsignedp, satp);
}


#define MTCS_MAKE_FIXED_MODE_NODE(KIND,NAME,MODE) \
  mtcs_ ## NAME ##_type_node = make_or_reuse_## KIND ##_type(self,mtcs_mode_get_bitsize(mtcsMode,mtcsMode->modes.M_ ## MODE ## mode),0,0);\
  mtcs_u ## NAME ##_type_node = make_or_reuse_## KIND ##_type(self,mtcs_mode_get_bitsize(mtcsMode,mtcsMode->modes.M_ ## MODE ## mode),1,0);\
  mtcs_sat_ ## NAME ##_type_node = make_or_reuse_## KIND ##_type(self,mtcs_mode_get_bitsize(mtcsMode,mtcsMode->modes.M_ ## MODE ## mode),0,1);\
  mtcs_sat_u ## NAME ##_type_node = make_or_reuse_## KIND ##_type(self,mtcs_mode_get_bitsize(mtcsMode,mtcsMode->modes.M_ ## MODE ## mode),1,1);\

static void make_fixed_type_node_fract(MtcsTree *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree = self;

   mtcs_sat_short_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         SHORT_FRACT_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_short_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         SHORT_FRACT_TYPE_SIZE,1,1);

   mtcs_short_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         SHORT_FRACT_TYPE_SIZE,0,0);

   mtcs_unsigned_short_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         SHORT_FRACT_TYPE_SIZE,1,0);

   mtcs_sat_fract_type_node     = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         FRACT_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         FRACT_TYPE_SIZE,1,1);

   mtcs_fract_type_node              =mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         FRACT_TYPE_SIZE,0,0);

   mtcs_unsigned_fract_type_node        =mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         FRACT_TYPE_SIZE,1,0);

   mtcs_sat_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_FRACT_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_FRACT_TYPE_SIZE,1,1);

   mtcs_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_FRACT_TYPE_SIZE,0,0);

   mtcs_unsigned_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_FRACT_TYPE_SIZE,1,0);

   mtcs_sat_long_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_LONG_FRACT_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_long_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_LONG_FRACT_TYPE_SIZE,1,1);

   mtcs_long_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_LONG_FRACT_TYPE_SIZE,0,0);

   mtcs_unsigned_long_long_fract_type_node = mtcs_stor_layout_make_fract_type/*!make_fract_type*/(mtcsStorLayout,
         LONG_LONG_FRACT_TYPE_SIZE,1,0);
}

static void make_fixed_type_node_accum(MtcsTree *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree = self;

   mtcs_sat_short_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         SHORT_ACCUM_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_short_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         SHORT_ACCUM_TYPE_SIZE,1,1);

   mtcs_short_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         SHORT_ACCUM_TYPE_SIZE,0,0);

   mtcs_unsigned_short_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         SHORT_ACCUM_TYPE_SIZE,1,0);

   mtcs_sat_accum_type_node          = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         ACCUM_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_accum_type_node          = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         ACCUM_TYPE_SIZE,1,1);

   mtcs_accum_type_node                  =mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         ACCUM_TYPE_SIZE,0,0);

   mtcs_unsigned_accum_type_node       =mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         ACCUM_TYPE_SIZE,1,0);

   mtcs_sat_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout
         ,LONG_ACCUM_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_ACCUM_TYPE_SIZE,1,1);

   mtcs_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_ACCUM_TYPE_SIZE,0,0);

   mtcs_unsigned_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_ACCUM_TYPE_SIZE,1,0);

   mtcs_sat_long_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_LONG_ACCUM_TYPE_SIZE,0,1);

   mtcs_sat_unsigned_long_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_LONG_ACCUM_TYPE_SIZE,1,1);

   mtcs_long_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_LONG_ACCUM_TYPE_SIZE,0,0);

   mtcs_unsigned_long_long_accum_type_node = mtcs_stor_layout_make_accum_type/*!make_accum_type*/(mtcsStorLayout,
         LONG_LONG_ACCUM_TYPE_SIZE,1,0);
}


#define BUILTIN_STRUCTPTR_STR_FILE          "FILE"
#define BUILTIN_STRUCTPTR_STR_TM            "tm"
#define BUILTIN_STRUCTPTR_STR_FENV_T        "fenv_t"
#define BUILTIN_STRUCTPTR_STR_FEXCEPT_T     "fexcept_t"

/* List of pointer types used to declare builtins before we have seen their
   real declaration.

   Keep the size up to date in tree.h !  */
//原型
//const builtin_structptr_type builtin_structptr_types[6] =
//{
//  { fileptr_type_node, ptr_type_node, "FILE" },
//  { const_tm_ptr_type_node, const_ptr_type_node, "tm" },
//  { fenv_t_ptr_type_node, ptr_type_node, "fenv_t" },
//  { const_fenv_t_ptr_type_node, const_ptr_type_node, "fenv_t" },
//  { fexcept_t_ptr_type_node, ptr_type_node, "fexcept_t" },
//  { const_fexcept_t_ptr_type_node, const_ptr_type_node, "fexcept_t" }
//};
//c-decl.cc 方法static tree match_builtin_function_types 引用到 builtin_structptr_types[j].node
static void createBuiltinStructptrType(MtcsTree *self,tree ptr_type_node0,tree const_ptr_type_node0)
{
   //for (unsigned i = 0; i < ARRAY_SIZE (self->builtin_structptr_types); ++i)
     // self->builtin_structptr_types[i].node = self->builtin_structptr_types[i].base;
   n_debug("mtcstree.c createBuiltinStructptrType 00 %d\n",ARRAY_SIZE (self->builtin_structptr_types));
   tree fileptr_type_node0=ptr_type_node0;
   self->global_trees[TI_FILEPTR_TYPE] = fileptr_type_node0;
   self->builtin_structptr_types[0].node=fileptr_type_node0;
   self->builtin_structptr_types[0].base=fileptr_type_node0;
   self->builtin_structptr_types[0].str=BUILTIN_STRUCTPTR_STR_FILE;

   tree const_tm_ptr_type_node0=const_ptr_type_node0;
   self->global_trees[TI_CONST_TM_PTR_TYPE] = const_tm_ptr_type_node0;
   self->builtin_structptr_types[1].node=const_tm_ptr_type_node0;
   self->builtin_structptr_types[1].base=const_tm_ptr_type_node0;
   self->builtin_structptr_types[1].str=BUILTIN_STRUCTPTR_STR_TM;

   tree fenv_t_ptr_type_node0=ptr_type_node0;
   self->global_trees[TI_FENV_T_PTR_TYPE] = fenv_t_ptr_type_node0;
   self->builtin_structptr_types[2].node=fenv_t_ptr_type_node0;
   self->builtin_structptr_types[2].base=fenv_t_ptr_type_node0;
   self->builtin_structptr_types[2].str=BUILTIN_STRUCTPTR_STR_FENV_T;

   tree const_fenv_t_ptr_type_node0=const_ptr_type_node0;
   self->global_trees[TI_CONST_FENV_T_PTR_TYPE] = const_fenv_t_ptr_type_node0;
   self->builtin_structptr_types[3].node=const_fenv_t_ptr_type_node0;
   self->builtin_structptr_types[3].base=const_fenv_t_ptr_type_node0;
   self->builtin_structptr_types[3].str=BUILTIN_STRUCTPTR_STR_FENV_T;

   tree fexcept_t_ptr_type_node0=ptr_type_node0;
   self->global_trees[TI_FEXCEPT_T_PTR_TYPE] = fexcept_t_ptr_type_node0;
   self->builtin_structptr_types[4].node=fexcept_t_ptr_type_node0;
   self->builtin_structptr_types[4].base=fexcept_t_ptr_type_node0;
   self->builtin_structptr_types[4].str=BUILTIN_STRUCTPTR_STR_FEXCEPT_T;

   tree const_fexcept_t_ptr_type_node0=const_ptr_type_node0;
   self->global_trees[TI_CONST_FEXCEPT_T_PTR_TYPE] = const_fexcept_t_ptr_type_node0;
   self->builtin_structptr_types[5].node=const_fexcept_t_ptr_type_node0;
   self->builtin_structptr_types[5].base=const_fexcept_t_ptr_type_node0;
   self->builtin_structptr_types[5].str=BUILTIN_STRUCTPTR_STR_FEXCEPT_T;

}


/* Create nodes for all integer types (and error_mark_node) using the sizes
   of C datatypes.  SIGNED_CHAR specifies whether char is signed.  */
//原型 build_common_tree_nodes tree.h tree.cc
void mtcs_tree_build_common_tree_nodes (MtcsTree *self,bool signed_char)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;

   int i;
   //原型 #define error_mark_node        global_trees[TI_ERROR_MARK]
   mtcs_error_mark_node = make_node (ERROR_MARK);
   TREE_TYPE (mtcs_error_mark_node) = mtcs_error_mark_node;

   initializeSizeTypes/*!initialize_sizetypes*/(self);

   /* Define both `signed char' and `unsigned char'.  */
   //原型 #define signed_char_type_node      integer_types[itk_signed_char]
   mtcs_signed_char_type_node = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,CHAR_TYPE_SIZE);
   TYPE_STRING_FLAG (mtcs_signed_char_type_node) = 1;

   //原型 #define unsigned_char_type_node      integer_types[itk_unsigned_char]
   mtcs_unsigned_char_type_node = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,CHAR_TYPE_SIZE);
   TYPE_STRING_FLAG (mtcs_unsigned_char_type_node) = 1;

   /* Define `char', which is like either `signed char' or `unsigned char'
   but not the same as either.  */
   //原型 #define char_type_node       integer_types[itk_char]
   mtcs_char_type_node = (signed_char ? mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,
         CHAR_TYPE_SIZE): mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,CHAR_TYPE_SIZE));
   TYPE_STRING_FLAG (mtcs_char_type_node) = 1;

   //原型 #define short_integer_type_node      integer_types[itk_short]
   mtcs_short_integer_type_node = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,SHORT_TYPE_SIZE);

   //原型 #define short_unsigned_type_node integer_types[itk_unsigned_short]
   mtcs_short_unsigned_type_node = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,SHORT_TYPE_SIZE);

   //原型 #define integer_type_node     integer_types[itk_int]
   mtcs_integer_type_node = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,INT_TYPE_SIZE);

   //原型 #define unsigned_type_node    integer_types[itk_unsigned_int]
   mtcs_unsigned_type_node = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,INT_TYPE_SIZE);

   //原型 #define long_integer_type_node      integer_types[itk_long]
   mtcs_long_integer_type_node = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,LONG_TYPE_SIZE);

   //原型 #define long_unsigned_type_node     integer_types[itk_unsigned_long]
   mtcs_long_unsigned_type_node = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,LONG_TYPE_SIZE);

   //原型 #define long_long_integer_type_node integer_types[itk_long_long]
   mtcs_long_long_integer_type_node = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,LONG_LONG_TYPE_SIZE);

   //原型  #define long_long_unsigned_type_node   integer_types[itk_unsigned_long_long]
   mtcs_long_long_unsigned_type_node = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,LONG_LONG_TYPE_SIZE);

   for (i = 0; i <mtcsMode->mtcs_NUM_INT_N_ENTS/*!NUM_INT_N_ENTS*/; i ++){
      self->int_n_trees[i].signed_type = mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,
            mtcsMode->intData/*!int_n_data*/[i].bitsize);
      self->int_n_trees[i].unsigned_type = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,
            mtcsMode->intData/*!int_n_data*/[i].bitsize);
      if (mtcsMode->/*!int_n_enabled_p*/int_n_enabled_p[i]){
         self->integer_types[itk_intN_0 + i * 2] = self->int_n_trees[i].signed_type;
         self->integer_types[itk_unsigned_intN_0 + i * 2] = self->int_n_trees[i].unsigned_type;
      }
   }

   /* Define a boolean type.  This type only represents boolean values but
   may be larger than char depending on the value of BOOL_TYPE_SIZE.  */
   //原型  #define boolean_type_node     global_trees[TI_BOOLEAN_TYPE]
   mtcs_boolean_type_node = mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,BOOL_TYPE_SIZE);
   TREE_SET_CODE (mtcs_boolean_type_node, BOOLEAN_TYPE);
   TYPE_PRECISION (mtcs_boolean_type_node) = 1;
   TYPE_MAX_VALUE (mtcs_boolean_type_node) = mtcs_tree_build_int_cst/*!build_int_cst*/(self,mtcs_boolean_type_node, 1);

   char *sizeTypeStr=self->sizeTypeStr;//原型 #define SIZE_TYPE (TARGET_ABI64 ? "long unsigned int" : "unsigned int")
   /* Define what type to use for size_t.  */
   if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "unsigned int") == 0)
      self->global_trees[TI_SIZE_TYPE]/*!size_type_node*/ = mtcs_unsigned_type_node;
   else if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "long unsigned int") == 0)
      self->global_trees[TI_SIZE_TYPE]/*!size_type_node*/ = mtcs_long_unsigned_type_node;
   else if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "long long unsigned int") == 0)
      self->global_trees[TI_SIZE_TYPE]/*!size_type_node*/ = mtcs_long_long_unsigned_type_node;
   else if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "short unsigned int") == 0)
      self->global_trees[TI_SIZE_TYPE]/*!size_type_node*/ = mtcs_short_unsigned_type_node;
   else{
      int i;
      mtcs_size_type_node = NULL_TREE;
      for (i = 0; i < mtcsMode->mtcs_NUM_INT_N_ENTS/*!NUM_INT_N_ENTS*/; i++)
         if (mtcsMode->int_n_enabled_p[i]){
            char name[50], altname[50];
            sprintf (name, "__int%d unsigned", mtcsMode->intData/*!int_n_data*/[i].bitsize);
            sprintf (altname, "__int%d__ unsigned", mtcsMode->intData/*!int_n_data*/[i].bitsize);
            if (strcmp (name, SIZE_TYPE) == 0 || strcmp (altname, SIZE_TYPE) == 0){
               mtcs_size_type_node = self->int_n_trees[i].unsigned_type;
            }
         }
      if (mtcs_size_type_node == NULL_TREE)
         gcc_unreachable ();
   }
   char *ptrDiffTypeStr=self->ptrDiffTypeStr;
   mtcs_ptrdiff_type_node = NULL_TREE;
   /* Define what type to use for ptrdiff_t.  */
   if (strcmp (ptrDiffTypeStr/*!PTRDIFF_TYPE*/, "int") == 0)
      mtcs_ptrdiff_type_node = mtcs_integer_type_node;
   else if (strcmp (ptrDiffTypeStr/*!PTRDIFF_TYPE*/, "long int") == 0)
      mtcs_ptrdiff_type_node = mtcs_long_integer_type_node;
   else if (strcmp (ptrDiffTypeStr/*!PTRDIFF_TYPE*/, "long long int") == 0)
      mtcs_ptrdiff_type_node = mtcs_long_long_integer_type_node;
   else if (strcmp (ptrDiffTypeStr/*!PTRDIFF_TYPE*/, "short int") == 0)
      mtcs_ptrdiff_type_node = mtcs_short_integer_type_node;
   else{
      for (int i = 0; i < mtcsMode->mtcs_NUM_INT_N_ENTS/*!NUM_INT_N_ENTS*/; i++)
         if (mtcsMode->int_n_enabled_p[i]){
            char name[50], altname[50];
            sprintf (name, "__int%d", mtcsMode->intData/*!int_n_data*/[i].bitsize);
            sprintf (altname, "__int%d__", mtcsMode->intData/*!int_n_data*/[i].bitsize);
            if (strcmp (name, ptrDiffTypeStr/*!PTRDIFF_TYPE*/) == 0
                  || strcmp (altname, ptrDiffTypeStr/*!PTRDIFF_TYPE*/) == 0)
               mtcs_ptrdiff_type_node = self->int_n_trees[i].signed_type;
         }
      if (mtcs_ptrdiff_type_node == NULL_TREE)
         gcc_unreachable ();
   }

   /* Fill in the rest of the sized types.  Reuse existing type nodes
   when possible.  */
   mtcs_intQI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_QImode), 0);
   mtcs_intHI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_HImode), 0);

   mtcs_intSI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_SImode), 0);

   mtcs_intDI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_DImode), 0);

   mtcs_intTI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_TImode), 0);

   mtcs_unsigned_intQI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_QImode), 1);

   mtcs_unsigned_intHI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_HImode), 1);

   mtcs_unsigned_intSI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_SImode), 1);

   mtcs_unsigned_intDI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_DImode), 1);

   mtcs_unsigned_intTI_type_node = make_or_reuse_type (self,
         mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcsMode->modes.M_TImode), 1);

   /* Don't call build_qualified type for atomics.  That routine does
   special processing for atomics, and until they are initialized
   it's better not to make that call.

   Check to see if there is a target override for atomic types.  */
   mtcs_atomicQI_type_node = build_atomic_base(self,mtcs_unsigned_intQI_type_node,
         mtcsTarget/*!targetm.atomic_align_for_mode*/->atomic_align_for_mode(mtcsTarget,mtcsMode->modes.M_QImode));

   mtcs_atomicHI_type_node = build_atomic_base(self,mtcs_unsigned_intHI_type_node,
         mtcsTarget/*!targetm.atomic_align_for_mode*/->atomic_align_for_mode(mtcsTarget,mtcsMode->modes.M_HImode));

   mtcs_atomicSI_type_node = build_atomic_base(self,mtcs_unsigned_intSI_type_node,
         mtcsTarget/*!targetm.atomic_align_for_mode*/->atomic_align_for_mode(mtcsTarget,mtcsMode->modes.M_SImode));

   mtcs_atomicDI_type_node = build_atomic_base(self,mtcs_unsigned_intDI_type_node,
         mtcsTarget/*!targetm.atomic_align_for_mode*/->atomic_align_for_mode(mtcsTarget,mtcsMode->modes.M_DImode));

   mtcs_atomicTI_type_node = build_atomic_base(self,mtcs_unsigned_intTI_type_node,
         mtcsTarget/*!targetm.atomic_align_for_mode*/->atomic_align_for_mode(mtcsTarget,mtcsMode->modes.M_TImode));

   mtcs_access_public_node= get_identifier ("public");

   mtcs_access_protected_node= get_identifier ("protected");

   mtcs_access_private_node = get_identifier ("private");

   /* Define these next since types below may used them.  */
   mtcs_integer_zero_node = mtcs_tree_build_int_cst/*!build_int_cst*/(self,mtcs_integer_type_node, 0);

   mtcs_integer_one_node = mtcs_tree_build_int_cst/*!build_int_cst*/(self,mtcs_integer_type_node, 1);

   mtcs_integer_minus_one_node= mtcs_tree_build_int_cst/*!build_int_cst*/(self,mtcs_integer_type_node, -1);

   mtcs_size_zero_node = mtcs_tree_size_int/*!size_int*/(self,0);
   //fprintf(stderr,"mtcstree.c mtcs_size_zero_node %p type %p %p\n",mtcs_size_zero_node,self->sizetype_tab[0],TREE_TYPE(mtcs_size_zero_node));

   mtcs_size_one_node =  mtcs_tree_size_int/*!size_int*/(self,1);

   mtcs_bitsize_zero_node = mtcs_tree_bitsize_int/*!bitsize_int*/(self,0);

   mtcs_bitsize_one_node = mtcs_tree_bitsize_int/*!bitsize_int*/(self,1);

   mtcs_bitsize_unit_node = mtcs_tree_bitsize_int/*!bitsize_int*/(self,BITS_PER_UNIT);

   mtcs_boolean_false_node = TYPE_MIN_VALUE (mtcs_boolean_type_node);

   mtcs_boolean_true_node = TYPE_MAX_VALUE (mtcs_boolean_type_node);

   mtcs_void_type_node = make_node (VOID_TYPE);
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_void_type_node);

   /* We are not going to have real types in C with less than byte alignment,
   so we might as well not have any types that claim to have it.  */
   SET_TYPE_ALIGN (mtcs_void_type_node, BITS_PER_UNIT);
   TYPE_USER_ALIGN (mtcs_void_type_node) = 0;

   mtcs_void_node= make_node (VOID_CST);
   TREE_TYPE (mtcs_void_node) = mtcs_void_type_node;

   mtcs_void_list_node = build_tree_list (NULL_TREE, mtcs_void_type_node);

   mtcs_null_pointer_node = mtcs_tree_build_int_cst/*!build_int_cst*/(self,
         mtcs_tree_build_pointer_type/*!build_pointer_type*/(self, mtcs_void_type_node), 0);
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,TREE_TYPE ( mtcs_null_pointer_node));

   mtcs_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self, mtcs_void_type_node);

   mtcs_const_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,
         mtcs_tree_build_type_variant/*!build_type_variant*/(self, mtcs_void_type_node, 1, 0));

   /*!for (unsigned i = 0; i < ARRAY_SIZE (self->builtin_structptr_types); ++i)
      self->builtin_structptr_types[i].node = self->builtin_structptr_types[i].base;
   */
   createBuiltinStructptrType(self, mtcs_ptr_type_node, mtcs_const_ptr_type_node);

   mtcs_pointer_sized_int_node = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(self,
         self->pointerSize/*!POINTER_SIZE*/, 1);

   mtcs_float_type_node = make_node (REAL_TYPE);
   TYPE_PRECISION (mtcs_float_type_node) =self->floatTypeSize/*!FLOAT_TYPE_SIZE*/;
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_float_type_node);

   mtcs_double_type_node = make_node (REAL_TYPE);
   TYPE_PRECISION (mtcs_double_type_node) =self->doubleTypeSize/*!DOUBLE_TYPE_SIZE*/;
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_double_type_node);

   mtcs_long_double_type_node= make_node (REAL_TYPE);
   TYPE_PRECISION (mtcs_long_double_type_node) = self->longDoubleTypeSize/*!LONG_DOUBLE_TYPE_SIZE*/;
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_long_double_type_node);
   n_debug("mtcstree.c mtcs_tree_build_common_tree_nodes 00 创建 float node TI_FLOATN_NX_TYPE_FIRST:%d NUM_FLOATN_NX_TYPES:%d\n",
         TI_FLOATN_NX_TYPE_FIRST,NUM_FLOATN_NX_TYPES);
   for (i = 0; i < NUM_FLOATN_NX_TYPES; i++){
      int n = floatn_nx_types[i].n;
      bool extended = floatn_nx_types[i].extended;
      n_debug("mtcstree.c mtcs_tree_build_common_tree_nodes 11 创建 float node n:%d extended:%d\n",n,extended);
      scalar_float_mode mode;
      if (!mtcsTarget/*!targetm.floatn_mode*/->floatn_mode(mtcsTarget,n, extended).exists (&mode))
         continue;
      int precision = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
      n_debug("mtcstree.c mtcs_tree_build_common_tree_nodes 22 创建 float node n:%d extended:%d mode:%d precision:%d\n",
            n,extended,mode,precision);

      /* Work around the rs6000 KFmode having precision 113 not
      128.  */
      const struct real_format *fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode);
      gcc_assert (fmt->b == 2 && fmt->emin + fmt->emax == 3);
      int min_precision = fmt->p + ceil_log2 (fmt->emax - fmt->emin);
      if (!extended)
         gcc_assert (min_precision == n);
      if (precision < min_precision)
         precision = min_precision;
      n_debug("mtcstree.c mtcs_tree_build_common_tree_nodes 33 创建 float node n:%d extended:%d mode:%d precision:%d min_precision:%d\n",
              n,extended,mode,precision,min_precision);
      tree floatnNode/*!FLOATN_NX_TYPE_NODE (i)*/ = make_node (REAL_TYPE);
      TYPE_PRECISION (floatnNode) = precision;
      mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,floatnNode);
      SET_TYPE_MODE (floatnNode, mode);
      self->global_trees[TI_FLOATN_NX_TYPE_FIRST + i]=floatnNode;
   }
   tree float128t_type_node0 =self->global_trees[TI_FLOAT128_TYPE]/*!float128_type_node*/;
   self->global_trees[TI_FLOAT128T_TYPE]=float128t_type_node0;


//   #ifdef HAVE_BFmode
//   if (REAL_MODE_FORMAT (BFmode) == &arm_bfloat_half_format
//   && targetm.scalar_mode_supported_p (BFmode)
//   && targetm.libgcc_floating_mode_supported_p (BFmode))
//   {
//   bfloat16_type_node = make_node (REAL_TYPE);
//   TYPE_PRECISION (bfloat16_type_node) = GET_MODE_PRECISION (BFmode);
//   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,bfloat16_type_node);
//   SET_TYPE_MODE (bfloat16_type_node, BFmode);
//   }
//   #endif

   //由子类实现，在子类中可以确定是否存在 BFmode
   if(self->createTreeForBFmode)
      self->createTreeForBFmode(self);

   mtcs_float_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,mtcs_float_type_node);

   mtcs_double_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,mtcs_double_type_node);

   mtcs_long_double_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,mtcs_long_double_type_node);

   mtcs_integer_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,mtcs_integer_type_node);

   /* Fixed size integer types.  */
   mtcs_uint16_type_node = make_or_reuse_type (self,16, 1);

   mtcs_uint32_type_node = make_or_reuse_type (self,32, 1);

   mtcs_uint64_type_node = make_or_reuse_type (self,64, 1);

   if (mtcsTarget/*!targetm.scalar_mode_supported_p*/->scalar_mode_supported_p(mtcsTarget,mtcsMode->modes.M_TImode)){
       mtcs_uint128_type_node = make_or_reuse_type (self,128, 1);
   }

   /* Decimal float types. */
   if (mtcsTarget/*!targetm.decimal_float_supported_p*/->decimal_float_supported_p(mtcsTarget)){
      mtcs_dfloat32_type_node = make_node (REAL_TYPE);
      TYPE_PRECISION (mtcs_dfloat32_type_node) = DECIMAL32_TYPE_SIZE;
      SET_TYPE_MODE (mtcs_dfloat32_type_node, mtcsMode->modes.M_SDmode);
      mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_dfloat32_type_node);

      mtcs_dfloat64_type_node = make_node (REAL_TYPE);
      TYPE_PRECISION (mtcs_dfloat64_type_node) = DECIMAL64_TYPE_SIZE;
      SET_TYPE_MODE (mtcs_dfloat64_type_node, mtcsMode->modes.M_DDmode);
      mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_dfloat64_type_node);

      mtcs_dfloat128_type_node = make_node (REAL_TYPE);
      TYPE_PRECISION (mtcs_dfloat128_type_node) = DECIMAL128_TYPE_SIZE;
      SET_TYPE_MODE (mtcs_dfloat128_type_node, mtcsMode->modes.M_TDmode);
      mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,mtcs_dfloat128_type_node);
   }

   mtcs_complex_integer_type_node =mtcs_tree_build_complex_type/*!build_complex_type*/(self,mtcs_integer_type_node, true);

   mtcs_complex_float_type_node = mtcs_tree_build_complex_type/*!build_complex_type*/(self,mtcs_float_type_node, true);

   mtcs_complex_double_type_node = mtcs_tree_build_complex_type/*!build_complex_type*/(self,mtcs_double_type_node, true);

   mtcs_complex_long_double_type_node = mtcs_tree_build_complex_type/*!build_complex_type*/(self,mtcs_long_double_type_node,true);

//#define FLOATN_NX_TYPE_NODE0(IDX) self->global_trees[TI_FLOATN_NX_TYPE_FIRST + (IDX)]
//#define COMPLEX_FLOATN_NX_TYPE_NODE0(IDX)   self->global_trees[TI_COMPLEX_FLOATN_NX_TYPE_FIRST + (IDX)]

   for (i = 0; i < NUM_FLOATN_NX_TYPES; i++){
      if (mtcs_FLOATN_NX_TYPE_NODE(i) != NULL_TREE)
         mtcs_COMPLEX_FLOATN_NX_TYPE_NODE(i)  = mtcs_tree_build_complex_type/*!build_complex_type*/(self,
               mtcs_FLOATN_NX_TYPE_NODE(i));
   }

   /* Fixed-point type and mode nodes.  */
   make_fixed_type_node_fract/*!MAKE_FIXED_TYPE_NODE_FAMILY (fract, FRACT)*/(self);
   make_fixed_type_node_accum/*!MAKE_FIXED_TYPE_NODE_FAMILY (accum, ACCUM)*/(self);
   MTCS_MAKE_FIXED_MODE_NODE (fract, qq, QQ)
   MTCS_MAKE_FIXED_MODE_NODE (fract, hq, HQ)
   MTCS_MAKE_FIXED_MODE_NODE (fract, sq, SQ)
   MTCS_MAKE_FIXED_MODE_NODE (fract, dq, DQ)
   MTCS_MAKE_FIXED_MODE_NODE (fract, tq, TQ)
   MTCS_MAKE_FIXED_MODE_NODE (accum, ha, HA)
   MTCS_MAKE_FIXED_MODE_NODE (accum, sa, SA)
   MTCS_MAKE_FIXED_MODE_NODE (accum, da, DA)
   MTCS_MAKE_FIXED_MODE_NODE (accum, ta, TA)

   {
      tree t = mtcsTarget/*!targetm.build_builtin_va_list*/->build_builtin_va_list(mtcsTarget);
      /* Many back-ends define record types without setting TYPE_NAME.
      If we copied the record type here, we'd keep the original
      record type without a name.  This breaks name mangling.  So,
      don't copy record types and let c_common_nodes_and_builtins()
      declare the type to be __builtin_va_list.  */
      if (TREE_CODE (t) != RECORD_TYPE)
         t = build_variant_type_copy (t);
      mtcs_va_list_type_node = t;
   }

   /* SCEV analyzer global shared trees.  */
   mtcs_chrec_dont_know = make_node (SCEV_NOT_KNOWN);
   TREE_TYPE (mtcs_chrec_dont_know) = mtcs_void_type_node;

   mtcs_chrec_known = make_node (SCEV_KNOWN);
   TREE_TYPE (mtcs_chrec_known) = mtcs_void_type_node;
}

/* Build nodes that would have be created by the C front-end; necessary
   for including builtin-types.def and ultimately builtins.def.  */
//原型 lto_build_c_type_nodes lto-lang.cc
void mtcs_tree_build_c_type_nodes (MtcsTree *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=self;

   gcc_assert (mtcs_void_type_node);
   self->mtcs_string_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,mtcs_char_type_node);
   self->mtcs_const_string_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,
         mtcs_tree_build_qualified_type/*!build_qualified_type*/(self,mtcs_char_type_node, TYPE_QUAL_CONST));
   char *sizeTypeStr=self->sizeTypeStr;//原型 #define SIZE_TYPE (TARGET_ABI64 ? "long unsigned int" : "unsigned int")
   if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "unsigned int") == 0){
      self->mtcs_intmax_type_node = mtcs_integer_type_node;
      self->mtcs_uintmax_type_node =  mtcs_unsigned_type_node;
      self->mtcs_signed_size_type_node = mtcs_integer_type_node;
   }else if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "long unsigned int") == 0){
      self->mtcs_intmax_type_node = mtcs_long_integer_type_node;
      self->mtcs_uintmax_type_node =  mtcs_long_unsigned_type_node;
      self->mtcs_signed_size_type_node =mtcs_long_integer_type_node;
   }else if (strcmp (sizeTypeStr/*!SIZE_TYPE*/, "long long unsigned int") == 0){
      self->mtcs_intmax_type_node =  mtcs_long_long_integer_type_node;
      self->mtcs_uintmax_type_node =  mtcs_long_long_unsigned_type_node;
      self->mtcs_signed_size_type_node =mtcs_long_long_integer_type_node;
   }else{
      int i;

      self->mtcs_signed_size_type_node = NULL_TREE;
      for (i = 0; i < mtcsMode->mtcs_NUM_INT_N_ENTS; i++)
         if (mtcsMode->int_n_enabled_p[i]){
            char name[50], altname[50];
            sprintf (name, "__int%d unsigned", mtcsMode->intData/*!int_n_data*/[i].bitsize);
            sprintf (altname, "__int%d__ unsigned", mtcsMode->intData/*!int_n_data*/[i].bitsize);

            if (strcmp (name, sizeTypeStr/*!SIZE_TYPE*/) == 0
            || strcmp (altname, sizeTypeStr/*!SIZE_TYPE*/) == 0){
               self->mtcs_intmax_type_node = self->int_n_trees[i].signed_type;
               self->mtcs_uintmax_type_node = self->int_n_trees[i].unsigned_type;
               self->mtcs_signed_size_type_node = self->int_n_trees[i].signed_type;
            }
         }
      if (self->mtcs_signed_size_type_node == NULL_TREE)
         gcc_unreachable ();
   }

   self->mtcs_wint_type_node = mtcs_unsigned_type_node;
   mtcs_pid_type_node =mtcs_integer_type_node;
}

/*-------------构建 builtin_types built_in_attributes buitlins的类型tree----*/

//builtins.def需要
static int flag_isoc94 = 0;
static int flag_isoc99 = 0;
static int flag_isoc11 = 0;
static int flag_isoc23 = 0;

static GTY(()) tree string_type_node;
static GTY(()) tree const_string_type_node;
static GTY(()) tree wint_type_node;
static GTY(()) tree intmax_type_node;
static GTY(()) tree uintmax_type_node;
static GTY(()) tree signed_size_type_node;


/* Used to help initialize the builtin-types.def table.  When a type of
   the correct size doesn't exist, use error_mark_node instead of NULL.
   The later results in segfaults even when a decl using the type doesn't
   get invoked.  */
//buildin-types.def调用 builtin_type_for_size
//lhd_type_for_size是LANG_HOOKS_TYPE_FOR_SIZE的缺省实现 langhooks-def.h langhooks.cc
//c语言实现定义为 #define LANG_HOOKS_TYPE_FOR_SIZE c_common_type_for_size c-objc-common.h
static tree builtin_type_for_size (int size, bool unsignedp)
{
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);
  tree type =mtcsLang->types.type_for_size/*!lang_hooks.types.type_for_size*/(mtcsLang,size, unsignedp);
  return type ? type : mtcsTree->global_trees[TI_ERROR_MARK];
}

/* Builtin types.  */
enum mtcs_builtin_type
{
#define DEF_PRIMITIVE_TYPE(NAME, VALUE) NAME,
#define DEF_FUNCTION_TYPE_0(NAME, RETURN) NAME,
#define DEF_FUNCTION_TYPE_1(NAME, RETURN, ARG1) NAME,
#define DEF_FUNCTION_TYPE_2(NAME, RETURN, ARG1, ARG2) NAME,
#define DEF_FUNCTION_TYPE_3(NAME, RETURN, ARG1, ARG2, ARG3) NAME,
#define DEF_FUNCTION_TYPE_4(NAME, RETURN, ARG1, ARG2, ARG3, ARG4) NAME,
#define DEF_FUNCTION_TYPE_5(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5) NAME,
#define DEF_FUNCTION_TYPE_6(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6) NAME,
#define DEF_FUNCTION_TYPE_7(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6, ARG7) NAME,
#define DEF_FUNCTION_TYPE_8(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6, ARG7, ARG8) NAME,
#define DEF_FUNCTION_TYPE_9(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6, ARG7, ARG8, ARG9) NAME,
#define DEF_FUNCTION_TYPE_10(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
              ARG6, ARG7, ARG8, ARG9, ARG10) NAME,
#define DEF_FUNCTION_TYPE_11(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
              ARG6, ARG7, ARG8, ARG9, ARG10, ARG11) NAME,
#define DEF_FUNCTION_TYPE_VAR_0(NAME, RETURN) NAME,
#define DEF_FUNCTION_TYPE_VAR_1(NAME, RETURN, ARG1) NAME,
#define DEF_FUNCTION_TYPE_VAR_2(NAME, RETURN, ARG1, ARG2) NAME,
#define DEF_FUNCTION_TYPE_VAR_3(NAME, RETURN, ARG1, ARG2, ARG3) NAME,
#define DEF_FUNCTION_TYPE_VAR_4(NAME, RETURN, ARG1, ARG2, ARG3, ARG4) NAME,
#define DEF_FUNCTION_TYPE_VAR_5(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG6) \
            NAME,
#define DEF_FUNCTION_TYPE_VAR_6(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6) NAME,
#define DEF_FUNCTION_TYPE_VAR_7(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
            ARG6, ARG7) NAME,
#define DEF_POINTER_TYPE(NAME, TYPE) NAME,
#include "builtin-types.def"
#undef DEF_PRIMITIVE_TYPE
#undef DEF_FUNCTION_TYPE_0
#undef DEF_FUNCTION_TYPE_1
#undef DEF_FUNCTION_TYPE_2
#undef DEF_FUNCTION_TYPE_3
#undef DEF_FUNCTION_TYPE_4
#undef DEF_FUNCTION_TYPE_5
#undef DEF_FUNCTION_TYPE_6
#undef DEF_FUNCTION_TYPE_7
#undef DEF_FUNCTION_TYPE_8
#undef DEF_FUNCTION_TYPE_9
#undef DEF_FUNCTION_TYPE_10
#undef DEF_FUNCTION_TYPE_11
#undef DEF_FUNCTION_TYPE_VAR_0
#undef DEF_FUNCTION_TYPE_VAR_1
#undef DEF_FUNCTION_TYPE_VAR_2
#undef DEF_FUNCTION_TYPE_VAR_3
#undef DEF_FUNCTION_TYPE_VAR_4
#undef DEF_FUNCTION_TYPE_VAR_5
#undef DEF_FUNCTION_TYPE_VAR_6
#undef DEF_FUNCTION_TYPE_VAR_7
#undef DEF_POINTER_TYPE
  BT_LAST
};

typedef enum mtcs_builtin_type builtin_type;


enum built_in_attribute
{
#define DEF_ATTR_NULL_TREE(ENUM) ENUM,
#define DEF_ATTR_INT(ENUM, VALUE) ENUM,
#define DEF_ATTR_STRING(ENUM, VALUE) ENUM,
#define DEF_ATTR_IDENT(ENUM, STRING) ENUM,
#define DEF_ATTR_TREE_LIST(ENUM, PURPOSE, VALUE, CHAIN) ENUM,
#include "builtin-attrs.def"
#undef DEF_ATTR_NULL_TREE
#undef DEF_ATTR_INT
#undef DEF_ATTR_STRING
#undef DEF_ATTR_IDENT
#undef DEF_ATTR_TREE_LIST
  ATTR_LAST
};


/* Set explicit builtin function nodes and whether it is an implicit
   function.  */
//原型 set_builtin_decl tree.h
void mtcs_tree_set_builtin_decl (MtcsTree *self,enum built_in_function fncode, tree decl, bool implicit_p)
{
  size_t ufncode = (size_t)fncode;
  gcc_checking_assert (BUILTIN_VALID_P (fncode)   && (decl != NULL_TREE || !implicit_p));
  self->builtin_info[ufncode].decl = decl;
  self->builtin_info[ufncode].implicit_p = implicit_p;
  self->builtin_info[ufncode].declared_p = false;
}

/* Support for DEF_BUILTIN.  */
//原型 def_builtin_1 lto-lang.cc
static void def_builtin_1 (MtcsTree *self,enum built_in_function fncode, const char *name,
          enum built_in_class fnclass, tree fntype, tree libtype,
          bool both_p, bool fallback_p, bool nonansi_p,
          tree fnattrs, bool implicit_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsTree *mtcsTree=self;
   //fprintf(stderr,"btlast----- %d\n",BT_LAST);

   tree decl;
   const char *libname;
   if (fntype == mtcs_error_mark_node)
      return;
   libname = name + strlen ("__builtin_");
   if(strcmp(name,"__builtin_exp2f")==0){
      n_debug("mtcstree.c def_builtin_1 00 fncode:%d name:%s,fnclass:%d fntype:%p libtype:%p %d %d  %d %p %d %s %d\n",
      fncode,name,fnclass,fntype,libtype,both_p,fallback_p,nonansi_p,fnattrs,implicit_p,get_tree_code_name(TREE_CODE(fnattrs)),ATTR_LAST);
      aet_print_tree(fntype);
      aet_print_tree(TREE_TYPE(fntype));
   }
   decl = mtcs_tree_add_builtin_function/*!add_builtin_function*/(self,
         name, fntype, fncode, fnclass, (fallback_p ? libname : NULL), fnattrs);
   if(strcmp(name,"__builtin_exp2f")==0)
      n_debug("mtcstree.c  def_builtin_1 11 fncode:%d name:%s,fnclass:%d fntype:%p libtype:%p %d %d  %d %p %d decl:%p\n",
         fncode,name,fnclass,fntype,libtype,both_p,fallback_p,nonansi_p,fnattrs,implicit_p,decl);
   if (both_p   && !mtcsOptionsItem->x_flag_no_builtin
         && !(nonansi_p && mtcsOptionsItem->x_flag_no_nonansi_builtin)){
      if(strcmp(name,"__builtin_exp2f")==0)
         n_debug("mtcstree.c def_builtin_1 22 fncode:%d name:%s,fnclass:%d fntype:%p libtype:%p %d %d  %d %p %d\n",
            fncode,name,fnclass,fntype,libtype,both_p,fallback_p,nonansi_p,fnattrs,implicit_p);
      mtcs_tree_add_builtin_function/*!add_builtin_function*/(self,
            libname, libtype, fncode, fnclass, NULL, fnattrs);
      if(strcmp(name,"__builtin_exp2f")==0)
         n_debug("mtcstree.c def_builtin_1 33 fncode:%d name:%s,fnclass:%d fntype:%p libtype:%p %d %d  %d %p %d\n",
            fncode,name,fnclass,fntype,libtype,both_p,fallback_p,nonansi_p,fnattrs,implicit_p);
   }
   mtcs_tree_set_builtin_decl/*!set_builtin_decl*/(self,fncode, decl, implicit_p);
}


/* Cribbed from c-common.cc.  */
static void def_fn_type (MtcsTree *self,builtin_type def, builtin_type ret, bool var, int n, ...)
{
  MtcsTree *mtcsTree=self;
  tree t;
  tree *args = XALLOCAVEC (tree, n);
  va_list list;
  int i;
  bool err = false;

  va_start (list, n);
  for (i = 0; i < n; ++i){
      builtin_type a = (builtin_type) va_arg (list, int);
      t = self->builtin_types[a];
      if (t == mtcs_error_mark_node){
         //fprintf(stderr,"mtcstree.c def_fn_type 是 mtcs_error_mark_node def:%d ret:%d var:%d n:%d\n",def,ret, var,n);
         err = true;
      }
      args[i] = t;
  }
  va_end (list);

  t = self->builtin_types[ret];
  if (err)
    t = mtcs_error_mark_node;
  if (t == mtcs_error_mark_node)
    ;
  else if (var)
    t = mtcs_tree_build_varargs_function_type_array/*!build_varargs_function_type_array*/(self,t, n, args);
  else
    t = mtcs_tree_build_function_type_array/*!build_function_type_array*/(self,t, n, args);

  self->builtin_types[def] = t;
}

/* Initialize the attribute table for all the supported builtins.  */
//原型 lto_init_attributes
static void initAttributes (MtcsTree *self)
{
  /* Fill in the built_in_attributes array.  */
#define DEF_ATTR_NULL_TREE(ENUM)          \
  self->built_in_attributes[(int) ENUM] = NULL_TREE;
#define DEF_ATTR_INT(ENUM, VALUE)            \
      self->built_in_attributes[(int) ENUM] = mtcs_tree_build_int_cst/*!build_int_cst*/(self,NULL_TREE, VALUE);
#define DEF_ATTR_STRING(ENUM, VALUE)            \
      self->built_in_attributes[(int) ENUM] = build_string (strlen (VALUE), VALUE);
#define DEF_ATTR_IDENT(ENUM, STRING)            \
      self->built_in_attributes[(int) ENUM] = get_identifier (STRING);
#define DEF_ATTR_TREE_LIST(ENUM, PURPOSE, VALUE, CHAIN)  \
      self->built_in_attributes[(int) ENUM]         \
    = tree_cons (self->built_in_attributes[(int) PURPOSE],  \
          self->built_in_attributes[(int) VALUE],  \
          self->built_in_attributes[(int) CHAIN]);
#include "builtin-attrs.def"
#undef DEF_ATTR_NULL_TREE
#undef DEF_ATTR_INT
#undef DEF_ATTR_STRING
#undef DEF_ATTR_IDENT
#undef DEF_ATTR_TREE_LIST
}

/**
 * 修改内建函数类型
 * 在builtin-types.def中有23处调用到了
 * build_complex_type
 * build_pointer_type
 * build_qualified_type
 */
static void modifyBuiltinTypes(MtcsTree *self)
{
   MtcsTree *mtcsTree=self;
   if(self->builtin_types[BT_COMPLEX_FLOAT16]==mtcs_error_mark_node){
      n_debug("mtcstree.c modifyBuiltinTypes BT_COMPLEX_FLOAT16 是 mtcs_error_mark_node\n");
   }
   n_debug("mtcstree.c modifyBuiltinTypes BUILT_IN_BCMP:%d BT_FN_INT_CONST_PTR_CONST_PTR_SIZE:%d\n",
         BUILT_IN_BCMP,BT_FN_INT_CONST_PTR_CONST_PTR_SIZE);
   //原型 DEF_PRIMITIVE_TYPE (BT_COMPLEX_FLOAT16,...
   self->builtin_types[BT_COMPLEX_FLOAT16]=mtcs_float16_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float16_type_node):mtcs_error_mark_node;
   self->builtin_types[BT_COMPLEX_FLOAT32]=mtcs_float32_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float32_type_node):mtcs_error_mark_node;
   self->builtin_types[BT_COMPLEX_FLOAT64]=mtcs_float64_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float64_type_node):mtcs_error_mark_node;
   self->builtin_types[BT_COMPLEX_FLOAT128]=mtcs_float128_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float128_type_node):mtcs_error_mark_node;
   self->builtin_types[BT_COMPLEX_FLOAT32X]=mtcs_float32x_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float32x_type_node):mtcs_error_mark_node;
   self->builtin_types[BT_COMPLEX_FLOAT64X]=mtcs_float64x_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float64x_type_node):mtcs_error_mark_node;
   self->builtin_types[BT_COMPLEX_FLOAT128X]=mtcs_float128x_type_node?
         mtcs_tree_build_complex_type(self,mtcs_float128x_type_node):mtcs_error_mark_node;


   self->builtin_types[BT_VOLATILE_PTR]=mtcs_tree_build_pointer_type(self,
         (mtcs_tree_build_qualified_type (self,mtcs_void_type_node,  TYPE_QUAL_VOLATILE)));

   self->builtin_types[BT_CONST_VOLATILE_PTR]=mtcs_tree_build_pointer_type(self,
         (mtcs_tree_build_qualified_type (self,mtcs_void_type_node,  TYPE_QUAL_VOLATILE|TYPE_QUAL_CONST)));

   self->builtin_types[BT_CONST_DOUBLE_PTR]=mtcs_tree_build_pointer_type(self,
         (mtcs_tree_build_qualified_type (self,mtcs_double_type_node,  TYPE_QUAL_CONST)));

   self->builtin_types[BT_FLOAT16_PTR]=mtcs_float16_type_node?
         mtcs_tree_build_pointer_type(self,mtcs_float16_type_node):mtcs_error_mark_node;

   self->builtin_types[BT_FLOAT32_PTR]=mtcs_float32_type_node?
         mtcs_tree_build_pointer_type(self,mtcs_float32_type_node):mtcs_error_mark_node;

   self->builtin_types[BT_FLOAT64_PTR]=mtcs_float64_type_node?
         mtcs_tree_build_pointer_type(self,mtcs_float64_type_node):mtcs_error_mark_node;

   self->builtin_types[BT_FLOAT128_PTR]=mtcs_float128_type_node?
            mtcs_tree_build_pointer_type(self,mtcs_float128_type_node):mtcs_error_mark_node;

   self->builtin_types[BT_FLOAT32X_PTR]=mtcs_float32x_type_node?
         mtcs_tree_build_pointer_type(self,mtcs_float32x_type_node):mtcs_error_mark_node;

   self->builtin_types[BT_FLOAT64X_PTR]=mtcs_float64x_type_node?
           mtcs_tree_build_pointer_type(self,mtcs_float64x_type_node):mtcs_error_mark_node;

   self->builtin_types[BT_FLOAT128X_PTR]=mtcs_float128x_type_node?
           mtcs_tree_build_pointer_type(self,mtcs_float128x_type_node):mtcs_error_mark_node;


   self->builtin_types[BT_CONST_SIZE]=mtcs_tree_build_qualified_type(self,mtcs_size_type_node, TYPE_QUAL_CONST);

   self->builtin_types[BT_PTR_CONST_STRING]=mtcs_tree_build_pointer_type(self,
         (mtcs_tree_build_qualified_type (self,self->mtcs_string_type_node,  TYPE_QUAL_CONST)));
}

/* Create builtin types and functions.  VA_LIST_REF_TYPE_NODE and
   VA_LIST_ARG_TYPE_NODE are used in builtin-types.def.  */
//原型 lto_define_builtins lto-lang.cc
//创建内建函数的三个内容 类型 属性 函数声明 分别存在 MtcsTree 中的 builtin_types、 built_in_attributes和builtin_info
void mtcs_tree_define_builtins (MtcsTree *self,tree va_list_ref_type_node ATTRIBUTE_UNUSED,
           tree va_list_arg_type_node ATTRIBUTE_UNUSED)
{
   tree string_type_node;
   //在builtins.def中引用到
 //  static GTY(()) tree string_type_node;
 //  static GTY(()) tree const_string_type_node;
 //  static GTY(()) tree wint_type_node;
 //  static GTY(()) tree intmax_type_node;
 //  static GTY(()) tree uintmax_type_node;
 //  static GTY(()) tree signed_size_type_node;
   string_type_node=self->mtcs_string_type_node;
   const_string_type_node=self->mtcs_const_string_type_node;
   wint_type_node=self->mtcs_wint_type_node;
   intmax_type_node=self->mtcs_intmax_type_node;
   uintmax_type_node=self->mtcs_uintmax_type_node;
   signed_size_type_node=self->mtcs_signed_size_type_node;

#define DEF_PRIMITIVE_TYPE(ENUM, VALUE) \
  self->builtin_types[ENUM] = VALUE;
#define DEF_FUNCTION_TYPE_0(ENUM, RETURN) \
  def_fn_type (self,ENUM, RETURN, 0, 0);
#define DEF_FUNCTION_TYPE_1(ENUM, RETURN, ARG1) \
  def_fn_type (self,ENUM, RETURN, 0, 1, ARG1);
#define DEF_FUNCTION_TYPE_2(ENUM, RETURN, ARG1, ARG2) \
  def_fn_type (self,ENUM, RETURN, 0, 2, ARG1, ARG2);
#define DEF_FUNCTION_TYPE_3(ENUM, RETURN, ARG1, ARG2, ARG3) \
  def_fn_type (self,ENUM, RETURN, 0, 3, ARG1, ARG2, ARG3);
#define DEF_FUNCTION_TYPE_4(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4) \
  def_fn_type (self,ENUM, RETURN, 0, 4, ARG1, ARG2, ARG3, ARG4);
#define DEF_FUNCTION_TYPE_5(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5) \
  def_fn_type (self,ENUM, RETURN, 0, 5, ARG1, ARG2, ARG3, ARG4, ARG5);
#define DEF_FUNCTION_TYPE_6(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6)               \
  def_fn_type (self,ENUM, RETURN, 0, 6, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#define DEF_FUNCTION_TYPE_7(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6, ARG7)               \
  def_fn_type (self,ENUM, RETURN, 0, 7, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define DEF_FUNCTION_TYPE_8(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6, ARG7, ARG8)            \
  def_fn_type (self,ENUM, RETURN, 0, 8, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6,  \
          ARG7, ARG8);
#define DEF_FUNCTION_TYPE_9(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6, ARG7, ARG8, ARG9)         \
  def_fn_type (self,ENUM, RETURN, 0, 9, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6,  \
          ARG7, ARG8, ARG9);
#define DEF_FUNCTION_TYPE_10(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
              ARG6, ARG7, ARG8, ARG9, ARG10)     \
  def_fn_type (self,ENUM, RETURN, 0, 10, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6,  \
          ARG7, ARG8, ARG9, ARG10);
#define DEF_FUNCTION_TYPE_11(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
              ARG6, ARG7, ARG8, ARG9, ARG10, ARG11)    \
  def_fn_type (self,ENUM, RETURN, 0, 11, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6,  \
          ARG7, ARG8, ARG9, ARG10, ARG11);
#define DEF_FUNCTION_TYPE_VAR_0(ENUM, RETURN) \
  def_fn_type (self,ENUM, RETURN, 1, 0);
#define DEF_FUNCTION_TYPE_VAR_1(ENUM, RETURN, ARG1) \
  def_fn_type (self,ENUM, RETURN, 1, 1, ARG1);
#define DEF_FUNCTION_TYPE_VAR_2(ENUM, RETURN, ARG1, ARG2) \
  def_fn_type (self,ENUM, RETURN, 1, 2, ARG1, ARG2);
#define DEF_FUNCTION_TYPE_VAR_3(ENUM, RETURN, ARG1, ARG2, ARG3) \
  def_fn_type (self,ENUM, RETURN, 1, 3, ARG1, ARG2, ARG3);
#define DEF_FUNCTION_TYPE_VAR_4(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4) \
  def_fn_type (self,ENUM, RETURN, 1, 4, ARG1, ARG2, ARG3, ARG4);
#define DEF_FUNCTION_TYPE_VAR_5(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5) \
  def_fn_type (self,ENUM, RETURN, 1, 5, ARG1, ARG2, ARG3, ARG4, ARG5);
#define DEF_FUNCTION_TYPE_VAR_6(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
             ARG6)   \
  def_fn_type (self,ENUM, RETURN, 1, 6, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#define DEF_FUNCTION_TYPE_VAR_7(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
            ARG6, ARG7)          \
  def_fn_type (self,ENUM, RETURN, 1, 7, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define DEF_POINTER_TYPE(ENUM, TYPE) \
  self->builtin_types[(int) ENUM] = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,self->builtin_types[(int) TYPE]);

#include "builtin-types.def"

#undef DEF_PRIMITIVE_TYPE
#undef DEF_FUNCTION_TYPE_0
#undef DEF_FUNCTION_TYPE_1
#undef DEF_FUNCTION_TYPE_2
#undef DEF_FUNCTION_TYPE_3
#undef DEF_FUNCTION_TYPE_4
#undef DEF_FUNCTION_TYPE_5
#undef DEF_FUNCTION_TYPE_6
#undef DEF_FUNCTION_TYPE_7
#undef DEF_FUNCTION_TYPE_8
#undef DEF_FUNCTION_TYPE_9
#undef DEF_FUNCTION_TYPE_10
#undef DEF_FUNCTION_TYPE_11
#undef DEF_FUNCTION_TYPE_VAR_0
#undef DEF_FUNCTION_TYPE_VAR_1
#undef DEF_FUNCTION_TYPE_VAR_2
#undef DEF_FUNCTION_TYPE_VAR_3
#undef DEF_FUNCTION_TYPE_VAR_4
#undef DEF_FUNCTION_TYPE_VAR_5
#undef DEF_FUNCTION_TYPE_VAR_6
#undef DEF_FUNCTION_TYPE_VAR_7
#undef DEF_POINTER_TYPE
  self->builtin_types[(int) BT_LAST] = NULL_TREE;
  modifyBuiltinTypes(self);
  initAttributes(self);

#define DEF_BUILTIN(ENUM, NAME, CLASS, TYPE, LIBTYPE, BOTH_P, FALLBACK_P,\
          NONANSI_P, ATTRS, IMPLICIT, COND)        \
    if (NAME && COND)                     \
      def_builtin_1 (self,ENUM, NAME, CLASS, self->builtin_types[(int) TYPE],   \
           self->builtin_types[(int) LIBTYPE], BOTH_P, FALLBACK_P,   \
           NONANSI_P, self->built_in_attributes[(int) ATTRS], IMPLICIT);
#include "builtins.def"
}

/****************完成构造内建函数-------*/

/* Common function for add_builtin_function, add_builtin_function_ext_scope
   and simulate_builtin_function_decl.  */
//原型 build_builtin_function langhooks.cc
static tree build_builtin_function (MtcsTree *self,location_t location, const char *name, tree type,
         int function_code, enum built_in_class cl,  const char *library_name, tree attrs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAttribs *mtcsAttribs=mtcs_target_get_attribs(mtcsTarget);

   tree   id = get_identifier (name);
   tree decl = mtcs_tree_build_decl/*!build_decl*/(self,location, FUNCTION_DECL, id, type);

   TREE_PUBLIC (decl)         = 1;
   DECL_EXTERNAL (decl)       = 1;

   set_decl_built_in_function (decl, cl, function_code);
   //fprintf(stderr,"mtcstree.c build_builtin_function 00xx %s cl:%d decl:%p\n",name,cl,decl);
   if (library_name){
      tree libname = get_identifier (library_name);
      libname =mtcsTarget/*!targetm.mangle_decl_assembler_name*/->mangle_decl_assembler_name(mtcsTarget,decl, libname);
      SET_DECL_ASSEMBLER_NAME (decl, libname);
   }
   /* Possibly apply some default attributes to this built-in function.  */
   if (attrs){
      //if(strcmp(name,"__builtin_nan")==0)
         //fprintf(stderr,"mtcstree.c build_builtin_function 00 %s %p %s\n",name,attrs,get_tree_code_name(TREE_CODE(attrs)));

//      tree list=attrs;
//
//        while (list){
//            tree attr = get_attribute_name (list);
//            size_t ident_len = IDENTIFIER_LENGTH (attr);
//            list = TREE_CHAIN (list);
//          }




      mtcs_attribs_decl_attributes/*!decl_attributes*/(mtcsAttribs,&decl, attrs, ATTR_FLAG_BUILT_IN);
      //if(strcmp(name,"__builtin_nan")==0)
       //  fprintf(stderr,"mtcstree.c build_builtin_function 00 %s %p\n",name,attrs);
   }else{
      mtcs_attribs_decl_attributes/*!decl_attributes*/(mtcsAttribs,&decl, NULL_TREE, 0);
   }

   return decl;
}


/* Create a builtin function.  */
//原型 add_builtin_function langhooks.h langhooks.cc
tree mtcs_tree_add_builtin_function (MtcsTree *self,const char *name, tree type, int function_code,
            enum built_in_class cl, const char *library_name,tree attrs)
{
  tree decl = build_builtin_function(self,BUILTINS_LOCATION, name, type,
                  function_code, cl, library_name, attrs);
  return self->builtin_function/*!lang_hooks.builtin_function (decl)*/(self,decl);
}


//原型 INT_TYPE_SIZE; default.h 平台定义 nvptx.h
void mtcs_tree_set_int_type_size(MtcsTree *self,int size)
{
   self->intTypeSize=size;
}

//原型 LONG_TYPE_SIZE; default.h 平台定义 nvptx.h
void mtcs_tree_set_long_type_size(MtcsTree *self,int size)
{
   self->longTypeSize=size;

}
//原型 LONG_LONG_TYPE_SIZE; default.h 平台定义 nvptx.h
void mtcs_tree_set_long_long_type_size(MtcsTree *self,int size)
{
   self->longLongTypeSize=size;
}

//原型 SHORT_TYPE_SIZE; default.h 平台定义 nvptx.h
void mtcs_tree_set_short_type_size(MtcsTree *self,int size)
{
   self->shortTypeSize=size;
}

//原型 #define SIZETYPE SIZE_TYPE  defaults.h SIZE_TYPE 每个平台定义不一样
void mtcs_tree_set_sizetype_string(MtcsTree *self, const char *sizeTypeStr)
{
   self->sizeTypeStr =n_strdup(sizeTypeStr);
}

//#define PTRDIFF_TYPE (TARGET_ABI64 ? "long int" : "int")
void mtcs_tree_set_prt_diff_type_string(MtcsTree *self, const char *prtDiffTypeStr)
{
   self->ptrDiffTypeStr =n_strdup(prtDiffTypeStr);
}


//原型 #define POINTER_SIZE BITS_PER_WORD
int mtcs_tree_get_pointer_size(MtcsTree *self)
{
   return self->pointerSize;
}

void mtcs_tree_set_pointer_size(MtcsTree *self, int size)
{
   self->pointerSize=size;
}

//原型 #define FLOAT_TYPE_SIZE BITS_PER_WORD
int mtcs_tree_get_float_type_size(MtcsTree *self)
{
   return self->floatTypeSize;
}

void mtcs_tree_set_float_type_size(MtcsTree *self, int size)
{
   self->floatTypeSize=size;

}

//原型 #define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
int mtcs_tree_get_double_type_size(MtcsTree *self)
{
   return self->doubleTypeSize;
}

void mtcs_tree_set_double_type_size(MtcsTree *self, int size)
{
   self->doubleTypeSize=size;
}

//原型 #define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
int mtcs_tree_get_long_double_type_size(MtcsTree *self)
{
   return self->longDoubleTypeSize;
}

void mtcs_tree_set_long_double_type_size(MtcsTree *self, int size)
{
   self->longDoubleTypeSize=size;
}

//原型 #define POINTER_SIZE_UNITS ((POINTER_SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT)
int mtcs_tree_get_pointer_size_units(MtcsTree *self)
{
   return self->pointerSizeUnits;
}

void mtcs_tree_set_pointer_size_units(MtcsTree *self, int size)
{
   self->pointerSizeUnits=size;
}


/* Return whether the standard builtin function can be used as an explicit
   function.  */
//原型 builtin_decl_explicit_p tree.h
bool mtcs_tree_builtin_decl_explicit_p (MtcsTree *self,enum built_in_function fncode)
{
  gcc_checking_assert (BUILTIN_VALID_P (fncode));
  return (self->builtin_info[(size_t)fncode].decl != NULL_TREE);
}

/* A subroutine of build_common_builtin_nodes.  Define a builtin function.  */
//原型 local_define_builtin tree.cc
static void local_define_builtin (MtcsTree *self,const char *name, tree type, enum built_in_function code,
                      const char *library_name, int ecf_flags)
{
  tree decl;
  decl = mtcs_tree_add_builtin_function/*!add_builtin_function*/(self,name, type, code, BUILT_IN_NORMAL,
                library_name, NULL_TREE);
  set_call_expr_flags (decl, ecf_flags);
  mtcs_tree_set_builtin_decl/*!set_builtin_decl*/(self,code, decl, true);
}


/* Return a data type that has machine mode MODE.
   If the mode is an integer,
   then UNSIGNEDP selects between signed and unsigned types.
   If the mode is a fixed-point mode,
   then UNSIGNEDP selects between saturating and nonsaturating types.  */
//原型 lto_type_for_mode lto-lang.cc #define LANG_HOOKS_TYPE_FOR_MODE lto_type_for_mode
static tree typeForMode (MtcsTree *self,machine_mode mode, int unsigned_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;

   tree t;
   int i;

   if (mode == TYPE_MODE (mtcs_integer_type_node))
      return unsigned_p ? mtcs_unsigned_type_node : mtcs_integer_type_node;

   if (mode == TYPE_MODE (mtcs_signed_char_type_node))
      return unsigned_p ? mtcs_unsigned_char_type_node : mtcs_signed_char_type_node;

   if (mode == TYPE_MODE (mtcs_short_integer_type_node))
      return unsigned_p ? mtcs_short_unsigned_type_node : mtcs_short_integer_type_node;

   if (mode == TYPE_MODE (mtcs_long_integer_type_node))
      return unsigned_p ? mtcs_long_unsigned_type_node : mtcs_long_integer_type_node;

   if (mode == TYPE_MODE (mtcs_long_long_integer_type_node))
      return unsigned_p ? mtcs_long_long_unsigned_type_node : mtcs_long_long_integer_type_node;

   for (i = 0; i < mtcsMode->mtcs_NUM_INT_N_ENTS/*!NUM_INT_N_ENTS*/; i ++)
      if (mtcsMode->int_n_enabled_p[i] && mode ==mtcsMode->intData/*!int_n_data*/[i].m)
         return (unsigned_p ? self->int_n_trees[i].unsigned_type: self->int_n_trees[i].signed_type);

   if (mode == mtcsMode->modes.M_QImode)
      return unsigned_p ? mtcs_unsigned_intQI_type_node : mtcs_intQI_type_node;

   if (mode == mtcsMode->modes.M_HImode)
      return unsigned_p ? mtcs_unsigned_intHI_type_node : mtcs_intHI_type_node;

   if (mode == mtcsMode->modes.M_SImode)
      return unsigned_p ? mtcs_unsigned_intSI_type_node : mtcs_intSI_type_node;

   if (mode == mtcsMode->modes.M_DImode)
      return unsigned_p ? mtcs_unsigned_intDI_type_node : mtcs_intDI_type_node;

#if HOST_BITS_PER_WIDE_INT >= 64
   if (mode == TYPE_MODE (mtcs_intTI_type_node))
      return unsigned_p ? mtcs_unsigned_intTI_type_node : mtcs_intTI_type_node;
#endif

   if (mtcs_float16_type_node && mode == TYPE_MODE (mtcs_float16_type_node))
      return mtcs_float16_type_node;

   if (mode == TYPE_MODE (mtcs_float_type_node))
      return mtcs_float_type_node;

   if (mode == TYPE_MODE (mtcs_double_type_node))
      return mtcs_double_type_node;

   if (mode == TYPE_MODE (mtcs_long_double_type_node))
      return mtcs_long_double_type_node;

   for (i = 0; i < NUM_FLOATN_NX_TYPES; i++)
      if (mtcs_FLOATN_NX_TYPE_NODE (i) != NULL_TREE  && mode == TYPE_MODE (mtcs_FLOATN_NX_TYPE_NODE (i)))
         return mtcs_FLOATN_NX_TYPE_NODE (i);

   if (mode == TYPE_MODE (mtcs_void_type_node))
      return mtcs_void_type_node;

   if (mode == TYPE_MODE (mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,char_type_node))
   || mode == TYPE_MODE (mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,integer_type_node))){
      unsigned int precision = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,
                                       mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode));
      return (unsigned_p ? mtcs_stor_layout_make_unsigned_type/*!make_unsigned_type*/(mtcsStorLayout,precision) :
            mtcs_stor_layout_make_signed_type/*!make_signed_type*/(mtcsStorLayout,precision));
   }

   if (mtcs_mode_is_complex_p/*!COMPLEX_MODE_P*/(mtcsMode,mode)){
      machine_mode inner_mode;
      tree inner_type;

      if (mode == TYPE_MODE (mtcs_complex_float_type_node))
         return mtcs_complex_float_type_node;
      if (mode == TYPE_MODE (mtcs_complex_double_type_node))
         return mtcs_complex_double_type_node;
      if (mode == TYPE_MODE (mtcs_complex_long_double_type_node))
         return mtcs_complex_long_double_type_node;

      for (i = 0; i < NUM_FLOATN_NX_TYPES; i++)
         if (mtcs_COMPLEX_FLOATN_NX_TYPE_NODE (i) != NULL_TREE
               && mode == TYPE_MODE (COMPLEX_FLOATN_NX_TYPE_NODE (i)))
            return mtcs_COMPLEX_FLOATN_NX_TYPE_NODE (i);

      if (mode == TYPE_MODE (mtcs_complex_integer_type_node) && !unsigned_p)
         return mtcs_complex_integer_type_node;

      inner_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
      inner_type = typeForMode(self,inner_mode, unsigned_p);
      if (inner_type != NULL_TREE)
         return mtcs_tree_build_complex_type/*!build_complex_type*/(self,inner_type);
   }else if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_VECTOR_BOOL
         && valid_vector_subparts_p (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode))){
      unsigned int elem_bits = vector_element_size (mtcs_mode_get_precision_poly/*!GET_MODE_PRECISION*/(mtcsMode,mode),
      mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode));
      tree bool_type = mtcs_tree_build_nonstandard_boolean_type/*!build_nonstandard_boolean_type*/(self,elem_bits);
      return mtcs_tree_build_vector_type_for_mode/*!build_vector_type_for_mode*/(self,bool_type, mode);
   }else if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)
         && valid_vector_subparts_p (mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode))){
      machine_mode inner_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
      tree inner_type = typeForMode(self,inner_mode, unsigned_p);
      if (inner_type != NULL_TREE)
         return mtcs_tree_build_vector_type_for_mode/*!build_vector_type_for_mode*/(self,inner_type, mode);
   }

   if (mtcs_dfloat32_type_node != NULL_TREE  && mode == TYPE_MODE (mtcs_dfloat32_type_node))
      return mtcs_dfloat32_type_node;
   if (mtcs_dfloat64_type_node != NULL_TREE && mode == TYPE_MODE (mtcs_dfloat64_type_node))
      return mtcs_dfloat64_type_node;
   if (mtcs_dfloat128_type_node != NULL_TREE  && mode == TYPE_MODE (mtcs_dfloat128_type_node))
      return mtcs_dfloat128_type_node;

   if (mtcs_mode_is_all_scalar_fixed_point_p/*!ALL_SCALAR_FIXED_POINT_MODE_P*/(mtcsMode,mode)){
      if (mode == TYPE_MODE (mtcs_short_fract_type_node))
         return unsigned_p ? mtcs_sat_short_fract_type_node : mtcs_short_fract_type_node;
      if (mode == TYPE_MODE (mtcs_fract_type_node))
         return unsigned_p ? mtcs_sat_fract_type_node : mtcs_fract_type_node;
      if (mode == TYPE_MODE (mtcs_long_fract_type_node))
         return unsigned_p ? mtcs_sat_long_fract_type_node : mtcs_long_fract_type_node;
      if (mode == TYPE_MODE (mtcs_long_long_fract_type_node))
         return unsigned_p ? mtcs_sat_long_long_fract_type_node : mtcs_long_long_fract_type_node;

      if (mode == TYPE_MODE (mtcs_unsigned_short_fract_type_node))
         return unsigned_p ? mtcs_sat_unsigned_short_fract_type_node : mtcs_unsigned_short_fract_type_node;
      if (mode == TYPE_MODE (mtcs_unsigned_fract_type_node))
         return unsigned_p ? mtcs_sat_unsigned_fract_type_node  : mtcs_unsigned_fract_type_node;
      if (mode == TYPE_MODE (mtcs_unsigned_long_fract_type_node))
         return unsigned_p ? mtcs_sat_unsigned_long_fract_type_node : mtcs_unsigned_long_fract_type_node;
      if (mode == TYPE_MODE (mtcs_unsigned_long_long_fract_type_node))
         return unsigned_p ? mtcs_sat_unsigned_long_long_fract_type_node : mtcs_unsigned_long_long_fract_type_node;

      if (mode == TYPE_MODE (mtcs_short_accum_type_node))
         return unsigned_p ? mtcs_sat_short_accum_type_node : mtcs_short_accum_type_node;
      if (mode == TYPE_MODE (mtcs_accum_type_node))
         return unsigned_p ? mtcs_sat_accum_type_node : mtcs_accum_type_node;
      if (mode == TYPE_MODE (mtcs_long_accum_type_node))
         return unsigned_p ? mtcs_sat_long_accum_type_node : mtcs_long_accum_type_node;
      if (mode == TYPE_MODE (mtcs_long_long_accum_type_node))
         return unsigned_p ? mtcs_sat_long_long_accum_type_node : mtcs_long_long_accum_type_node;

      if (mode == TYPE_MODE (mtcs_unsigned_short_accum_type_node))
         return unsigned_p ? mtcs_sat_unsigned_short_accum_type_node : mtcs_unsigned_short_accum_type_node;
      if (mode == TYPE_MODE (mtcs_unsigned_accum_type_node))
         return unsigned_p ? mtcs_sat_unsigned_accum_type_node : mtcs_unsigned_accum_type_node;
      if (mode == TYPE_MODE (mtcs_unsigned_long_accum_type_node))
         return unsigned_p ? mtcs_sat_unsigned_long_accum_type_node: mtcs_unsigned_long_accum_type_node;
      if (mode == TYPE_MODE (mtcs_unsigned_long_long_accum_type_node))
         return unsigned_p ? mtcs_sat_unsigned_long_long_accum_type_node : mtcs_unsigned_long_long_accum_type_node;

      if (mode == mtcsMode->modes.M_QQmode)
         return unsigned_p ? mtcs_sat_qq_type_node : mtcs_qq_type_node;
      if (mode == mtcsMode->modes.M_HQmode)
         return unsigned_p ? mtcs_sat_hq_type_node : mtcs_hq_type_node;
      if (mode == mtcsMode->modes.M_SQmode)
         return unsigned_p ? mtcs_sat_sq_type_node : mtcs_sq_type_node;
      if (mode == mtcsMode->modes.M_DQmode)
         return unsigned_p ? mtcs_sat_dq_type_node : mtcs_dq_type_node;
      if (mode == mtcsMode->modes.M_TQmode)
         return unsigned_p ? mtcs_sat_tq_type_node : mtcs_tq_type_node;

      if (mode == mtcsMode->modes.M_UQQmode)
         return unsigned_p ? mtcs_sat_uqq_type_node : mtcs_uqq_type_node;
      if (mode == mtcsMode->modes.M_UHQmode)
         return unsigned_p ? mtcs_sat_uhq_type_node : mtcs_uhq_type_node;
      if (mode == mtcsMode->modes.M_USQmode)
         return unsigned_p ? mtcs_sat_usq_type_node : mtcs_usq_type_node;
      if (mode == mtcsMode->modes.M_UDQmode)
         return unsigned_p ? mtcs_sat_udq_type_node : mtcs_udq_type_node;
      if (mode == mtcsMode->modes.M_UTQmode)
         return unsigned_p ? mtcs_sat_utq_type_node : mtcs_utq_type_node;

      if (mode == mtcsMode->modes.M_HAmode)
         return unsigned_p ? mtcs_sat_ha_type_node : mtcs_ha_type_node;
      if (mode == mtcsMode->modes.M_SAmode)
         return unsigned_p ? mtcs_sat_sa_type_node : mtcs_sa_type_node;
      if (mode == mtcsMode->modes.M_DAmode)
         return unsigned_p ? mtcs_sat_da_type_node : mtcs_da_type_node;
      if (mode == mtcsMode->modes.M_TAmode)
         return unsigned_p ? mtcs_sat_ta_type_node : mtcs_ta_type_node;

      if (mode == mtcsMode->modes.M_UHAmode)
         return unsigned_p ? mtcs_sat_uha_type_node : mtcs_uha_type_node;
      if (mode == mtcsMode->modes.M_USAmode)
         return unsigned_p ? mtcs_sat_usa_type_node : mtcs_usa_type_node;
      if (mode == mtcsMode->modes.M_UDAmode)
         return unsigned_p ? mtcs_sat_uda_type_node : mtcs_uda_type_node;
      if (mode == mtcsMode->modes.M_UTAmode)
         return unsigned_p ? mtcs_sat_uta_type_node : mtcs_uta_type_node;
   }

   for (t = self->registered_builtin_types; t; t = TREE_CHAIN (t)){
      tree type = TREE_VALUE (t);
      if (TYPE_MODE (type) == mode  && VECTOR_TYPE_P (type) ==mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/(mtcsMode,mode)
            && !!unsigned_p == !!TYPE_UNSIGNED (type))
      return type;
   }
   return NULL_TREE;
}


//原型init_internal_fns internal-fn.h internal-fn.cc
static void initInternalFns (MtcsTree *self)
{
#define DEF_INTERNAL_FN(CODE, FLAGS, FNSPEC) \
  if (FNSPEC) self->internal_fn_fnspec_array[IFN_##CODE] = \
    build_string ((int) sizeof (FNSPEC) - 1, FNSPEC ? FNSPEC : "");
#include "internal-fn.def"
  self->internal_fn_fnspec_array[IFN_LAST] = 0;
}


/* Call this function after instantiating all builtins that the language
   front end cares about.  This will build the rest of the builtins
   and internal functions that are relied upon by the tree optimizers and
   the middle-end.  */
//原型 build_common_builtin_nodes tree.h tree.cc
//调用前需要备份global_trees等主机的tree
void mtcs_tree_build_common_builtin_nodes (MtcsTree *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   tree tmp, ftype;
   int ecf_flags;
   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_CLEAR_PADDING)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node,   ptr_type_node,
      ptr_type_node, integer_type_node, NULL_TREE);
      local_define_builtin(self,"__builtin_clear_padding", ftype,
            BUILT_IN_CLEAR_PADDING, "__builtin_clear_padding", ECF_LEAF | ECF_NOTHROW);
   }

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_UNREACHABLE)
   || !mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_TRAP)
   || !mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_UNREACHABLE_TRAP)
   || !mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_ABORT)){
      ftype = mtcs_tree_build_function_type/*!build_function_type*/(self,void_type_node, void_list_node);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_UNREACHABLE))
         local_define_builtin(self,"__builtin_unreachable", ftype, BUILT_IN_UNREACHABLE,  "__builtin_unreachable",
               ECF_NOTHROW | ECF_LEAF | ECF_NORETURN | ECF_CONST | ECF_COLD);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_UNREACHABLE_TRAP))
         local_define_builtin(self,"__builtin_unreachable trap", ftype,
               BUILT_IN_UNREACHABLE_TRAP, "__builtin_unreachable trap",
               ECF_NOTHROW | ECF_LEAF | ECF_NORETURN | ECF_CONST | ECF_COLD);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_ABORT))
         local_define_builtin(self,"__builtin_abort", ftype, BUILT_IN_ABORT,
               "abort", ECF_LEAF | ECF_NORETURN | ECF_CONST | ECF_COLD);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_TRAP))
         local_define_builtin(self,"__builtin_trap", ftype, BUILT_IN_TRAP,
               "__builtin_trap", ECF_NORETURN | ECF_NOTHROW | ECF_LEAF | ECF_COLD);
   }

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_MEMCPY)
   || !mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_MEMMOVE)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,ptr_type_node,
      ptr_type_node, const_ptr_type_node,size_type_node, NULL_TREE);

      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_MEMCPY))
         local_define_builtin(self,"__builtin_memcpy", ftype, BUILT_IN_MEMCPY,
               "memcpy", ECF_NOTHROW | ECF_LEAF);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_MEMMOVE))
         local_define_builtin(self,"__builtin_memmove", ftype, BUILT_IN_MEMMOVE,
               "memmove", ECF_NOTHROW | ECF_LEAF);
   }

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_MEMCMP)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
            integer_type_node, const_ptr_type_node,const_ptr_type_node, size_type_node, NULL_TREE);
      local_define_builtin(self,"__builtin_memcmp", ftype, BUILT_IN_MEMCMP,"memcmp", ECF_PURE | ECF_NOTHROW | ECF_LEAF);
   }

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_MEMSET)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
            ptr_type_node,ptr_type_node, integer_type_node, size_type_node, NULL_TREE);
      local_define_builtin(self,"__builtin_memset", ftype, BUILT_IN_MEMSET,"memset", ECF_NOTHROW | ECF_LEAF);
   }

   /* If we're checking the stack, `alloca' can throw.  */
   const int alloca_flags = ECF_MALLOC | ECF_LEAF | (flag_stack_check ? 0 : ECF_NOTHROW);

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_ALLOCA)) {
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
            ptr_type_node,  size_type_node, NULL_TREE);
      local_define_builtin(self,"__builtin_alloca", ftype, BUILT_IN_ALLOCA,"alloca", alloca_flags);
   }

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
         ptr_type_node, size_type_node, size_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_alloca_with_align", ftype,
         BUILT_IN_ALLOCA_WITH_ALIGN, "__builtin_alloca_with_align", alloca_flags);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
         ptr_type_node, size_type_node,size_type_node, size_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_alloca_with_align_and_max", ftype,
         BUILT_IN_ALLOCA_WITH_ALIGN_AND_MAX, "__builtin_alloca_with_align_and_max",alloca_flags);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
         void_type_node,ptr_type_node, ptr_type_node, ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_init_trampoline", ftype,
         BUILT_IN_INIT_TRAMPOLINE, "__builtin_init_trampoline", ECF_NOTHROW | ECF_LEAF);
   local_define_builtin(self,"__builtin_init_heap_trampoline", ftype,
         BUILT_IN_INIT_HEAP_TRAMPOLINE,  "__builtin_init_heap_trampoline",ECF_NOTHROW | ECF_LEAF);
   local_define_builtin(self,"__builtin_init_descriptor", ftype,
         BUILT_IN_INIT_DESCRIPTOR, "__builtin_init_descriptor", ECF_NOTHROW | ECF_LEAF);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,ptr_type_node, ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_adjust_trampoline", ftype,
         BUILT_IN_ADJUST_TRAMPOLINE, "__builtin_adjust_trampoline", ECF_CONST | ECF_NOTHROW);
   local_define_builtin(self,"__builtin_adjust_descriptor", ftype,
         BUILT_IN_ADJUST_DESCRIPTOR,"__builtin_adjust_descriptor",ECF_CONST | ECF_NOTHROW);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
         void_type_node,ptr_type_node, ptr_type_node, NULL_TREE);
   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_CLEAR_CACHE))
      local_define_builtin(self,"__builtin___clear_cache", ftype,
            BUILT_IN_CLEAR_CACHE, "__clear_cache",ECF_NOTHROW);

   local_define_builtin(self,"__builtin_nonlocal_goto", ftype,
         BUILT_IN_NONLOCAL_GOTO, "__builtin_nonlocal_goto", ECF_NORETURN | ECF_NOTHROW);

   tree ptr_ptr_type_node = mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,ptr_type_node);

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_GCC_NESTED_PTR_CREATED)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node,
      ptr_type_node, // void *chain
      ptr_type_node, // void *func
      ptr_ptr_type_node, // void **dst
      NULL_TREE);
      local_define_builtin(self,"__builtin___gcc_nested_func_ptr_created", ftype,
            BUILT_IN_GCC_NESTED_PTR_CREATED,     "__gcc_nested_func_ptr_created", ECF_NOTHROW);
   }

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_GCC_NESTED_PTR_DELETED)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node, NULL_TREE);
      local_define_builtin(self,"__builtin___gcc_nested_func_ptr_deleted", ftype,
            BUILT_IN_GCC_NESTED_PTR_DELETED, "__gcc_nested_func_ptr_deleted", ECF_NOTHROW);
   }

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
         void_type_node,ptr_type_node, ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_setjmp_setup", ftype,
         BUILT_IN_SETJMP_SETUP,  "__builtin_setjmp_setup", ECF_NOTHROW);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node, ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_setjmp_receiver", ftype,
         BUILT_IN_SETJMP_RECEIVER, "__builtin_setjmp_receiver", ECF_NOTHROW | ECF_LEAF);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_stack_save", ftype, BUILT_IN_STACK_SAVE,
         "__builtin_stack_save", ECF_NOTHROW | ECF_LEAF);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node, ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_stack_restore", ftype,
         BUILT_IN_STACK_RESTORE,"__builtin_stack_restore", ECF_NOTHROW | ECF_LEAF);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,integer_type_node, const_ptr_type_node,
         const_ptr_type_node, size_type_node,NULL_TREE);
   local_define_builtin(self,"__builtin_memcmp_eq", ftype, BUILT_IN_MEMCMP_EQ,
         "__builtin_memcmp_eq",ECF_PURE | ECF_NOTHROW | ECF_LEAF);

   local_define_builtin(self,"__builtin_strncmp_eq", ftype, BUILT_IN_STRNCMP_EQ,
         "__builtin_strncmp_eq",ECF_PURE | ECF_NOTHROW | ECF_LEAF);

   local_define_builtin(self,"__builtin_strcmp_eq", ftype, BUILT_IN_STRCMP_EQ,
         "__builtin_strcmp_eq",ECF_PURE | ECF_NOTHROW | ECF_LEAF);

   /* If there's a possibility that we might use the ARM EABI, build the
   alternate __cxa_end_cleanup node used to resume from C++.  */
   if (mtcsTarget/*!targetm.arm_eabi_unwinder*/->arm_eabi_unwinder){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node, NULL_TREE);
      local_define_builtin(self,"__builtin_cxa_end_cleanup", ftype,
            BUILT_IN_CXA_END_CLEANUP, "__cxa_end_cleanup", ECF_NORETURN | ECF_XTHROW | ECF_LEAF);
   }

   ftype =mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,void_type_node, ptr_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_unwind_resume", ftype,
      BUILT_IN_UNWIND_RESUME,((target_common_except_unwind_info/*!targetm_common.except_unwind_info*/(
      mtcsMachine->common,mtcsOptionsItem)== UI_SJLJ)  ? "_Unwind_SjLj_Resume" : "_Unwind_Resume"),ECF_NORETURN | ECF_XTHROW);

   if (mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_RETURN_ADDRESS) == NULL_TREE){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,ptr_type_node, integer_type_node,NULL_TREE);
      local_define_builtin(self,"__builtin_return_address", ftype,
            BUILT_IN_RETURN_ADDRESS, "__builtin_return_address", ECF_NOTHROW);
   }

   if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_PROFILE_FUNC_ENTER)
   || !mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_PROFILE_FUNC_EXIT)){
      ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
            void_type_node, ptr_type_node,ptr_type_node, NULL_TREE);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_PROFILE_FUNC_ENTER))
         local_define_builtin(self,"__cyg_profile_func_enter", ftype,
               BUILT_IN_PROFILE_FUNC_ENTER, "__cyg_profile_func_enter", 0);
      if (!mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_PROFILE_FUNC_EXIT))
         local_define_builtin(self,"__cyg_profile_func_exit", ftype,
               BUILT_IN_PROFILE_FUNC_EXIT, "__cyg_profile_func_exit", 0);
   }

   /* The exception object and filter values from the runtime.  The argument
   must be zero before exception lowering, i.e. from the front end.  After
   exception lowering, it will be the region number for the exception
   landing pad.  These functions are PURE instead of CONST to prevent
   them from being hoisted past the exception edge that will initialize
   its value in the landing pad.  */
   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,ptr_type_node,integer_type_node, NULL_TREE);
   ecf_flags = ECF_PURE | ECF_NOTHROW | ECF_LEAF;
   /* Only use TM_PURE if we have TM language support.  */
   if (mtcs_tree_builtin_decl_explicit_p/*!builtin_decl_explicit_p*/(self,BUILT_IN_TM_LOAD_1))
      ecf_flags |= ECF_TM_PURE;
   local_define_builtin(self,"__builtin_eh_pointer", ftype, BUILT_IN_EH_POINTER,
         "__builtin_eh_pointer", ecf_flags);

   tmp = lang_hooks.types.type_for_mode (targetm.eh_return_filter_mode (), 0);
   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,tmp, integer_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_eh_filter", ftype, BUILT_IN_EH_FILTER,
         "__builtin_eh_filter", ECF_PURE | ECF_NOTHROW | ECF_LEAF);

   ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
         void_type_node,integer_type_node, integer_type_node, NULL_TREE);
   local_define_builtin(self,"__builtin_eh_copy_values", ftype,
         BUILT_IN_EH_COPY_VALUES, "__builtin_eh_copy_values", ECF_NOTHROW);

   /* Complex multiplication and division.  These are handled as builtins
   rather than optabs because emit_library_call_value doesn't support
   complex.  Further, we can do slightly better with folding these
   beasties if the real and complex parts of the arguments are separate.  */
   {
      int mode;

      for (mode =mtcsMode->modesMinMax.min_COMPLEX_FLOAT/*!MIN_MODE_COMPLEX_FLOAT*/;
            mode <= mtcsMode->modesMinMax.max_COMPLEX_FLOAT/*!MAX_MODE_COMPLEX_FLOAT*/; ++mode){
         char mode_name_buf[4], *q;
         const char *p;
         enum built_in_function mcode, dcode;
         tree type, inner_type;
         const char *prefix = "__";

         if (mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix)
            prefix = "__gnu_";
         type =typeForMode/*lang_hooks.types.type_for_mode*/(self,(machine_mode) mode, 0);
         if (type == NULL)
            continue;
         inner_type = TREE_TYPE (type);

         ftype = mtcs_tree_build_function_type_list/*!build_function_type_list*/(self,
               type, inner_type, inner_type,inner_type, inner_type, NULL_TREE);

         mcode = ((enum built_in_function) (BUILT_IN_COMPLEX_MUL_MIN + mode -
               mtcsMode->modesMinMax.min_COMPLEX_FLOAT/*!MIN_MODE_COMPLEX_FLOAT*/));
         dcode = ((enum built_in_function) (BUILT_IN_COMPLEX_DIV_MIN + mode -
               mtcsMode->modesMinMax.min_COMPLEX_FLOAT/*!MIN_MODE_COMPLEX_FLOAT*/));

         for (p = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,mode), q = mode_name_buf; *p; p++, q++)
            *q = TOLOWER (*p);
         *q = '\0';

         /* For -ftrapping-math these should throw from a former  -fnon-call-exception stmt.  */
         self->built_in_names[mcode] = concat (prefix, "mul", mode_name_buf, "3", NULL);
         local_define_builtin(self,self->built_in_names[mcode], ftype, mcode,
               self->built_in_names[mcode], ECF_CONST | ECF_LEAF);

         self->built_in_names[dcode] = concat (prefix, "div", mode_name_buf, "3", NULL);
         local_define_builtin(self,self->built_in_names[dcode], ftype, dcode,
               self->built_in_names[dcode],ECF_CONST | ECF_LEAF);
      }
   }
   initInternalFns(self);
}

//原型 size_int fold-const.h fold-const.hh
tree  mtcs_tree_get_size_int(MtcsTree *self,int size)
{
  return mtcs_tree_size_int(self,size);
}

tree  mtcs_tree_get_size_int(MtcsTree *self,poly_int64 size)
{
  return mtcs_tree_size_int(self,size);
}

//原型 bitsize_int fold-const.h fold-const.hh
tree  mtcs_tree_get_bitsize_int(MtcsTree *self,int size)
{
  return mtcs_tree_bitsize_int(self,size);
}

//原型 lto_type_for_mode lto-lang.cc #define LANG_HOOKS_TYPE_FOR_MODE lto_type_for_mode
tree mtcs_tree_type_for_mode(MtcsTree *self,machine_mode mode, int unsigned_p)
{
   return typeForMode(self,mode,unsigned_p);
}

//原型 #define LANG_HOOKS_UNIT_SIZE_WITHOUT_REUSABLE_PADDING lhd_unit_size_without_reusable_padding
tree mtcs_tree_unit_size_without_reusable_padding(MtcsTree *self,tree t)
{
   return TYPE_SIZE_UNIT (t);
}

/**********************------以下是创建各种tree的方法----------------------------*/

/* Create a DECL_... node of code CODE, name NAME  (if non-null)
   and data type TYPE.
   We do NOT enter this node in any sort of symbol table.

   LOC is the location of the decl.

   layout_decl is used to set up the decl's storage layout.
   Other slots are initialized to 0 or null pointers.  */
//原型 build_decl tree.h tree.cc
tree mtcs_tree_build_decl(MtcsTree *self,location_t loc, enum tree_code code, tree name,
          tree type MEM_STAT_DECL)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   tree t;
   t = make_node (code PASS_MEM_STAT);
   if(code==FUNCTION_DECL){
      int funcitonBoundary=mtcs_func_get_function_boundary(mtcsFunc);
      int align=mtcs_align_get_function_alignment(mtcsAlign,funcitonBoundary);
      SET_DECL_ALIGN (t, align/*!FUNCTION_ALIGNMENT (FUNCTION_BOUNDARY)*/);
      SET_DECL_MODE (t, mtcs_mode_get_function_mode/*!FUNCTION_MODE*/(mtcsMode));
      //fprintf(stderr,"mtcstree.c mtcs_tree_build_decl function_decl类型 aling:%d mode:%d\n",align,DECL_MODE(t));
   }
   DECL_SOURCE_LOCATION (t) = loc;
   /*  if (type == error_mark_node)
   type = integer_type_node; */
   /* That is not done, deliberately, so that having error_mark_node
   as the type can suppress useless errors in the use of this variable.  */
   DECL_NAME (t) = name;
   TREE_TYPE (t) = type;
   if (code == VAR_DECL || code == PARM_DECL || code == RESULT_DECL)
      mtcs_stor_layout_layout_decl/*!layout_decl*/(mtcsStorLayout,t, 0);
   return t;
}

/* Same as build_pointer_type_for_mode, but for REFERENCE_TYPE.  */
//原型 build_reference_type_for_mode tree.h tree.cc
tree mtcs_tree_build_reference_type_for_mode (MtcsTree *self,tree to_type, machine_mode mode,bool can_alias_all)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsTree *mtcsTree=self;

   tree t;
   bool could_alias = can_alias_all;

   if (to_type == mtcs_error_mark_node)
   return mtcs_error_mark_node;

   if (mode == VOIDmode){
      addr_space_t as = TYPE_ADDR_SPACE (to_type);
      mode =target_addr_space_pointer_mode/*!targetm.addr_space.pointer_mode*/(mtcsMachine->addrSpace,as);
   }

   /* If the pointed-to type has the may_alias attribute set, force
   a TYPE_REF_CAN_ALIAS_ALL pointer to be generated.  */
   if (lookup_attribute ("may_alias", TYPE_ATTRIBUTES (to_type)))
      can_alias_all = true;

   /* In some cases, languages will have things that aren't a REFERENCE_TYPE
   (such as a RECORD_TYPE for fat pointers in Ada) as TYPE_REFERENCE_TO.
   In that case, return that type without regard to the rest of our
   operands.

   ??? This is a kludge, but consistent with the way this function has
   always operated and there doesn't seem to be a good way to avoid this
   at the moment.  */
   if (TYPE_REFERENCE_TO (to_type) != 0  && TREE_CODE (TYPE_REFERENCE_TO (to_type)) != REFERENCE_TYPE)
      return TYPE_REFERENCE_TO (to_type);

   /* First, if we already have a type for pointers to TO_TYPE and it's
   the proper mode, use it.  */
   for (t = TYPE_REFERENCE_TO (to_type); t; t = TYPE_NEXT_REF_TO (t))
      if (TYPE_MODE (t) == mode && TYPE_REF_CAN_ALIAS_ALL (t) == can_alias_all)
         return t;

   t = make_node (REFERENCE_TYPE);

   TREE_TYPE (t) = to_type;
   SET_TYPE_MODE (t, mode);
   TYPE_REF_CAN_ALIAS_ALL (t) = can_alias_all;
   TYPE_NEXT_REF_TO (t) = TYPE_REFERENCE_TO (to_type);
   TYPE_REFERENCE_TO (to_type) = t;

   /* During LTO we do not set TYPE_CANONICAL of pointers and references.  */
   if (TYPE_STRUCTURAL_EQUALITY_P (to_type) || in_lto_p)
      SET_TYPE_STRUCTURAL_EQUALITY (t);
   else if (TYPE_CANONICAL (to_type) != to_type || could_alias)
      TYPE_CANONICAL (t) =mtcs_tree_build_reference_type_for_mode/*!build_reference_type_for_mode*/(self,
                        TYPE_CANONICAL (to_type), mode, false);

   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,t);

   return t;
}



/* Build the node for the type of references-to-TO_TYPE by default
   in ptr_mode.  */
//原型 build_reference_type tree.h tree.cc
tree mtcs_tree_build_reference_type (MtcsTree *self,tree to_type)
{
  return mtcs_tree_build_reference_type_for_mode/*!build_reference_type_for_mode*/(self,
        to_type, VOIDmode, false);
}

/* Constructors for pointer, array and function types.
   (RECORD_TYPE, UNION_TYPE and ENUMERAL_TYPE nodes are
   constructed by language-dependent code, not here.)  */

/* Construct, lay out and return the type of pointers to TO_TYPE with
   mode MODE.  If MODE is VOIDmode, a pointer mode for the address
   space of TO_TYPE will be picked.  If CAN_ALIAS_ALL is TRUE,
   indicate this type can reference all of memory. If such a type has
   already been constructed, reuse it.  */
//原型 build_pointer_type_for_mode tree.h tree.cc
tree mtcs_tree_build_pointer_type_for_mode (MtcsTree *self,tree to_type, machine_mode mode,
              bool can_alias_all)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   MtcsTree *mtcsTree=self;

   tree t;
   bool could_alias = can_alias_all;
   if (to_type == mtcs_error_mark_node)
      return mtcs_error_mark_node;

   if (mode == VOIDmode){
      addr_space_t as = TYPE_ADDR_SPACE (to_type);
      mode = target_addr_space_pointer_mode/*!targetm.addr_space.pointer_mode*/(mtcsMachine->addrSpace,as);
   }

   /* If the pointed-to type has the may_alias attribute set, force
   a TYPE_REF_CAN_ALIAS_ALL pointer to be generated.  */
   if (lookup_attribute ("may_alias", TYPE_ATTRIBUTES (to_type)))
      can_alias_all = true;

   /* In some cases, languages will have things that aren't a POINTER_TYPE
   (such as a RECORD_TYPE for fat pointers in Ada) as TYPE_POINTER_TO.
   In that case, return that type without regard to the rest of our
   operands.

   ??? This is a kludge, but consistent with the way this function has
   always operated and there doesn't seem to be a good way to avoid this
   at the moment.  */
   if (TYPE_POINTER_TO (to_type) != 0  && TREE_CODE (TYPE_POINTER_TO (to_type)) != POINTER_TYPE)
      return TYPE_POINTER_TO (to_type);

   /* First, if we already have a type for pointers to TO_TYPE and it's
   the proper mode, use it.  */
   for (t = TYPE_POINTER_TO (to_type); t; t = TYPE_NEXT_PTR_TO (t))
      if (TYPE_MODE (t) == mode && TYPE_REF_CAN_ALIAS_ALL (t) == can_alias_all)
         return t;

   t = make_node (POINTER_TYPE);

   TREE_TYPE (t) = to_type;
   SET_TYPE_MODE (t, mode);
   TYPE_REF_CAN_ALIAS_ALL (t) = can_alias_all;
   TYPE_NEXT_PTR_TO (t) = TYPE_POINTER_TO (to_type);
   TYPE_POINTER_TO (to_type) = t;

   /* During LTO we do not set TYPE_CANONICAL of pointers and references.  */
   if (TYPE_STRUCTURAL_EQUALITY_P (to_type) || in_lto_p)
      SET_TYPE_STRUCTURAL_EQUALITY (t);
   else if (TYPE_CANONICAL (to_type) != to_type || could_alias)
      TYPE_CANONICAL (t) = mtcs_tree_build_pointer_type_for_mode/*!build_pointer_type_for_mode*/(self,
                     TYPE_CANONICAL (to_type),mode, false);

   /* Lay out the type.  This function has many callers that are concerned
   with expression-construction, and this simplifies them all.  */
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,t);

   return t;
}

/* By default build pointers in ptr_mode.  */
//原型 build_pointer_type tree.h tree.cc
tree mtcs_tree_build_pointer_type (MtcsTree *self,tree to_type)
{
  return mtcs_tree_build_pointer_type_for_mode/*!build_pointer_type_for_mode*/(self,to_type, VOIDmode, false);
}

//原型 build_type_variant tree.h
//#define build_type_variant(TYPE, CONST_P, VOLATILE_P)       \
//  build_qualified_type ((TYPE),                 \
//         ((CONST_P) ? TYPE_QUAL_CONST : 0)      \
//         | ((VOLATILE_P) ? TYPE_QUAL_VOLATILE : 0))
tree mtcs_tree_build_type_variant(MtcsTree *self,tree type,int const_p,int volatile_p)
{
   int type_quals=((const_p) ? TYPE_QUAL_CONST : 0)| ((volatile_p) ? TYPE_QUAL_VOLATILE : 0);
   return mtcs_tree_build_qualified_type(self,type,type_quals);
}

//原型 build_qualified_type tree.h tree.cc
tree mtcs_tree_build_qualified_type(MtcsTree *self,tree type, int type_quals MEM_STAT_DECL)
{
   tree t;
   /* See if we already have the appropriate qualified variant.  */
   t = mtcs_tree_get_qualified_type/*!get_qualified_type*/(self,type, type_quals);
   /* If not, build it.  */
   if (!t){
      t = build_variant_type_copy (type PASS_MEM_STAT);
      set_type_quals (t, type_quals);

      if (((type_quals & TYPE_QUAL_ATOMIC) == TYPE_QUAL_ATOMIC)){
         /* See if this object can map to a basic atomic type.  */
         tree atomic_type = find_atomic_core_type(self,type);
         if (atomic_type){
            /* Ensure the alignment of this type is compatible with
            the required alignment of the atomic type.  */
            if (TYPE_ALIGN (atomic_type) > TYPE_ALIGN (t))
               SET_TYPE_ALIGN (t, TYPE_ALIGN (atomic_type));
         }
      }

      if (TYPE_STRUCTURAL_EQUALITY_P (type))
         /* Propagate structural equality. */
         SET_TYPE_STRUCTURAL_EQUALITY (t);
      else if (TYPE_CANONICAL (type) != type)
      /* Build the underlying canonical type, since it is different
      from TYPE. */
      {
         tree c = mtcs_tree_build_qualified_type/*!build_qualified_type*/(self,TYPE_CANONICAL (type), type_quals);
         TYPE_CANONICAL (t) = TYPE_CANONICAL (c);
      }else
         /* T is its own canonical type. */
         TYPE_CANONICAL (t) = t;
   }
   return t;
}


/* Build a function type.  RETURN_TYPE is the type returned by the
   function; VAARGS indicates whether the function takes varargs.  The
   function takes N named arguments, the types of which are provided in
   ARG_TYPES.  */
//原型 build_function_type_array_1 tree.cc
static tree build_function_type_array_1 (MtcsTree *self,bool vaargs, tree return_type, int n,
              tree *arg_types)
{
  MtcsTree *mtcsTree=self;
  int i;
  tree t = vaargs ? NULL_TREE : mtcs_void_list_node;
  for (i = n - 1; i >= 0; i--)
    t = tree_cons (NULL_TREE, arg_types[i], t);

  return mtcs_tree_build_function_type/*!build_function_type*/(self,return_type, t, vaargs && n == 0);
}

/* Computes the canonical argument types from the argument type list
   ARGTYPES.

   Upon return, *ANY_STRUCTURAL_P will be true iff either it was true
   on entry to this function, or if any of the ARGTYPES are
   structural.

   Upon return, *ANY_NONCANONICAL_P will be true iff either it was
   true on entry to this function, or if any of the ARGTYPES are
   non-canonical.

   Returns a canonical argument list, which may be ARGTYPES when the
   canonical argument list is unneeded (i.e., *ANY_STRUCTURAL_P is
   true) or would not differ from ARGTYPES.  */
//原型 maybe_canonicalize_argtypes
static tree maybe_canonicalize_argtypes (MtcsTree *self,tree argtypes,
              bool *any_structural_p,bool *any_noncanonical_p)
{
   MtcsTree *mtcsTree=self;
   tree arg;
   bool any_noncanonical_argtypes_p = false;

   for (arg = argtypes; arg && !(*any_structural_p); arg = TREE_CHAIN (arg)){
      if (!TREE_VALUE (arg) || TREE_VALUE (arg) == error_mark_node)
         /* Fail gracefully by stating that the type is structural.  */
         *any_structural_p = true;
      else if (TYPE_STRUCTURAL_EQUALITY_P (TREE_VALUE (arg)))
         *any_structural_p = true;
      else if (TYPE_CANONICAL (TREE_VALUE (arg)) != TREE_VALUE (arg) || TREE_PURPOSE (arg))
         /* If the argument has a default argument, we consider it
         non-canonical even though the type itself is canonical.
         That way, different variants of function and method types
         with default arguments will all point to the variant with
         no defaults as their canonical type.  */
         any_noncanonical_argtypes_p = true;
   }

   if (*any_structural_p)
      return argtypes;

   if (any_noncanonical_argtypes_p){
      /* Build the canonical list of argument types.  */
      tree canon_argtypes = NULL_TREE;
      bool is_void = false;

      for (arg = argtypes; arg; arg = TREE_CHAIN (arg)){
         if (arg == mtcs_void_list_node)
            is_void = true;
         else
            canon_argtypes = tree_cons (NULL_TREE, TYPE_CANONICAL (TREE_VALUE (arg)),
         canon_argtypes);
      }

      canon_argtypes = nreverse (canon_argtypes);
      if (is_void)
         canon_argtypes = chainon (canon_argtypes, void_list_node);

      /* There is a non-canonical type.  */
      *any_noncanonical_p = true;
      return canon_argtypes;
   }
   /* The canonical argument types are the same as ARGTYPES.  */
   return argtypes;
}



/* Build a function type.  RETURN_TYPE is the type returned by the
   function.  The function takes N named arguments, the types of which
   are provided in ARG_TYPES.  */
//原型 build_function_type_array tree.h tree.cc
tree mtcs_tree_build_function_type_array (MtcsTree *self,tree return_type, int n, tree *arg_types)
{
  return build_function_type_array_1(self,false, return_type, n, arg_types);
}


/* Build a variable argument function type.  RETURN_TYPE is the type
   returned by the function.  The function takes N named arguments, the
   types of which are provided in ARG_TYPES.  */
//原型 build_varargs_function_type_array tree.h tree.cc
tree mtcs_tree_build_varargs_function_type_array (MtcsTree *self,tree return_type, int n, tree *arg_types)
{
  return build_function_type_array_1(self,true, return_type, n, arg_types);
}

/* Hashing of types so that we don't make duplicates.
   The entry point is `type_hash_canon'.  */

/* Generate the default hash code for TYPE.  This is designed for
   speed, rather than maximum entropy.  */
//原型 type_hash_canon_hash tree.h tree.cc
hashval_t mtcs_tree_type_hash_canon_hash (MtcsTree *self,tree type)
{
   inchash::hash hstate;

   hstate.add_int (TREE_CODE (type));

   if (TREE_TYPE (type))
      hstate.add_object (TYPE_HASH (TREE_TYPE (type)));

   for (tree t = TYPE_ATTRIBUTES (type); t; t = TREE_CHAIN (t))
      /* Just the identifier is adequate to distinguish.  */
      hstate.add_object (IDENTIFIER_HASH_VALUE (get_attribute_name (t)));

   int count=0;
   switch (TREE_CODE (type)){
      case METHOD_TYPE:
         hstate.add_object (TYPE_HASH (TYPE_METHOD_BASETYPE (type)));
      /* FALLTHROUGH. */
      case FUNCTION_TYPE:
         //fprintf(stderr,"mtcs_tree_type_hash_canon_hash 00 count:%d FUNCTION_TYPE %s type:%p\n",
                             //count,get_tree_code_name(TREE_CODE(type)),type);
         for (tree t = TYPE_ARG_TYPES (type); t; t = TREE_CHAIN (t)){
            if (TREE_VALUE (t) != error_mark_node ){
              // fprintf(stderr,"mtcs_tree_type_hash_canon_hash 11 count:%d FUNCTION_TYPE %s type:%p TREE_VALUE (t):%p\n",
                    // count,get_tree_code_name(TREE_CODE(t)),type,TREE_VALUE (t));
               //count++;
               hstate.add_object (TYPE_HASH (TREE_VALUE (t)));
            }
         }
         //fprintf(stderr,"mtcs_tree_type_hash_canon_hash 22 count:%d FUNCTION_TYPE %s type:%p\n",
                                   // count,get_tree_code_name(TREE_CODE(type)),type);
         break;

      case OFFSET_TYPE:
         hstate.add_object (TYPE_HASH (TYPE_OFFSET_BASETYPE (type)));
         break;

      case ARRAY_TYPE:
      {
         if (TYPE_DOMAIN (type))
            hstate.add_object (TYPE_HASH (TYPE_DOMAIN (type)));
         if (!AGGREGATE_TYPE_P (TREE_TYPE (type))){
            unsigned typeless = TYPE_TYPELESS_STORAGE (type);
            hstate.add_object (typeless);
         }
      }
      break;

      case INTEGER_TYPE:
      {
         tree t = TYPE_MAX_VALUE (type);
         if (!t)
            t = TYPE_MIN_VALUE (type);
         for (int i = 0; i < TREE_INT_CST_NUNITS (t); i++)
            hstate.add_object (TREE_INT_CST_ELT (t, i));
         break;
      }

      case BITINT_TYPE:
      {
         unsigned prec = TYPE_PRECISION (type);
         unsigned uns = TYPE_UNSIGNED (type);
         hstate.add_object (prec);
         hstate.add_int (uns);
         break;
      }

      case REAL_TYPE:
      case FIXED_POINT_TYPE:
      {
         unsigned prec = TYPE_PRECISION (type);
         hstate.add_object (prec);
         break;
      }

      case VECTOR_TYPE:
         hstate.add_poly_int (TYPE_VECTOR_SUBPARTS (type));
         break;

      default:
         break;
   }

   return hstate.end ();
}


/* Construct, lay out and return
   the type of functions returning type VALUE_TYPE
   given arguments of types ARG_TYPES.
   ARG_TYPES is a chain of TREE_LIST nodes whose TREE_VALUEs
   are data type nodes for the arguments of the function.
   NO_NAMED_ARGS_STDARG_P is true if this is a prototyped
   variable-arguments function with (...) prototype (no named arguments).
   If such a type has already been constructed, reuse it.  */
//原型 build_function_type tree.h tree.cc
tree mtcs_tree_build_function_type (MtcsTree *self,tree value_type, tree arg_types,
           bool no_named_args_stdarg_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;

   tree t;
   inchash::hash hstate;
   bool any_structural_p, any_noncanonical_p;
   tree canon_argtypes;

   gcc_assert (arg_types != mtcs_error_mark_node);

   if (TREE_CODE (value_type) == FUNCTION_TYPE){
      error ("function return type cannot be function");
      value_type = mtcs_integer_type_node;
   }

   /* Make a node of the sort we want.  */
   t = make_node (FUNCTION_TYPE);
   TREE_TYPE (t) = value_type;
   TYPE_ARG_TYPES (t) = arg_types;
   if (no_named_args_stdarg_p){
      gcc_assert (arg_types == NULL_TREE);
      TYPE_NO_NAMED_ARGS_STDARG_P (t) = 1;
   }

   /* If we already have such a type, use the old one.  */
 //  printf("tx is vvv 00 :%p t:%p %s no_named_args_stdarg_p:%d\n",value_type,t,get_tree_code_name(TREE_CODE(t)),no_named_args_stdarg_p);

   hashval_t hash = mtcs_tree_type_hash_canon_hash/*!type_hash_canon_hash*/(self,t);
   t =mtcs_tree_type_hash_canon/*!type_hash_canon*/(self,hash, t);
 //  printf("tx is vvv 11 :%p t:%p %s no_named_args_stdarg_p:%d hash:%ld\n",
        // value_type,t,get_tree_code_name(TREE_CODE(t)),no_named_args_stdarg_p,hash);

   /* Set up the canonical type. */
   any_structural_p   = TYPE_STRUCTURAL_EQUALITY_P (value_type);
   any_noncanonical_p = TYPE_CANONICAL (value_type) != value_type;
   canon_argtypes = maybe_canonicalize_argtypes(self,arg_types,&any_structural_p, &any_noncanonical_p);
   if (any_structural_p)
      SET_TYPE_STRUCTURAL_EQUALITY (t);
   else if (any_noncanonical_p)
      TYPE_CANONICAL (t) = mtcs_tree_build_function_type/*!build_function_type*/(self,
            TYPE_CANONICAL (value_type),canon_argtypes);

   if (!COMPLETE_TYPE_P (t))
      mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,t);
   return t;
}

/* Given TYPE, and HASHCODE its hash code, return the canonical
   object for an identical type if one already exists.
   Otherwise, return TYPE, and record it as the canonical object.

   To use this function, first create a type of the sort you want.
   Then compute its hash code from the fields of the type that
   make it different from other similar types.
   Then call this function and use the value.  */
//原型 type_hash_canon tree.h tree.cc
tree mtcs_tree_type_hash_canon (MtcsTree *self,unsigned int hashcode, tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);

   mtcs_type_hash in;
   mtcs_type_hash **loc;

   /* The hash table only contains main variants, so ensure that's what we're
   being passed.  */
   gcc_assert (TYPE_MAIN_VARIANT (type) == type);

   /* The TYPE_ALIGN field of a type is set by layout_type(), so we
   must call that routine before comparing TYPE_ALIGNs.  */
   //fprintf(stderr,"mtcstree.c mtcs_tree_type_hash_canon 00 type:%p %s\n",type,get_tree_code_name(TREE_CODE(type)));
   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,type);

   in.hash = hashcode;
   in.type = type;

   loc = self->type_hash_table->find_slot_with_hash (&in, hashcode, INSERT);
   if (*loc){
      tree t1 = ((mtcs_type_hash *) *loc)->type;
      gcc_assert (TYPE_MAIN_VARIANT (t1) == t1   && t1 != type);
      if (TYPE_UID (type) + 1 == self->next_type_uid)
         --self->next_type_uid;
      //fprintf(stderr,"mtcstree.c mtcs_tree_type_hash_canon 11 type:%p %s\n",t1,get_tree_code_name(TREE_CODE(t1)));

      /* Free also min/max values and the cache for integer
      types.  This can't be done in free_node, as LTO frees
      those on its own.  */
      if (TREE_CODE (type) == INTEGER_TYPE || TREE_CODE (type) == BITINT_TYPE){
         if (TYPE_MIN_VALUE (type) && TREE_TYPE (TYPE_MIN_VALUE (type)) == type){
            /* Zero is always in TYPE_CACHED_VALUES.  */
            if (! TYPE_UNSIGNED (type))
               self->int_cst_hash_table->remove_elt (TYPE_MIN_VALUE (type));
            ggc_free (TYPE_MIN_VALUE (type));
         }
         if (TYPE_MAX_VALUE (type) && TREE_TYPE (TYPE_MAX_VALUE (type)) == type){
            self->int_cst_hash_table->remove_elt (TYPE_MAX_VALUE (type));
            ggc_free (TYPE_MAX_VALUE (type));
         }
         if (TYPE_CACHED_VALUES_P (type))
            ggc_free (TYPE_CACHED_VALUES (type));
      }
      free_node (type);
      return t1;
   }else {
      struct mtcs_type_hash *h;
      h = ggc_alloc<mtcs_type_hash> ();
      h->hash = hashcode;
      h->type = type;
      *loc = h;
      return type;
   }
}

/* Return the value that TREE_INT_CST_EXT_NUNITS should have for an
   INTEGER_CST with value CST and type TYPE.   */
//原型 get_int_cst_ext_nunits tree.cc
static unsigned int get_int_cst_ext_nunits (MtcsTree *self,tree type, const wide_int &cst)
{
  gcc_checking_assert (cst.get_precision () == TYPE_PRECISION (type));
  /* We need extra HWIs if CST is an unsigned integer with its
     upper bit set.  */
  if (TYPE_UNSIGNED (type) && wi::neg_p (cst))
    return cst.get_precision () / HOST_BITS_PER_WIDE_INT + 1;
  return cst.get_len ();
}

/* Return a new INTEGER_CST with value CST and type TYPE.  */
//原型 build_new_int_cst tree.cc

static tree build_new_int_cst (MtcsTree *self,tree type, const wide_int &cst)
{
   unsigned int len = cst.get_len ();
   unsigned int ext_len = get_int_cst_ext_nunits(self,type, cst);
   tree nt = make_int_cst (len, ext_len);

   if (len < ext_len){
      --ext_len;
      TREE_INT_CST_ELT (nt, ext_len) = zext_hwi (-1, cst.get_precision () % HOST_BITS_PER_WIDE_INT);
      for (unsigned int i = len; i < ext_len; ++i)
         TREE_INT_CST_ELT (nt, i) = -1;
   }else if (TYPE_UNSIGNED (type) && cst.get_precision () < len * HOST_BITS_PER_WIDE_INT){
      len--;
      TREE_INT_CST_ELT (nt, len) = zext_hwi (cst.elt (len), cst.get_precision () % HOST_BITS_PER_WIDE_INT);
   }

   for (unsigned int i = 0; i < len; i++)
      TREE_INT_CST_ELT (nt, i) = cst.elt (i);
   TREE_TYPE (nt) = type;
   return nt;
}


/* Cache wide_int CST into the TYPE_CACHED_VALUES cache for TYPE.
   SLOT is the slot entry to store it in, and MAX_SLOTS is the maximum
   number of slots that can be cached for the type.  */
//原型 cache_wide_int_in_type_cache tree.cc
static inline tree cache_wide_int_in_type_cache (MtcsTree *self,tree type, const wide_int &cst,
               int slot, int max_slots)
{
   gcc_checking_assert (slot >= 0);
   /* Initialize cache.  */
   if (!TYPE_CACHED_VALUES_P (type)){
      TYPE_CACHED_VALUES_P (type) = 1;
      TYPE_CACHED_VALUES (type) = make_tree_vec (max_slots);
   }
   tree t = TREE_VEC_ELT (TYPE_CACHED_VALUES (type), slot);
   if (!t){
      /* Create a new shared int.  */
      t = build_new_int_cst (self,type, cst);
      TREE_VEC_ELT (TYPE_CACHED_VALUES (type), slot) = t;
   }
   return t;
}

/* Create an INT_CST node of TYPE and value CST.
   The returned node is always shared.  For small integers we use a
   per-type vector cache, for larger ones we use a single hash table.
   The value is extended from its precision according to the sign of
   the type to be a multiple of HOST_BITS_PER_WIDE_INT.  This defines
   the upper bits and ensures that hashing and value equality based
   upon the underlying HOST_WIDE_INTs works without masking.  */
//原型 wide_int_to_tree_1 tree.cc
static tree wide_int_to_tree_1 (MtcsTree *self,tree type, const wide_int_ref &pcst)
{
   tree t;
   int ix = -1;
   int limit = 0;

   gcc_assert (type);
   unsigned int prec = TYPE_PRECISION (type);
   signop sgn = TYPE_SIGN (type);

   /* Verify that everything is canonical.  */
   int l = pcst.get_len ();
   if (l > 1) {
      if (pcst.elt (l - 1) == 0)
         gcc_checking_assert (pcst.elt (l - 2) < 0);
      if (pcst.elt (l - 1) == HOST_WIDE_INT_M1)
         gcc_checking_assert (pcst.elt (l - 2) >= 0);
   }

   wide_int cst = wide_int::from (pcst, prec, sgn);
   unsigned int ext_len = get_int_cst_ext_nunits(self,type, cst);

   enum tree_code code = TREE_CODE (type);
   if (code == POINTER_TYPE || code == REFERENCE_TYPE){
      /* Cache NULL pointer and zero bounds.  */
      if (cst == 0)
         ix = 0;
      /* Cache upper bounds of pointers.  */
      else if (cst == wi::max_value (prec, sgn))
         ix = 1;
      /* Cache 1 which is used for a non-zero range.  */
      else if (cst == 1)
         ix = 2;

      if (ix >= 0){
         t = cache_wide_int_in_type_cache(self,type, cst, ix, 3);
         /* Make sure no one is clobbering the shared constant.  */
         gcc_checking_assert (TREE_TYPE (t) == type  && cst == wi::to_wide (t));
         return t;
      }
   }
   if (ext_len == 1){
      /* We just need to store a single HOST_WIDE_INT.  */
      HOST_WIDE_INT hwi;
      if (TYPE_UNSIGNED (type))
         hwi = cst.to_uhwi ();
      else
         hwi = cst.to_shwi ();

      switch (code){
         case NULLPTR_TYPE:
            gcc_assert (hwi == 0);
         /* Fallthru.  */

         case POINTER_TYPE:
         case REFERENCE_TYPE:
            /* Ignore pointers, as they were already handled above.  */
            break;

         case BOOLEAN_TYPE:
            /* Cache false or true.  */
            limit = 2;
            if (IN_RANGE (hwi, 0, 1))
               ix = hwi;
            break;

         case INTEGER_TYPE:
         case OFFSET_TYPE:
         case BITINT_TYPE:
            if (TYPE_SIGN (type) == UNSIGNED){
               /* Cache [0, N).  */
               limit = param_integer_share_limit;
               if (IN_RANGE (hwi, 0, param_integer_share_limit - 1))
                  ix = hwi;
            }else{
               /* Cache [-1, N).  */
               limit = param_integer_share_limit + 1;
               if (IN_RANGE (hwi, -1, param_integer_share_limit - 1))
                  ix = hwi + 1;
            }
            break;

         case ENUMERAL_TYPE:
            break;

         default:
            gcc_unreachable ();
      }

      if (ix >= 0){
         t = cache_wide_int_in_type_cache(self,type, cst, ix, limit);
         /* Make sure no one is clobbering the shared constant.  */
         gcc_checking_assert (TREE_TYPE (t) == type
            && TREE_INT_CST_NUNITS (t) == 1
            && TREE_INT_CST_EXT_NUNITS (t) == 1
            && TREE_INT_CST_ELT (t, 0) == hwi);
         return t;
      }else{
         /* Use the cache of larger shared ints, using int_cst_node as
         a temporary.  */

         TREE_INT_CST_ELT (self->int_cst_node, 0) = hwi;
         TREE_TYPE (self->int_cst_node) = type;

         tree *slot = self->int_cst_hash_table->find_slot (self->int_cst_node, INSERT);
         t = *slot;
         if (!t){
            /* Insert this one into the hash table.  */
            t = self->int_cst_node;
            *slot = t;
            /* Make a new node for next time round.  */
            self->int_cst_node = make_int_cst (1, 1);
         }
      }
   }else{
      /* The value either hashes properly or we drop it on the floor
      for the gc to take care of.  There will not be enough of them
      to worry about.  */

      tree nt = build_new_int_cst(self,type, cst);
      tree *slot = self->int_cst_hash_table->find_slot (nt, INSERT);
      t = *slot;
      if (!t){
         /* Insert this one into the hash table.  */
         t = nt;
         *slot = t;
      }else
         ggc_free (nt);
   }

   return t;
}

/* Create a constant tree that contains CST sign-extended to TYPE.  */
//原型 build_int_cst tree.h tree.cc
tree mtcs_tree_build_int_cst (MtcsTree *self,tree type, poly_int64 cst)
{
  MtcsTree *mtcsTree=self;
  /* Support legacy code.  */
  if (!type)
    type = mtcs_integer_type_node;

  return mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(self,type, wi::shwi (cst, TYPE_PRECISION (type)));
}

/* Create a new vector type node holding NUNITS units of type INNERTYPE,
   and mapped to the machine mode MODE.  Initialize its fields and build
   the information necessary for debugging output.  */
//原型 make_vector_type tree.cc
static tree make_vector_type (MtcsTree *self,tree innertype, poly_int64 nunits, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAttribs *mtcsAttribs =mtcs_target_get_attribs(mtcsTarget);
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   tree t;
   tree mv_innertype = TYPE_MAIN_VARIANT (innertype);

   t = make_node (VECTOR_TYPE);
   TREE_TYPE (t) = mv_innertype;
   SET_TYPE_VECTOR_SUBPARTS (t, nunits);
   SET_TYPE_MODE (t, mode);

   if (TYPE_STRUCTURAL_EQUALITY_P (mv_innertype) || mtcsOptionsItem->x_in_lto_p)
      SET_TYPE_STRUCTURAL_EQUALITY (t);
   else if ((TYPE_CANONICAL (mv_innertype) != innertype || mode != VOIDmode)
         && !VECTOR_BOOLEAN_TYPE_P (t))
      TYPE_CANONICAL (t) = make_vector_type (self,TYPE_CANONICAL (mv_innertype), nunits, VOIDmode);

   mtcs_stor_layout_layout_type/*!layout_type*/(mtcsStorLayout,t);

   hashval_t hash = mtcs_tree_type_hash_canon_hash/*!type_hash_canon_hash*/(self,t);
   t = mtcs_tree_type_hash_canon/*!type_hash_canon*/(self,hash, t);

   /* We have built a main variant, based on the main variant of the
   inner type. Use it to build the variant we return.  */
   if ((TYPE_ATTRIBUTES (innertype) || TYPE_QUALS (innertype))  && TREE_TYPE (t) != innertype)
      return mtcs_attribs_build_type_attribute_qual_variant/*!build_type_attribute_qual_variant*/(mtcsAttribs,
            t, TYPE_ATTRIBUTES (innertype),TYPE_QUALS (innertype));

   return t;
}

/* Returns a vector tree node given a mode (integer, vector, or BLKmode) and
   the inner type.  */
//原型 build_vector_type_for_mode tree.h tree.cc
tree mtcs_tree_build_vector_type_for_mode (MtcsTree *self,tree innertype, machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsLang   *mtcsLang=mtcs_target_get_lang(mtcsTarget);

   poly_int64 nunits;
   unsigned int bitsize;

   switch (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode)){
      case MODE_VECTOR_BOOL:
      case MODE_VECTOR_INT:
      case MODE_VECTOR_FLOAT:
      case MODE_VECTOR_FRACT:
      case MODE_VECTOR_UFRACT:
      case MODE_VECTOR_ACCUM:
      case MODE_VECTOR_UACCUM:
         nunits = mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode);
         break;

      case MODE_INT:
         /* Check that there are no leftover bits.  */
         bitsize = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mtcs_mode_as_a <scalar_int_mode> (mtcsMode,mode));
         gcc_assert (bitsize % TREE_INT_CST_LOW (TYPE_SIZE (innertype)) == 0);
         nunits = bitsize / TREE_INT_CST_LOW (TYPE_SIZE (innertype));
         break;

      default:
         gcc_unreachable ();
   }

   return make_vector_type(self,innertype, nunits, mode);
}

/* Similarly, but takes the inner type and number of units, which must be
   a power of two.  */
//原型 build_vector_type tree.h tree.cc
tree mtcs_tree_build_vector_type (MtcsTree *self,tree innertype, poly_int64 nunits)
{
  return make_vector_type(self,innertype, nunits, VOIDmode);
}

/* Create a complex type whose components are COMPONENT_TYPE.

   If NAMED is true, the type is given a TYPE_NAME.  We do not always
   do so because this creates a DECL node and thus make the DECL_UIDs
   dependent on the type canonicalization hashtable, which is GC-ed,
   so the DECL_UIDs would not be stable wrt garbage collection.  */
//原型 build_complex_type tree.h tree.cc
tree mtcs_tree_build_complex_type (MtcsTree *self,tree component_type, bool named)
{
   MtcsTree *mtcsTree=self;

   gcc_assert (INTEGRAL_TYPE_P (component_type) || SCALAR_FLOAT_TYPE_P (component_type) || FIXED_POINT_TYPE_P (component_type));

   /* Make a node of the sort we want.  */
   tree probe = make_node (COMPLEX_TYPE);

   TREE_TYPE (probe) = TYPE_MAIN_VARIANT (component_type);

   /* If we already have such a type, use the old one.  */
   hashval_t hash = mtcs_tree_type_hash_canon_hash/*!type_hash_canon_hash*/(self,probe);
   tree t =mtcs_tree_type_hash_canon/*!type_hash_canon*/(self,hash, probe);

   if (t == probe){
      /* We created a new type.  The hash insertion will have laid
      out the type.  We need to check the canonicalization and
      maybe set the name.  */
      gcc_checking_assert (COMPLETE_TYPE_P (t)
      && !TYPE_NAME (t)
      && TYPE_CANONICAL (t) == t);

      if (TYPE_STRUCTURAL_EQUALITY_P (TREE_TYPE (t)))
         SET_TYPE_STRUCTURAL_EQUALITY (t);
      else if (TYPE_CANONICAL (TREE_TYPE (t)) != TREE_TYPE (t))
         TYPE_CANONICAL (t) = mtcs_tree_build_complex_type/*!build_complex_type*/(self,
               TYPE_CANONICAL (TREE_TYPE (t)), named);

      /* We need to create a name, since complex is a fundamental type.  */
      if (named){
         const char *name = NULL;

         if (TREE_TYPE (t) == mtcs_char_type_node)
            name = "complex char";
         else if (TREE_TYPE (t) == mtcs_signed_char_type_node)
            name = "complex signed char";
         else if (TREE_TYPE (t) == mtcs_unsigned_char_type_node)
            name = "complex unsigned char";
         else if (TREE_TYPE (t) == mtcs_short_integer_type_node)
            name = "complex short int";
         else if (TREE_TYPE (t) == mtcs_short_unsigned_type_node)
            name = "complex short unsigned int";
         else if (TREE_TYPE (t) == mtcs_integer_type_node)
            name = "complex int";
         else if (TREE_TYPE (t) == mtcs_unsigned_type_node)
            name = "complex unsigned int";
         else if (TREE_TYPE (t) == mtcs_long_integer_type_node)
            name = "complex long int";
         else if (TREE_TYPE (t) == mtcs_long_unsigned_type_node)
            name = "complex long unsigned int";
         else if (TREE_TYPE (t) == mtcs_long_long_integer_type_node)
            name = "complex long long int";
         else if (TREE_TYPE (t) == mtcs_long_long_unsigned_type_node)
            name = "complex long long unsigned int";

         if (name != NULL)
            TYPE_NAME (t) =mtcs_tree_build_decl/*!build_decl*/(self,UNKNOWN_LOCATION, TYPE_DECL,
                  get_identifier (name), t);
      }
   }

   return mtcs_tree_build_qualified_type/*!build_qualified_type*/(self,t, TYPE_QUALS (component_type));
}

//原型 MAX_BOOL_CACHED_PREC tree.cc
#define MAX_BOOL_CACHED_PREC \
  (HOST_BITS_PER_WIDE_INT > 64 ? HOST_BITS_PER_WIDE_INT : 64)

#define MAX_INT_CACHED_PREC \
  (HOST_BITS_PER_WIDE_INT > 64 ? HOST_BITS_PER_WIDE_INT : 64)
/* Builds a boolean type of precision PRECISION.
   Used for boolean vectors to choose proper vector element size.  */
//原型 build_nonstandard_boolean_type tree.h tree.cc
tree mtcs_tree_build_nonstandard_boolean_type (MtcsTree *self,unsigned HOST_WIDE_INT precision)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);

   tree type;

   if (precision <= MAX_BOOL_CACHED_PREC){
      type = self->nonstandard_boolean_type_cache[precision];
      if (type)
         return type;
   }


   type = make_node (BOOLEAN_TYPE);
   TYPE_PRECISION (type) = precision;
   mtcs_stor_layout_fixup_signed_type/*!fixup_signed_type*/(mtcsStorLayout,type);

   if (precision <= MAX_INT_CACHED_PREC)
      self->nonstandard_boolean_type_cache[precision] = type;

   return type;
}

/* Build a function type.  The RETURN_TYPE is the type returned by the
   function.  If VAARGS is set, no void_type_node is appended to the
   list.  ARGP must be always be terminated be a NULL_TREE.  */
//原型 build_function_type_list_1 tree.cc
static tree build_function_type_list_1 (MtcsTree *self,bool vaargs, tree return_type, va_list argp)
{
   MtcsTree *mtcsTree=self;
   tree t, args, last;

   t = va_arg (argp, tree);
   for (args = NULL_TREE; t != NULL_TREE; t = va_arg (argp, tree))
      args = tree_cons (NULL_TREE, t, args);

   if (vaargs) {
      last = args;
      if (args != NULL_TREE)
         args = nreverse (args);
      gcc_assert (last != mtcs_void_list_node);
   }else if (args == NULL_TREE)
      args = mtcs_void_list_node;
   else{
      last = args;
      args = nreverse (args);
      TREE_CHAIN (last) = mtcs_void_list_node;
   }
   args = mtcs_tree_build_function_type/*!build_function_type*/(self,return_type, args, vaargs && args == NULL_TREE);

   return args;
}

/* Build a function type.  The RETURN_TYPE is the type returned by the
   function.  If additional arguments are provided, they are
   additional argument types.  The list of argument types must always
   be terminated by NULL_TREE.  */
//原型 build_function_type_list tree.h tree.cc
tree mtcs_tree_build_function_type_list (MtcsTree *self,tree return_type, ...)
{
   tree args;
   va_list p;
   va_start (p, return_type);
   args = build_function_type_list_1(self,false, return_type, p);
   va_end (p);
   return args;
}

/* Create a constant tree with value VALUE in type TYPE.  */
//原型 wide_int_to_tree tree.h tree.cc
tree mtcs_tree_wide_int_to_tree (MtcsTree *self,tree type, const poly_wide_int_ref &value)
{
  if (value.is_constant ())
    return wide_int_to_tree_1(self,type, value.coeffs[0]);
  return mtcs_tree_build_poly_int_cst/*!build_poly_int_cst*/(self,type, value);
}

/* Return tree node kind based on tree CODE.  */

static tree_node_kind get_stats_node_kind (enum tree_code code)
{
  enum tree_code_class type = TREE_CODE_CLASS (code);

  switch (type){
    case tcc_declaration:  /* A decl node */
      return d_kind;
    case tcc_type:  /* a type node */
      return t_kind;
    case tcc_statement:  /* an expression with side effects */
      return s_kind;
    case tcc_reference:  /* a reference */
      return r_kind;
    case tcc_expression:  /* an expression */
    case tcc_comparison:  /* a comparison expression */
    case tcc_unary:  /* a unary arithmetic expression */
    case tcc_binary:  /* a binary arithmetic expression */
      return e_kind;
    case tcc_constant:  /* a constant */
      return c_kind;
    case tcc_exceptional:  /* something random, like an identifier.  */
      switch (code)
   {
   case IDENTIFIER_NODE:
     return id_kind;
   case TREE_VEC:
     return vec_kind;
   case TREE_BINFO:
     return binfo_kind;
   case SSA_NAME:
     return ssa_name_kind;
   case BLOCK:
     return b_kind;
   case CONSTRUCTOR:
     return constr_kind;
   case OMP_CLAUSE:
     return omp_clause_kind;
   default:
     return x_kind;
   }
      break;
    case tcc_vl_exp:
      return e_kind;
    default:
      gcc_unreachable ();
    }
}


/* Record interesting allocation statistics for a tree node with CODE
   and LENGTH.  */
//原型 record_node_allocation_statistics tree.cc

static void record_node_allocation_statistics (MtcsTree *self,enum tree_code code, size_t length)
{
  if (!GATHER_STATISTICS)
    return;

  tree_node_kind kind = get_stats_node_kind (code);
  self->tree_code_counts[(int) code]++;
  self->tree_node_counts[(int) kind]++;
  self->tree_node_sizes[(int) kind] += length;
}

/* Return a new POLY_INT_CST with coefficients COEFFS and type TYPE.  */
//原型 build_new_poly_int_cst tree.cc
static tree build_new_poly_int_cst (MtcsTree *self,tree type, tree (&coeffs)[NUM_POLY_INT_COEFFS]
         CXX_MEM_STAT_INFO)
{
  size_t length = sizeof (struct tree_poly_int_cst);
  record_node_allocation_statistics(self,POLY_INT_CST, length);

  tree t = ggc_alloc_cleared_tree_node_stat (length PASS_MEM_STAT);

  TREE_SET_CODE (t, POLY_INT_CST);
  TREE_CONSTANT (t) = 1;
  TREE_TYPE (t) = type;
  for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
    POLY_INT_CST_COEFF (t, i) = coeffs[i];
  return t;
}

/* Build a POLY_INT_CST node with type TYPE and with the elements in VALUES.
   The elements must also have type TYPE.  */
//原型 build_poly_int_cst tree.h tree.cc
tree mtcs_tree_build_poly_int_cst (MtcsTree *self,tree type, const poly_wide_int_ref &values)
{
   unsigned int prec = TYPE_PRECISION (type);
   gcc_assert (prec <= values.coeffs[0].get_precision ());
   poly_wide_int c = poly_wide_int::from (values, prec, SIGNED);

   inchash::hash h;
   h.add_int (TYPE_UID (type));
   for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
      h.add_wide_int (c.coeffs[i]);
   mtcs_poly_int_cst_hasher::compare_type comp (type, &c);
   tree *slot = self->poly_int_cst_hash_table->find_slot_with_hash (comp, h.end (),INSERT);
   if (*slot == NULL_TREE){
      tree coeffs[NUM_POLY_INT_COEFFS];
      for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i)
         coeffs[i] = wide_int_to_tree_1(self,type, c.coeffs[i]);
      *slot = build_new_poly_int_cst(self,type, coeffs);
   }
   return *slot;
}


/* Create a sizetype INT_CST node with NUMBER sign extended.  KIND
   indicates which particular sizetype to create.  */
//原型 size_int_kind fold-const.h fold-const.hh
tree mtcs_tree_size_int_kind(MtcsTree *self,poly_int64 number, enum size_type_kind kind)
{
  return mtcs_tree_build_int_cst/*!build_int_cst*/(self,self->sizetype_tab[(int) kind], number);
}

/* True if T is an erroneous expression.  */
//原型 error_operand_p tree.h
bool mtcs_tree_error_operand_p (MtcsTree *self,const_tree t)
{
  MtcsTree *mtcsTree=self;
  return (t == error_mark_node  || (t && TREE_TYPE (t) == error_mark_node));
}


/****------------------------------------以下替换主机的global_trees等--------------*/
static bool replace_targetm_floatn_builtin_p(int func)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   return mtcsTarget->floatn_builtin_p(mtcsTarget,func);
}

static bool  replace_targetm_libc_has_function(enum function_class fn_class, tree type)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   return mtcsTarget->libc_has_function(mtcsTarget,fn_class,type);
}

static scalar_int_mode replace_targetm_unwind_word_mode ()
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   return mtcsTarget->unwind_word_mode(mtcsTarget);
}

//为了在mtcs调用tree.cc中的build1 需要把recompute_tree_invariant_for_addr_expr中调用的方法
//node = lang_hooks.expr_to_decl (node, &tc, &se); 指向mtcs中，所以在mtcstree.c的backup restore中加入对lang_hooks的操用
static tree replace_lang_hooks_expr_to_decl (tree expr, bool *tc ATTRIBUTE_UNUSED, bool *se)
{
   return expr;
}

//原型 lto_type_for_mode lto-lang.cc #define LANG_HOOKS_TYPE_FOR_MODE lto_type_for_mode
static tree replace_lang_hooks_types_type_for_mode(machine_mode mode, int unsigned_p)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   return typeForMode(mtcsTree,mode,unsigned_p);
}

/* Return the tree node for an explicit standard builtin function or NULL.  */
//原型 builtin_decl_explicit tree.h
tree mtcs_tree_builtin_decl_explicit (MtcsTree *self,enum built_in_function fncode)
{
  gcc_checking_assert (BUILTIN_VALID_P (fncode));
  return self->builtin_info[(size_t)fncode].decl;
}

/* Build a simple MEM_REF tree with the sematics of a plain INDIRECT_REF
   on the pointer PTR.  */
//原型 build_simple_mem_ref_loc fold-const.h tree.cc
tree mtcs_tree_build_simple_mem_ref_loc (MtcsTree *self,location_t loc, tree ptr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfa   *mtcsDfa=mtcs_target_get_dfa(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsGimpleExpr *mtcsGimpleExpr=mtcs_target_get_gimple_expr(mtcsTarget);

   poly_int64 offset = 0;
   tree ptype = TREE_TYPE (ptr);
   tree tem;
   /* For convenience allow addresses that collapse to a simple base
   and offset.  */
   if (TREE_CODE (ptr) == ADDR_EXPR
   && (handled_component_p (TREE_OPERAND (ptr, 0))
   || TREE_CODE (TREE_OPERAND (ptr, 0)) == MEM_REF)){
      ptr = mtcs_dfa_get_addr_base_and_unit_offset/*!get_addr_base_and_unit_offset*/(mtcsDfa,TREE_OPERAND (ptr, 0), &offset);
      gcc_assert (ptr);
      if (TREE_CODE (ptr) == MEM_REF){
         offset += mem_ref_offset (ptr).force_shwi ();
         ptr = TREE_OPERAND (ptr, 0);
      }else
         ptr = mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,ptr);
      gcc_assert (mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,ptr) || is_gimple_min_invariant (ptr));
   }
   tem = build2 (MEM_REF, TREE_TYPE (ptype),ptr, mtcs_tree_build_int_cst/*!build_int_cst*/(self,ptype, offset));
   SET_EXPR_LOCATION (tem, loc);
   return tem;
}

//原型 build_simple_mem_ref fold-const.h
tree mtcs_tree_build_simple_mem_ref(MtcsTree *self,tree ptr)
{
   return mtcs_tree_build_simple_mem_ref_loc(self,UNKNOWN_LOCATION,ptr);
}
/* Return a tree representing the offset, in bytes, of the field referenced
   by EXP.  This does not include any offset in DECL_FIELD_BIT_OFFSET.  */
//原型 component_ref_field_offset tree.h tree.cc
tree mtcs_tree_component_ref_field_offset (MtcsTree *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree aligned_offset = TREE_OPERAND (exp, 2);
   tree field = TREE_OPERAND (exp, 1);
   location_t loc = EXPR_LOCATION (exp);

   /* If an offset was specified in the COMPONENT_REF, it's the offset measured
   in units of DECL_OFFSET_ALIGN / BITS_PER_UNIT.  So multiply by that
   value.  */
   if (aligned_offset){
      fprintf(stderr,"mtcstree.c mtcs_tree_component_ref_field_offset 00\n");
      /* ??? tree_ssa_useless_type_conversion will eliminate casts to
      sizetype from another type of the same width and signedness.  */
      if (TREE_TYPE (aligned_offset) != sizetype){
         fprintf(stderr,"mtcstree.c mtcs_tree_component_ref_field_offset 11\n");

         aligned_offset = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype, aligned_offset);
         aet_print_tree(aligned_offset);
      }
      return mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,
            loc, MULT_EXPR, aligned_offset,size_int (DECL_OFFSET_ALIGN (field) / BITS_PER_UNIT));
   }
   /* Otherwise, take the offset from that of the field.  Substitute
   any PLACEHOLDER_EXPR that we have.  */
   else{
      tree offset= DECL_FIELD_OFFSET (field);
      if(offset==NULL_TREE)
         n_debug("mtcstree.c mtcs_tree_component_ref_field_offset 22 是空的\n");
      if(TREE_CONSTANT (offset))
         n_debug("mtcstree.c mtcs_tree_component_ref_field_offset 33 是常数\n");
      aet_print_tree(offset);
      aet_print_tree(field);

      return SUBSTITUTE_PLACEHOLDER_IN_EXPR (DECL_FIELD_OFFSET (field), exp);
   }
}

/* We force the wide_int CST to the range of the type TYPE by sign or
   zero extending it.  OVERFLOWABLE indicates if we are interested in
   overflow of the value, when >0 we are only interested in signed
   overflow, for <0 we are interested in any overflow.  OVERFLOWED
   indicates whether overflow has already occurred.  CONST_OVERFLOWED
   indicates whether constant overflow has already occurred.  We force
   T's value to be within range of T's type (by setting to 0 or 1 all
   the bits outside the type's range).  We set TREE_OVERFLOWED if,
        OVERFLOWED is nonzero,
        or OVERFLOWABLE is >0 and signed overflow occurs
        or OVERFLOWABLE is <0 and any overflow occurs
   We return a new tree node for the extended wide_int.  The node
   is shared if no overflow flags are set.  */
//原型 force_fit_type tree.h tree.cc
tree mtcs_tree_force_fit_type (MtcsTree *self,tree type, const poly_wide_int_ref &cst,
      int overflowable, bool overflowed)
{
   signop sign = TYPE_SIGN (type);
   /* If we need to set overflow flags, return a new unshared node.  */
   if (overflowed || !wi::fits_to_tree_p (cst, type)){
      if (overflowed || overflowable < 0  || (overflowable > 0 && sign == SIGNED)){
         poly_wide_int tmp = poly_wide_int::from (cst, TYPE_PRECISION (type),sign);
         tree t;
         if (tmp.is_constant ())
            t = build_new_int_cst(self,type, tmp.coeffs[0]);
         else{
            tree coeffs[NUM_POLY_INT_COEFFS];
            for (unsigned int i = 0; i < NUM_POLY_INT_COEFFS; ++i){
               coeffs[i] = build_new_int_cst(self,type, tmp.coeffs[i]);
               TREE_OVERFLOW (coeffs[i]) = 1;
            }
            t = build_new_poly_int_cst(self,type, coeffs);
         }
         TREE_OVERFLOW (t) = 1;
         return t;
      }
   }
   /* Else build a shared node.  */
   return mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(self,type, cst);
}

/* Return a newly constructed COMPLEX_CST node whose value is
   specified by the real and imaginary parts REAL and IMAG.
   Both REAL and IMAG should be constant nodes.  TYPE, if specified,
   will be the type of the COMPLEX_CST; otherwise a new type will be made.  */
//原型 build_complex tree.h tree.cc
tree mtcs_tree_build_complex (MtcsTree *self,tree type, tree real, tree imag)
{
  gcc_assert (CONSTANT_CLASS_P (real));
  gcc_assert (CONSTANT_CLASS_P (imag));

  tree t = make_node (COMPLEX_CST);

  TREE_REALPART (t) = real;
  TREE_IMAGPART (t) = imag;
  TREE_TYPE (t) = type ? type : mtcs_tree_build_complex_type/*!build_complex_type*/(self,TREE_TYPE (real));
  TREE_OVERFLOW (t) = TREE_OVERFLOW (real) | TREE_OVERFLOW (imag);
  return t;
}

/* Constructs tree in type TYPE from with value given by CST.  Signedness
   of CST is assumed to be the same as the signedness of TYPE.  */
//原型 double_int_to_tree tree.h tree.cc
tree mtcs_tree_double_int_to_tree (MtcsTree *self,tree type, double_int cst)
{
  return mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(self,type, widest_int::from (cst, TYPE_SIGN (type)));
}

/* Return a new REAL_CST node whose type is TYPE and value is D.  */
//原型 build_real tree.h tree.cc
tree mtcs_tree_build_real (MtcsTree *self,tree type, REAL_VALUE_TYPE d)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   tree v;
   int overflow = 0;

   /* dconst{0,1,2,m1,half} are used in various places in
   the middle-end and optimizers, allow them here
   even for decimal floating point types as an exception
   by converting them to decimal.  */
   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,TYPE_MODE (type))
   && (d.cl == rvc_normal || d.cl == rvc_zero)  && !d.decimal){
      if (memcmp (&d, &dconst1, sizeof (d)) == 0)
         decimal_real_from_string (&d, "1");
      else if (memcmp (&d, &dconst2, sizeof (d)) == 0)
         decimal_real_from_string (&d, "2");
      else if (memcmp (&d, &dconstm1, sizeof (d)) == 0)
         decimal_real_from_string (&d, "-1");
      else if (memcmp (&d, &dconsthalf, sizeof (d)) == 0)
         decimal_real_from_string (&d, "0.5");
      else if (memcmp (&d, &dconst0, sizeof (d)) == 0){
         /* Make sure to give zero the minimum quantum exponent for
         the type (which corresponds to all bits zero).  */
         const struct real_format *fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,TYPE_MODE (type));
         char buf[16];
         sprintf (buf, "0e%d", fmt->emin - fmt->p);
         decimal_real_from_string (&d, buf);
      }else
         gcc_unreachable ();
   }

   /* ??? Used to check for overflow here via CHECK_FLOAT_TYPE.
   Consider doing it via real_convert now.  */

   v = make_node (REAL_CST);
   TREE_TYPE (v) = type;
   memcpy (TREE_REAL_CST_PTR (v), &d, sizeof (REAL_VALUE_TYPE));
   TREE_OVERFLOW (v) = overflow;
   return v;
}

/* Given a tree representing an integer constant I, return a tree
   representing the same value as a floating-point constant of type TYPE.  */
//原型 build_real_from_int_cst tree.h tree.cc
tree mtcs_tree_build_real_from_int_cst (MtcsTree *self,tree type, const_tree i)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReal  *mtcsReal=mtcs_target_get_real(mtcsTarget);

   tree v;
   int overflow = TREE_OVERFLOW (i);
   v = mtcs_tree_build_real/*!build_real*/(self,type,
         mtcs_real_real_value_from_int_cst/*!real_value_from_int_cst*/(mtcsReal,type, i));

   TREE_OVERFLOW (v) |= overflow;
   return v;
}

/* Return a tree representing the lower bound of the array mentioned in
   EXP, an ARRAY_REF or an ARRAY_RANGE_REF.  */
//原型 array_ref_low_bound tree.h tree.cc
tree mtcs_tree_array_ref_low_bound (MtcsTree *self,tree exp)
{
   tree domain_type = TYPE_DOMAIN (TREE_TYPE (TREE_OPERAND (exp, 0)));

   /* If a lower bound is specified in EXP, use it.  */
   if (TREE_OPERAND (exp, 2))
      return TREE_OPERAND (exp, 2);

   /* Otherwise, if there is a domain type and it has a lower bound, use it,
   substituting for a PLACEHOLDER_EXPR as needed.  */
   if (domain_type && TYPE_MIN_VALUE (domain_type))
      return SUBSTITUTE_PLACEHOLDER_IN_EXPR (TYPE_MIN_VALUE (domain_type), exp);

   /* Otherwise, return a zero of the appropriate type.  */
   tree idxtype = TREE_TYPE (TREE_OPERAND (exp, 1));
   return (idxtype == error_mark_node? integer_zero_node :
         mtcs_tree_build_int_cst/*!build_int_cst*/(self,idxtype, 0));
}

/* Return a tree of sizetype representing the size, in bytes, of the element
   of EXP, an ARRAY_REF or an ARRAY_RANGE_REF.  */
//原型 array_ref_element_size tree.h tree.cc
tree mtcs_tree_array_ref_element_size (MtcsTree *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree = self;

   tree aligned_size = TREE_OPERAND (exp, 3);
   tree elmt_type = TREE_TYPE (TREE_TYPE (TREE_OPERAND (exp, 0)));
   location_t loc = EXPR_LOCATION (exp);
   n_debug("mtcstree.c mtcs_tree_array_ref_element_size 00 expr:%p mode:%d\n",exp,TYPE_MODE(elmt_type));

   /* If a size was specified in the ARRAY_REF, it's the size measured
   in alignment units of the element type.  So multiply by that value.  */
   if (aligned_size){
      n_debug("mtcstree.c mtcs_tree_array_ref_element_size 11 expr:%p\n",exp);

      /* ??? tree_ssa_useless_type_conversion will eliminate casts to
      sizetype from another type of the same width and signedness.  */
      if (TREE_TYPE (aligned_size) != mtcs_sizetype/*!sizetype*/){
         n_debug("mtcstree.c mtcs_tree_array_ref_element_size 22 expr:%p\n",exp);

         aligned_size = mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, mtcs_sizetype/*!sizetype*/, aligned_size);
      }
      tree ret = mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,loc, MULT_EXPR,
            aligned_size,mtcs_tree_get_size_int/*!size_int*/(self,TYPE_ALIGN_UNIT (elmt_type)));
      n_debug("mtcstree.c mtcs_tree_array_ref_element_size 33 expr:%p %d\n",exp,TYPE_MODE(TREE_TYPE(ret)));

      return ret;
   }
   /* Otherwise, take the size from that of the element type.  Substitute
   any PLACEHOLDER_EXPR that we have.  */
   else{
      return SUBSTITUTE_PLACEHOLDER_IN_EXPR (TYPE_SIZE_UNIT (elmt_type), exp);
   }
}

/**
 * 假设主机和设备的TI_MAX相同
 * builtins.def中用到targetm
 * targetm.floatn_builtin_p ((int) ENUM)
 * targetm.libc_has_function
 * targetm.have_tls
 * targetm.emutls.get_address
 * targetm.emutls.register_common
 * builtin-types.def中用到
 * targetm.unwind_word_mode
 * lang_hooks.types.type_for_mode
 */
static   void backup_cb(MtcsBackupRestore *iface)
{
   MtcsTree *self=(MtcsTree *)iface->impl;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   MtcsTreeBackup *backup=(MtcsTreeBackup *)self->backup;

   int i;
   for(i=0;i<TI_MAX;i++){
      backup->back_global_trees[i]=global_trees[i];
      global_trees[i]=self->global_trees[i];
   }

   for(i=0;i<itk_none;i++){
         backup->back_integer_types[i]=integer_types[i];
         integer_types[i]=self->integer_types[i];
   }

   for(i=0;i<(int) stk_type_kind_last;i++){
         backup->back_sizetype_tab[i]=sizetype_tab[i];
       //  fprintf(stderr,"mtcstree.c backup sizetype aa i:%d %p\n",i,sizetype_tab[i]);
         sizetype_tab[i]=self->sizetype_tab[i];
       //  fprintf(stderr,"mtcstree.c backup sizetype bb i:%d %p %p\n",i,sizetype_tab[i],bitsizetype);

   }

   backup->back_targetm_floatn_builtin_p=targetm.floatn_builtin_p;
   targetm.floatn_builtin_p=replace_targetm_floatn_builtin_p;

   backup->back_targetm_libc_has_function=targetm.libc_has_function;
   targetm.libc_has_function=replace_targetm_libc_has_function;

   backup->back_targetm_have_tls =targetm.have_tls;
   targetm.have_tls=mtcsTarget->have_tls;

   backup->back_targetm_emutls_get_address=targetm.emutls.get_address;
   targetm.emutls.get_address = mtcsMachine->emutls->get_address;

   backup->back_targetm_emutls_register_common=targetm.emutls.register_common;
   targetm.emutls.register_common = mtcsMachine->emutls->register_common;

   backup->back_targetm_unwind_word_mode = targetm.unwind_word_mode;
   targetm.unwind_word_mode =replace_targetm_unwind_word_mode;

   //原型 (*lang_hooks.types.type_for_mode)
   backup->back_lang_hooks_types_type_for_mode=lang_hooks.types.type_for_mode;
   lang_hooks.types.type_for_mode= replace_lang_hooks_types_type_for_mode;

   //为了在mtcs调用tree.cc中的build1 需要把recompute_tree_invariant_for_addr_expr中调用的方法
   //node = lang_hooks.expr_to_decl (node, &tc, &se); 指向mtcs中，所以在mtcstree.c的backup restore中加入对lang_hooks的操作
   backup->back_lang_hooks_expr_to_decl = lang_hooks.expr_to_decl;
   lang_hooks.expr_to_decl= replace_lang_hooks_expr_to_decl;

}

static   void restore_cb(MtcsBackupRestore *iface)
{
   MtcsTree *self=(MtcsTree *)iface->impl;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTreeBackup *backup=(MtcsTreeBackup *)self->backup;

   int i;
   for(i=0;i<TI_MAX;i++){
      global_trees[i]=backup->back_global_trees[i];
   }
   for(i=0;i<itk_none;i++){
         integer_types[i]=backup->back_integer_types[i];
   }
   for(i=0;i<(int) stk_type_kind_last;i++){
      sizetype_tab[i]=backup->back_sizetype_tab[i];
   }
   targetm.floatn_builtin_p=backup->back_targetm_floatn_builtin_p;
   targetm.libc_has_function=backup->back_targetm_libc_has_function;
   targetm.have_tls=backup->back_targetm_have_tls;
   targetm.emutls.get_address = backup->back_targetm_emutls_get_address;
   targetm.emutls.register_common = backup->back_targetm_emutls_register_common;
   targetm.unwind_word_mode =  backup->back_targetm_unwind_word_mode;
   lang_hooks.types.type_for_mode= backup->back_lang_hooks_types_type_for_mode;
   lang_hooks.expr_to_decl=backup->back_lang_hooks_expr_to_decl;
}

/* Builds a signed or unsigned integer type of precision PRECISION.
   Used for C bitfields whose precision does not match that of
   built-in target types.  */
//原型 build_nonstandard_integer_type tree.h tree.cc
tree mtcs_tree_build_nonstandard_integer_type (MtcsTree *self,unsigned HOST_WIDE_INT precision,
            int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsTree *mtcsTree=self;

   tree itype, ret;

   if (unsignedp)
      unsignedp = MAX_INT_CACHED_PREC + 1;

   if (precision <= MAX_INT_CACHED_PREC){
      itype = self->nonstandard_integer_type_cache[precision + unsignedp];
      if (itype)
         return itype;
   }

   itype = make_node (INTEGER_TYPE);
   TYPE_PRECISION (itype) = precision;

   if (unsignedp)
      mtcs_stor_layout_fixup_unsigned_type/*!fixup_unsigned_type*/(mtcsStorLayout,itype);
   else
      mtcs_stor_layout_fixup_signed_type/*!fixup_signed_type*/(mtcsStorLayout,itype);


   inchash::hash hstate;
   inchash::add_expr (TYPE_MAX_VALUE (itype), hstate);
   ret = mtcs_tree_type_hash_canon/*!type_hash_canon*/(self,hstate.end (), itype);
   if (precision <= MAX_INT_CACHED_PREC)
      self->nonstandard_integer_type_cache[precision + unsignedp] = ret;

   return ret;
}

/* Return true if type T has the same precision as its underlying mode.  */
//原型 type_has_mode_precision_p  tree.h
 bool mtcs_tree_type_has_mode_precision_p (MtcsTree *self,const_tree t)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   return known_eq (TYPE_PRECISION (t), mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,TYPE_MODE (t)));
}

 void mtcs_tree_debug_tree(MtcsTree *self,int passType,char *name)
 {
    tree t1=global_trees[TI_SIZE_ZERO];
    tree t2=self->global_trees[TI_SIZE_ZERO];
    MtcsTreeBackup *backup=(MtcsTreeBackup *)self->backup;
    tree t3=backup->back_global_trees[TI_SIZE_ZERO];

    fprintf(stderr,"在 pass %d %s 中打印 tree ---global_trees[TI_SIZE_ZERO] global:%p %p mtcs:%p %p back:%p %p\n",
         passType,name, t1,TREE_TYPE(t1), t2,TREE_TYPE(t2), t3,TREE_TYPE(t3));
}


 /* Conveniently construct a function call expression.  FNDECL names the
    function to be called and N arguments are passed in the array
    ARGARRAY.  */
 //原型 build_call_expr_loc_array tree.h tree.cc
tree mtcs_tree_build_call_expr_loc_array (MtcsTree *self,location_t loc, tree fndecl, int n, tree *argarray)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree fntype = TREE_TYPE (fndecl);
   tree fn = build1 (ADDR_EXPR, mtcs_tree_build_pointer_type/*!build_pointer_type*/(self,fntype), fndecl);
   return mtcs_const_fold_build_call_array_loc/*!fold_build_call_array_loc*/(mtcsConst,loc, TREE_TYPE (fntype), fn, n, argarray);
}

 /* Like build_call_expr_loc (UNKNOWN_LOCATION, ...).  Duplicated because
    varargs macros aren't supported by all bootstrap compilers.  */
//原型 build_call_expr tree.h tree.cc
tree mtcs_tree_build_call_expr (MtcsTree *self,tree fndecl, int n, ...)
{
   va_list ap;
   tree *argarray = XALLOCAVEC (tree, n);
   int i;

   va_start (ap, n);
   for (i = 0; i < n; i++)
      argarray[i] = va_arg (ap, tree);
   va_end (ap);
   return mtcs_tree_build_call_expr_loc_array/*!build_call_expr_loc_array*/(self,UNKNOWN_LOCATION, fndecl, n, argarray);
}


 /* The built-in decl to use to mark code points believed to be unreachable.
    Typically __builtin_unreachable, but __builtin_trap if
    -fsanitize=unreachable -fsanitize-trap=unreachable.  If only
    -fsanitize=unreachable, we rely on sanopt to replace calls with the
    appropriate ubsan function.  When building a call directly, use
    {gimple_},build_builtin_unreachable instead.  */
 //原型 builtin_decl_unreachable tree.h tree.cc
tree mtcs_tree_builtin_decl_unreachable(MtcsTree *self)
{
   enum built_in_function fncode = BUILT_IN_UNREACHABLE;

   if (sanitize_flags_p (SANITIZE_UNREACHABLE)
   ? (flag_sanitize_trap & SANITIZE_UNREACHABLE)
   : flag_unreachable_traps)
      fncode = BUILT_IN_UNREACHABLE_TRAP;
   /* For non-trapping sanitize, we will rewrite __builtin_unreachable () later,
   in the sanopt pass.  */

   return mtcs_tree_builtin_decl_explicit/*!builtin_decl_explicit*/(self,fncode);
}

/* Build a call to __builtin_unreachable, possibly rewritten by
   -fsanitize=unreachable.  Use this rather than the above when practical.  */
//原型 builtin_decl_unreachable tree.h tree.cc
//重载函数 builtin_decl_unreachable
tree mtcs_tree_builtin_decl_unreachable (MtcsTree *self,location_t loc)
{
   tree data = NULL_TREE;
   tree fn = sanitize_unreachable_fn (&data, loc);
   return mtcs_tree_build_call_expr/*!build_call_expr_loc*/(self,loc, fn, data != NULL_TREE, data);
}

/* Build 0 constant of type TYPE.  This is used by constructor folding
   and thus the constant should be represented in memory by
   zero(es).  */
//原型 build_zero_cst tree.h tree.cc
tree mtcs_tree_build_zero_cst (MtcsTree *self,tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   switch (TREE_CODE (type)){
      case INTEGER_TYPE: case ENUMERAL_TYPE: case BOOLEAN_TYPE:
      case POINTER_TYPE: case REFERENCE_TYPE:
      case OFFSET_TYPE: case NULLPTR_TYPE: case BITINT_TYPE:
         return mtcs_tree_build_int_cst/*!build_int_cst*/(self,type, 0);

      case REAL_TYPE:
         return mtcs_tree_build_real/*!build_real*/(self,type, dconst0);

      case FIXED_POINT_TYPE:
         return build_fixed (type, FCONST0 (TYPE_MODE (type)));

      case VECTOR_TYPE:
      {
         tree scalar = mtcs_tree_build_zero_cst/*!build_zero_cst*/(self,TREE_TYPE (type));
         return build_vector_from_val (type, scalar);
      }

      case COMPLEX_TYPE:
      {
         tree zero = mtcs_tree_build_zero_cst/*!build_zero_cst*/(self,TREE_TYPE (type));
         return build_complex (type, zero, zero);
      }

      default:
         if (!AGGREGATE_TYPE_P (type))
            return mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, integer_zero_node);
         return build_constructor (type, NULL);
   }
}

//备份主机的 builtin_info 只能在创建完mtcs的builtin后才能备份
//需求来自 gimple_call_builtin_p 最终会调用 builtin_decl_explicit 返回的是主机builtins,
//再调用 useless_type_conversion_p 永远返回false,所以 gimple_call_builtin_p 也不会返回 true.
//
void mtcs_tree_backup_builtin_info(MtcsTree *self)
{
   MtcsTreeBackup *backup=(MtcsTreeBackup *)self->backup;
   //备份主机内建函数
   int i;
   for(i=0;i<(int)END_BUILTINS;i++){
      backup->back_builtin_info[i].decl=builtin_info[i].decl;
      backup->back_builtin_info[i].implicit_p=builtin_info[i].implicit_p;
      backup->back_builtin_info[i].declared_p=builtin_info[i].declared_p;

      builtin_info[i].decl = self->builtin_info[i].decl;
      builtin_info[i].implicit_p = self->builtin_info[i].implicit_p;
      builtin_info[i].declared_p = self->builtin_info[i].declared_p;
   }
}

//恢复主机的 builtin_info
void mtcs_tree_restore_builtin_info(MtcsTree *self)
{
   MtcsTreeBackup *backup=(MtcsTreeBackup *)self->backup;
   int i;
   for(i=0;i<(int)END_BUILTINS;i++){
      builtin_info[i].decl = backup->back_builtin_info[i].decl;
      builtin_info[i].implicit_p = backup->back_builtin_info[i].implicit_p;
      builtin_info[i].declared_p = backup->back_builtin_info[i].declared_p;
   }
}
