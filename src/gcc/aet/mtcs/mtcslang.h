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


#ifndef __GCC_MTCS_LANG__
#define __GCC_MTCS_LANG__

#include "../nlib.h"
#include "mtcscomponent.h"

//原型 lang_hooks_for_types laghooks.h 上面的宏定义在langhooks-def.h中
//c语言的lang_hooks...宏在c/c-objc-common.h中定义
//nvptx在lto-lang.cc中定义

/*
//替换langhooks.h中的功能
#define LANG_HOOKS_FOR_TYPES_INITIALIZER { \
  LANG_HOOKS_MAKE_TYPE, \
  LANG_HOOKS_SIMULATE_ENUM_DECL, \
  LANG_HOOKS_SIMULATE_RECORD_DECL, \
  LANG_HOOKS_CLASSIFY_RECORD, \
  LANG_HOOKS_TYPE_FOR_MODE, \
  LANG_HOOKS_TYPE_FOR_SIZE, \
  LANG_HOOKS_GENERIC_TYPE_P, \
  LANG_HOOKS_GET_ARGUMENT_PACK_ELEMS, \
  LANG_HOOKS_TYPE_PROMOTES_TO, \
  LANG_HOOKS_REGISTER_BUILTIN_TYPE, \
  LANG_HOOKS_INCOMPLETE_TYPE_ERROR, \
  LANG_HOOKS_TYPE_MAX_SIZE, \
  LANG_HOOKS_OMP_FIRSTPRIVATIZE_TYPE_SIZES, \
  LANG_HOOKS_TYPE_HASH_EQ, \
  LANG_HOOKS_COPY_LANG_QUALIFIERS, \
  LANG_HOOKS_GET_ARRAY_DESCR_INFO, \
  LANG_HOOKS_GET_SUBRANGE_BOUNDS, \
  LANG_HOOKS_GET_TYPE_BIAS, \
  LANG_HOOKS_DESCRIPTIVE_TYPE, \
  LANG_HOOKS_RECONSTRUCT_COMPLEX_TYPE, \
  LANG_HOOKS_ENUM_UNDERLYING_BASE_TYPE, \
  LANG_HOOKS_GET_DEBUG_TYPE, \
  LANG_HOOKS_GET_FIXED_POINT_TYPE_INFO, \
  LANG_HOOKS_TYPE_DWARF_ATTRIBUTE, \
  LANG_HOOKS_UNIT_SIZE_WITHOUT_REUSABLE_PADDING, \
  LANG_HOOKS_CLASSTYPE_AS_BASE \
}

struct lang_hooks_for_types
{
   1.LANG_HOOKS_MAKE_TYPE          = tree (*make_type) (enum tree_code);
   2.LANG_HOOKS_SIMULATE_ENUM_DECL = tree (*simulate_enum_decl) (location_t, const char *, vec<string_int_pair> *);

   3.LANG_HOOKS_SIMULATE_RECORD_DECL = tree (*simulate_record_decl) (location_t loc, const char *name,
            array_slice<const tree> fields);
   4.LANG_HOOKS_CLASSIFY_RECORD      = enum classify_record (*classify_record) (tree);

   5.LANG_HOOKS_TYPE_FOR_MODE        = tree (*type_for_mode) (machine_mode, int);

  6.LANG_HOOKS_TYPE_FOR_SIZE = tree (*type_for_size) (unsigned, int);

  7.LANG_HOOKS_GENERIC_TYPE_P = bool (*generic_p) (const_tree);

  8.LANG_HOOKS_GET_ARGUMENT_PACK_ELEMS = tree (*get_argument_pack_elems) (const_tree);

  9.LANG_HOOKS_TYPE_PROMOTES_TO = = tree (*type_promotes_to) (tree);

  10.LANG_HOOKS_REGISTER_BUILTIN_TYPE =  void (*register_builtin_type) (tree, const char *);

  11.LANG_HOOKS_INCOMPLETE_TYPE_ERROR = void (*incomplete_type_error) (location_t loc, const_tree value,
             const_tree type);


  12.LANG_HOOKS_TYPE_MAX_SIZE = tree (*max_size) (const_tree);

  13.LANG_HOOKS_OMP_FIRSTPRIVATIZE_TYPE_SIZES = void (*omp_firstprivatize_type_sizes) (struct gimplify_omp_ctx *, tree);

  14.LANG_HOOKS_TYPE_HASH_EQ = bool (*type_hash_eq) (const_tree, const_tree);

  15.LANG_HOOKS_COPY_LANG_QUALIFIERS = tree (*copy_lang_qualifiers) (const_tree, const_tree);

  16.LANG_HOOKS_GET_ARRAY_DESCR_INFO = bool (*get_array_descr_info) (const_tree, struct array_descr_info *);

  17.LANG_HOOKS_GET_SUBRANGE_BOUNDS = void (*get_subrange_bounds) (const_tree, tree *, tree *);

  18.LANG_HOOKS_GET_TYPE_BIAS = tree (*get_type_bias) (const_tree);

  19.LANG_HOOKS_DESCRIPTIVE_TYPE = tree (*descriptive_type) (const_tree);

  20.LANG_HOOKS_RECONSTRUCT_COMPLEX_TYPE = tree (*reconstruct_complex_type) (tree, tree);

  21.LANG_HOOKS_ENUM_UNDERLYING_BASE_TYPE = tree (*enum_underlying_base_type) (const_tree);

  22.LANG_HOOKS_GET_DEBUG_TYPE = tree (*get_debug_type) (const_tree);

  23.LANG_HOOKS_GET_FIXED_POINT_TYPE_INFO = bool (*get_fixed_point_type_info) (const_tree,
                 struct fixed_point_type_info *);

  24.LANG_HOOKS_TYPE_DWARF_ATTRIBUTE int (*type_dwarf_attribute) (const_tree, int);

  25.LANG_HOOKS_UNIT_SIZE_WITHOUT_REUSABLE_PADDING = tree (*unit_size_without_reusable_padding) (tree);

  26.LANG_HOOKS_CLASSTYPE_AS_BASE = tree (*classtype_as_base) (const_tree);
};
*/
typedef struct _MtcsLang MtcsLang;

