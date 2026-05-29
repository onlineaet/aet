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


/* This file handles generation of all the assembler code
   *except* the instructions of a langtion.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

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
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"
#include "gimple.h"

#include "mtcslang.h"
#include "mtcstarget.h"


/* Perform LTO-specific initialization.  */
//原型  lang_hooks.init () lto-lang.cc lto_init实现
 bool mtcs_lang_init_tree_and_builtins (MtcsLang *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int i;
      /* Initialize LTO-specific data structures.  */
   mtcsOptionsItem->x_in_lto_p = true;
      /* We need to generate LTO if running in WPA mode.  */
   mtcsOptionsItem->x_flag_generate_lto = (mtcsOptionsItem->x_flag_incremental_link == INCREMENTAL_LINK_LTO
      || mtcsOptionsItem->x_flag_wpa != NULL);
   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 00 实现 lto-lang.cc中的 LANG_HOOKS_INIT lang_hooks.init 创建 global_trees\n");

   /* Create the basic integer types.  */
   mtcs_tree_build_common_tree_nodes/*!build_common_tree_nodes*/(mtcsTree,mtcsOptionsItem->x_flag_signed_char);
   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 11  %d %d\n",mtcsOptionsItem->x_flag_generate_lto,mtcsOptionsItem->x_flag_signed_char);

   /* The global tree for the main identifier is filled in by
   language-specific front-end initialization that is not run in the
   LTO back-end.  It appears that all languages that perform such
   initialization currently do so in the same way, so we do it here.  */
   /*! 暂时不用
   if (main_identifier_node == NULL_TREE)
   main_identifier_node = get_identifier ("main");
   */
   /* In the C++ front-end, fileptr_type_node is defined as a variant
   copy of ptr_type_node, rather than ptr_node itself.  The
   distinction should only be relevant to the front-end, so we
   always use the C definition here in lto1.
   Likewise for const struct tm*.  */
   for (unsigned i = 0; i < ARRAY_SIZE (mtcsTree->builtin_structptr_types); ++i){
      gcc_assert (mtcsTree->builtin_structptr_types[i].node== mtcsTree->builtin_structptr_types[i].base);
      gcc_assert (TYPE_MAIN_VARIANT (mtcsTree->builtin_structptr_types[i].node) == mtcsTree->builtin_structptr_types[i].base);
   }

   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 22 创建共同的内建函数 mtcs_va_list_type_node:%p\n",mtcs_va_list_type_node);
   mtcs_tree_build_c_type_nodes/*!lto_build_c_type_nodes*/(mtcsTree);
   gcc_assert (mtcs_va_list_type_node);
   mtcsTree->mtcsBackupRestore.backup(&mtcsTree->mtcsBackupRestore);//备份并设设备的tree覆盖主机的global_trees
   if (TREE_CODE (mtcs_va_list_type_node) == ARRAY_TYPE){
      tree x = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (mtcs_va_list_type_node));
      mtcs_tree_define_builtins/*!lto_define_builtins*/(mtcsTree,x, x);
   }else{
      mtcs_tree_define_builtins/*!lto_define_builtins*/(mtcsTree,
            mtcs_tree_build_reference_type/*!build_reference_type*/(mtcsTree,mtcs_va_list_type_node),mtcs_va_list_type_node);
   }
   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 33 创建平台相关的内建函数和共同的内建节点\n");
   mtcsTarget/*!targetm.init_builtins*/->init_builtins(mtcsTarget);
   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 44 \n");

   mtcs_tree_build_common_builtin_nodes/*!build_common_builtin_nodes*/(mtcsTree);
   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 55 \n");
   /* Assign names to the builtin types, otherwise they'll end up
   as __unknown__ in debug info.
   ???  We simply need to stop pre-seeding the streamer cache.
   Below is modeled after from c-common.cc:c_common_nodes_and_builtins  */