typedef struct _MtcsLangHookTypes
{
   //原型 lang_hooks.types.type_hash_eq #define LANG_HOOKS_TYPE_HASH_EQ   cxx_type_hash_eq
   bool (*type_hash_eq)(MtcsLang *self,const_tree cand, const_tree base);
   //原型  lang_hooks.types.copy_lang_qualifiers LANG_HOOKS_COPY_LANG_QUALIFIERS NULL
   tree (*copy_lang_qualifiers)(MtcsLang *self,const_tree typea, const_tree typeb);
   //原型  lang_hooks.types.type_for_size  #define LANG_HOOKS_TYPE_FOR_SIZE c_common_type_for_size c-objc-common.h
   tree (*type_for_size) (MtcsLang *self,int size, bool unsignedp);

}MtcsLangHookTypes;

typedef struct _MtcsLangHook
{
   /* Called to obtain the alias set to be used for an expression or type.
      Returns -1 if the language does nothing special for it.  */
   //原型 #define LANG_HOOKS_GET_ALIAS_SET gimple_get_alias_set
   alias_set_type (*get_alias_set) (MtcsLang *self,tree);

}MtcsLangHook;

struct _MtcsLang
{
    MtcsComponent parent;
    MtcsLangHookTypes types;
    MtcsLangHook hook;
    //原型 lang_hooks.custom_function_descriptors #define LANG_HOOKS_CUSTOM_FUNCTION_DESCRIPTORS  false
    bool custom_function_descriptors;
};


MtcsLang    *mtcs_lang_new(MtcsMode *mtcsMode);
bool         mtcs_lang_init_tree_and_builtins (MtcsLang *self);
//原型 lang_hooks.decls.global_bindings_p () #define LANG_HOOKS_GLOBAL_BINDINGS_P lto_global_bindings_p
bool mtcs_lang_global_bindings_p (MtcsLang *self);

#endif