#define NAME_TYPE(t,n) \
   if (t) \
      TYPE_NAME (t) = mtcs_tree_build_decl(mtcsTree,UNKNOWN_LOCATION, TYPE_DECL,get_identifier (n), t)

   NAME_TYPE (integer_type_node, "int");
   NAME_TYPE (char_type_node, "char");
   NAME_TYPE (long_integer_type_node, "long int");
   NAME_TYPE (unsigned_type_node, "unsigned int");
   NAME_TYPE (long_unsigned_type_node, "long unsigned int");
   NAME_TYPE (long_long_integer_type_node, "long long int");
   NAME_TYPE (long_long_unsigned_type_node, "long long unsigned int");
   NAME_TYPE (short_integer_type_node, "short int");
   NAME_TYPE (short_unsigned_type_node, "short unsigned int");
   if (signed_char_type_node != char_type_node)
      NAME_TYPE (signed_char_type_node, "signed char");
   if (unsigned_char_type_node != char_type_node)
      NAME_TYPE (unsigned_char_type_node, "unsigned char");
   NAME_TYPE (float_type_node, "float");
   NAME_TYPE (double_type_node, "double");
   NAME_TYPE (long_double_type_node, "long double");
   NAME_TYPE (void_type_node, "void");
   NAME_TYPE (boolean_type_node, "bool");
   NAME_TYPE (complex_float_type_node, "complex float");
   NAME_TYPE (complex_double_type_node, "complex double");
   NAME_TYPE (complex_long_double_type_node, "complex long double");

   for (i = 0; i <mtcsMode->mtcs_NUM_INT_N_ENTS; i++)
      if (mtcsMode->int_n_enabled_p[i]){
         char name[50];
         sprintf (name, "__int%d", mtcsMode->intData/*!int_n_data*/[i].bitsize);
         NAME_TYPE (mtcsTree->int_n_trees[i].signed_type, name);
      }

   #undef NAME_TYPE
   n_debug("mtcslang.c mtcs_lang_init_tree_and_builtins 66 恢复主机的global_trees\n");
   mtcsTree->mtcsBackupRestore.restore(&mtcsTree->mtcsBackupRestore);//恢复主机的global_trees

   return true;
}

//原型 lang_hooks.decls.global_bindings_p () #define LANG_HOOKS_GLOBAL_BINDINGS_P lto_global_bindings_p
bool mtcs_lang_global_bindings_p (MtcsLang *self)
{
   return cfun == NULL;
}

//原型  lang_hooks.types.type_for_size  #define LANG_HOOKS_TYPE_FOR_SIZE c_common_type_for_size c-objc-common.h
static tree typeForSize_cb (MtcsLang *self,unsigned precision, int unsignedp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   int i;
   gcc_assert(integer_type_node==mtcs_integer_type_node);
   gcc_assert(unsigned_type_node==mtcs_unsigned_type_node);
   gcc_assert(signed_char_type_node==mtcs_signed_char_type_node);
   gcc_assert(short_integer_type_node==mtcs_short_integer_type_node);
   gcc_assert(short_unsigned_type_node==mtcs_short_unsigned_type_node);
   gcc_assert(long_integer_type_node==mtcs_long_integer_type_node);
   gcc_assert(long_unsigned_type_node==mtcs_long_unsigned_type_node);
   gcc_assert(long_long_integer_type_node==mtcs_long_long_integer_type_node);
   gcc_assert(long_long_unsigned_type_node==mtcs_long_long_unsigned_type_node);

   if (precision == TYPE_PRECISION (mtcs_integer_type_node))
      return unsignedp ? mtcs_unsigned_type_node : mtcs_integer_type_node;

   if (precision == TYPE_PRECISION (mtcs_signed_char_type_node))
      return unsignedp ? mtcs_unsigned_char_type_node : mtcs_signed_char_type_node;

   if (precision == TYPE_PRECISION (mtcs_short_integer_type_node))
      return unsignedp ? mtcs_short_unsigned_type_node : mtcs_short_integer_type_node;

   if (precision == TYPE_PRECISION (mtcs_long_integer_type_node))
      return unsignedp ? mtcs_long_unsigned_type_node : mtcs_long_integer_type_node;

   if (precision == TYPE_PRECISION (mtcs_long_long_integer_type_node))
      return unsignedp  ? mtcs_long_long_unsigned_type_node : mtcs_long_long_integer_type_node;

   for (i = 0; i < mtcsMode->mtcs_NUM_INT_N_ENTS; i ++)
      if (mtcsMode->int_n_enabled_p[i] && precision == mtcsMode->intData/*!int_n_data*/[i].bitsize)
         return (unsignedp ? mtcsTree->int_n_trees[i].unsigned_type : mtcsTree->int_n_trees[i].signed_type);

   gcc_assert(intQI_type_node==mtcsTree->global_trees[TI_INTQI_TYPE]);
   gcc_assert(unsigned_intQI_type_node==mtcsTree->global_trees[TI_UINTQI_TYPE]);

   gcc_assert(intHI_type_node==mtcsTree->global_trees[TI_INTHI_TYPE]);
   gcc_assert(unsigned_intHI_type_node==mtcsTree->global_trees[TI_UINTHI_TYPE]);

   gcc_assert(intSI_type_node==mtcsTree->global_trees[TI_INTSI_TYPE]);
   gcc_assert(unsigned_intSI_type_node==mtcsTree->global_trees[TI_UINTSI_TYPE]);

   gcc_assert(intTI_type_node==mtcsTree->global_trees[TI_INTTI_TYPE]);
   gcc_assert(unsigned_intTI_type_node==mtcsTree->global_trees[TI_UINTTI_TYPE]);

   if (precision <= TYPE_PRECISION (mtcs_intQI_type_node))
      return unsignedp ? mtcs_unsigned_intQI_type_node : mtcs_intQI_type_node;

   if (precision <= TYPE_PRECISION (mtcs_intHI_type_node))
      return unsignedp ? mtcs_unsigned_intHI_type_node : mtcs_intHI_type_node;

   if (precision <= TYPE_PRECISION (mtcs_intSI_type_node))
      return unsignedp ? mtcs_unsigned_intSI_type_node : mtcs_intSI_type_node;

   if (precision <= TYPE_PRECISION (mtcs_intDI_type_node))
      return unsignedp ? mtcs_unsigned_intDI_type_node : mtcs_intDI_type_node;

   if (precision <= TYPE_PRECISION (mtcs_intTI_type_node))
      return unsignedp ? mtcs_unsigned_intTI_type_node : mtcs_intTI_type_node;

   return NULL_TREE;
}

/* Return the typed-based alias set for T, which may be an expression
   or a type.  Return -1 if we don't do anything special.  */
//原型 gimple_get_alias_set gimple.h gimple.cc
static alias_set_type getAliasSet_cb (MtcsLang *self,tree t)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlias  *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

   /* That's all the expressions we handle specially.  */
   if (!TYPE_P (t))
      return -1;

   /* For convenience, follow the C standard when dealing with
   character types.  Any object may be accessed via an lvalue that
   has character type.  */
   if (t == char_type_node
   || t == signed_char_type_node
   || t == unsigned_char_type_node)
      return 0;

   /* Allow aliasing between signed and unsigned variants of the same
   type.  We treat the signed variant as canonical.  */
   if (TREE_CODE (t) == INTEGER_TYPE && TYPE_UNSIGNED (t)){
      tree t1 = gimple_signed_type (t);

      /* t1 == t can happen for boolean nodes which are always unsigned.  */
      if (t1 != t)
         return mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,t1);
   }

   /* Allow aliasing between enumeral types and the underlying
   integer type.  This is required for C since those are
   compatible types.  */
   else if (TREE_CODE (t) == ENUMERAL_TYPE){
      tree t1 = self/*!lang_hooks.types.type_for_size*/->types.type_for_size(self,
            tree_to_uhwi (TYPE_SIZE (t)),false /* short-cut above */);
      return mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,t1);
   }
   return -1;
}


static void mtcsLangInit(MtcsLang *self)
{
    self->types.type_hash_eq=NULL;
    self->types.copy_lang_qualifiers=NULL;
    self->custom_function_descriptors = false;
    //原型  lang_hooks.types.type_for_size  #define LANG_HOOKS_TYPE_FOR_SIZE c_common_type_for_size c-objc-common.h
    self->types.type_for_size=typeForSize_cb ;
    //原型 #define LANG_HOOKS_GET_ALIAS_SET gimple_get_alias_set
    self->hook.get_alias_set=getAliasSet_cb ;

}

 MtcsLang    *mtcs_lang_new(MtcsMode *mtcsMode)
 {
    MtcsLang *self = n_slice_alloc0 (sizeof(MtcsLang));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsLangInit(self);
    return self;
 }

