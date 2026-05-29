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
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "alloc-pool.h"
#include "timevar.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs-libfuncs.h"
#include "insn-config.h"
#include "ira.h"
#include "recog.h"
#include "cgraph.h"
#include "coverage.h"
#include "diagnostic.h"
#include "varasm.h"
#include "tree-inline.h"
#include "realmpfr.h"   /* For GMP/MPFR/MPC versions, in print_version.  */
#include "version.h"
#include "flags.h"
#include "insn-attr.h"
#include "output.h"
#include "toplev.h"
#include "expr.h"
#include "intl.h"
#include "tree-diagnostic.h"
#include "reload.h"
#include "lra.h"
#include "dwarf2asm.h"
#include "debug.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "cfgloop.h" /* for init_set_costs */
#include "hosthooks.h"
#include "opts.h"
#include "opts-diagnostic.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tsan.h"
#include "plugin.h"
#include "context.h"
#include "pass_manager.h"
#include "auto-profile.h"
#include "dwarf2out.h"
#include "ipa-reference.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-utils.h"
#include "gcse.h"
#include "omp-offload.h"
#include "edit-context.h"
#include "tree-pass.h"
#include "dumpfile.h"
#include "ipa-fnsummary.h"
#include "dump-context.h"
#include "print-tree.h"
#include "optinfo-emit-json.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "ipa-param-manipulation.h"
#include "dbgcnt.h"
#include "gcc-urlifier.h"

#include "tree-pass.h"
#include "stringpool.h"
#include "gimple-ssa.h"
#include "cgraph.h"
#include "coverage.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "output.h"
#include "cfgcleanup.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "tree-into-ssa.h"
#include "tree-ssa.h"
#include "langhooks.h"
#include "toplev.h"
#include "debug.h"
#include "symbol-summary.h"
#include "tree-ssanames.h"
#include "tree-dfa.h"
#include "tree-phinodes.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "tree-iterator.h"
#include "ssa-iterators.h"
#include "gimple.h"
#include "tree-outof-ssa.h"
#include "tree-eh.h"

#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "gimple-pretty-print.h"
#include "plugin.h"
#include "ipa-fnsummary.h"
#include "ipa-utils.h"
#include "except.h"
#include "cfgloop.h"
#include "context.h"
#include "pass_manager.h"
#include "tree-nested.h"
#include "dbgcnt.h"
#include "lto-section-names.h"
#include "stringpool.h"
#include "attribs.h"
#include "ipa-inline.h"
#include "omp-offload.h"
#include "symtab-thunks.h"
#include "ipa-reference.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "tree-ssa-operands.h"
#include "cfghooks.h"

#include "mtcsport.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"
#include "mtcsprintrtl.h"

#define MTCS_CLONES "mtcsclones"

static tree mtcs_copy_tree_body_r (tree *tp, int *walk_subtrees, void *data);
//原型 remap_decl tree-inline.h tree-inline.cc
static tree remapDecl (MtcsPort *self,tree decl, copy_body_data *id);
//原型 remap_type tree-inline.h tree-inline.cc
static tree remapType (MtcsPort *self,tree type, copy_body_data *id);
static tree remap_ssa_name(MtcsPort *self,tree name, copy_body_data *id);
//原型 copy_tree_r tree-inline.h tree-inline.cc
static tree mtcs_copy_tree_r (MtcsPort *self,tree *tp, int *walk_subtrees, void *data ATTRIBUTE_UNUSED);

static void replaceGimple(MtcsPort *self,basic_block bb,gimple *stmt,copy_body_data *id);
//原型 remap_type_1 tree-inline.cc
static tree remap_type_1 (MtcsPort *self,tree type, copy_body_data *id);

static void remove_unreachble_bb(MtcsPort *self);

static void mtcsPortInit(MtcsPort *self)
{
   self->processing_debug_stmt = 0;

   //在remap时，保存类型是bitsizetype的field_decl，当移植完成后，再替换bitsizetype为mtcs的bitsizetype。
   //当调用aet_print_tree时，遇到类型是field_decl的树，会调用 dump_child ("bpos", bit_position (t));
   //bit_position(tree.cc)-->  bit_from_pos (DECL_FIELD_OFFSET (field),DECL_FIELD_BIT_OFFSET (field))(stor-alyout.cc)
   //-->size_binop(宏)-->size_binop_loc(flod-const.cc)-->int_binop_types_match_p (enum tree_code code, const_tree type1, const_tree type2)
   //在int_binop_types_match_p中会判断type1,type2的type mode是否相同。由于mtcs的bitsizetype与主机的不同，所以执行size_binop_loc 段错误。

   self->bufferArray = n_ptr_array_new();
}

static void printDef(struct function *fn)
{
   /* Keep track of SSA names present in the IL.  */
   size_t i;
   tree name;
   if(!fn)
      return;

   FOR_EACH_SSA_NAME (i, name, fn){
      gimple *stmt;
      //TREE_VISITED (name) = 0;
      //n_debug("tree-ssa.cc verify_ssa 00 %d \n",virtual_operand_p (name));
      //aet_print_tree(name);
      stmt = SSA_NAME_DEF_STMT (name);
      // n_debug("tree-ssa.cc verify_ssa 11 %d check_ssa_operands:%d\n",virtual_operand_p (name),check_ssa_operands);
      // aet_print_gimple(stmt);
      enum gimple_code code = gimple_code (stmt);
      if(code==GIMPLE_CALL){
         bool internal=gimple_call_internal_p(stmt);
         bool builtin=gimple_call_builtin_p(stmt);
         tree decl=gimple_call_fndecl(stmt);
         n_debug("mtcsport.c printDef 被调函数 %p %s 内部:%d 内建:%d %p 函数节点:%p\n",
            decl,decl?IDENTIFIER_POINTER(DECL_NAME(decl)):"NULL",
            internal,builtin,decl?decl->decl_with_vis.symtab_node:NULL);
      }

      if (!gimple_nop_p (stmt)){
         basic_block bb = gimple_bb (stmt);
         n_debug("mtcsport.c printDef 打印 ssa bb:%p i:%d virtual_operand_p:%d stmt:%p version:%d name:%p stmt:%s\n",
               bb,i,virtual_operand_p (name),stmt,SSA_NAME_VERSION (name),name,gimple_code_name[gimple_code(stmt)]);
               tree type=TREE_TYPE(name);
         n_debug("mtcsport.c printDef 打印 ssa type:%p %s %d %s\n",
               type,get_tree_code_name(TREE_CODE(type)),TYPE_MODE(type),GET_MODE_NAME(TYPE_MODE(type)));
         aet_print_gimple(stmt);
      }
   }

}

/********************移植主机的tree到设备tree********************************************/
/* Copy NODE (which must be a DECL).  The DECL originally was in the FROM_FN,
   but now it will be in the TO_FN.  PARM_TO_VAR means enable PARM_DECL to
   VAR_DECL translation.  */
//原型 copy_decl_for_dup_finish tree-inline.h tree-inline.cc
static tree mtcs_copy_decl_for_dup_finish (copy_body_data *id, tree decl, tree copy)
{
   tree *selfTree = id->decl_map->get (get_identifier (MTCS_CLONES));
   wide_int result=wi::to_wide(*selfTree);
   unsigned long long size=result.to_shwi();
   MtcsPort *self=(MtcsPort *)(size);
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsGimpleExpr *mtcsGimpleExpr=mtcs_target_get_gimple_expr(mtcsTarget);

   /* Don't generate debug information for the copy if we wouldn't have
   generated it for the copy either.  */
   DECL_ARTIFICIAL (copy) = DECL_ARTIFICIAL (decl);
   DECL_IGNORED_P (copy) = DECL_IGNORED_P (decl);

   /* Set the DECL_ABSTRACT_ORIGIN so the debugging routines know what
   declaration inspired this copy.  */
   DECL_ABSTRACT_ORIGIN (copy) = DECL_ORIGIN (decl);

   /* The new variable/label has no RTL, yet.  */
   if (HAS_RTL_P (copy)  && !TREE_STATIC (copy) && !DECL_EXTERNAL (copy))
      SET_DECL_RTL (copy, 0);//设声明的rtl为空

   machine_mode mode=DECL_MODE(decl);
   machine_mode newMode=DECL_MODE(copy);
   gcc_assert(mode==newMode);
   machine_mode deviceMode=mtcs_mode_host2device(mtcsMode,newMode);
   SET_DECL_MODE (copy, deviceMode);
   /* For vector typed decls make sure to update DECL_MODE according
   to the new function context.  */
   if (VECTOR_TYPE_P (TREE_TYPE (copy))){
      n_debug("mtcsport.c mtcs_copy_decl_for_dup_finish 00 声明类型是向量:%d\n",TYPE_MODE (TREE_TYPE (copy)),copy);
      SET_DECL_MODE (copy, TYPE_MODE (TREE_TYPE (copy)));
   }
   n_debug("mtcsport.c mtcs_copy_decl_for_dup_finish 11 原声明的mode:%d decl:%p 新:%d %p deviceMode:%d new mode:%d\n",
         mode,decl,newMode,copy,deviceMode,DECL_MODE(copy));
   /* These args would always appear unused, if not for this.  */
   TREE_USED (copy) = 1;

   /* Set the context for the new declaration.  */
   if (!DECL_CONTEXT (decl))
      /* Globals stay global.  */
      ;
   else if (DECL_CONTEXT (decl) != id->src_fn)
      /* Things that weren't in the scope of the function we're inlining
      from aren't in the scope we're inlining to, either.  */
      ;
   else if (TREE_STATIC (decl))
      /* Function-scoped static variables should stay in the original
      function.  */
      ;
   else{
      /* Ordinary automatic local variables are now in the scope of the
      new function.  */
      DECL_CONTEXT (copy) = id->dst_fn;
      if (VAR_P (copy) && id->dst_simt_vars && !mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,copy)){
         if (!lookup_attribute ("omp simt private", DECL_ATTRIBUTES (copy)))
            DECL_ATTRIBUTES (copy) = tree_cons (get_identifier ("omp simt private"), NULL, DECL_ATTRIBUTES (copy));
         id->dst_simt_vars->safe_push (copy);
      }
   }

   return copy;
}

static tree mtcs_copy_decl_no_change (tree decl, copy_body_data *id)
{
   tree copy;
   copy = copy_node (decl);
   /* The COPY is not abstract; it will be generated in DST_FN.  */
   DECL_ABSTRACT_P (copy) = false;
   lang_hooks.dup_lang_specific_decl (copy);
   /* TREE_ADDRESSABLE isn't used to indicate that a label's address has
   been taken; it's for internal bookkeeping in expand_goto_internal.  */
   if (TREE_CODE (copy) == LABEL_DECL){
      TREE_ADDRESSABLE (copy) = 0;
      LABEL_DECL_UID (copy) = -1;
   }
   return mtcs_copy_decl_for_dup_finish/*!copy_decl_for_dup_finish*/ (id, decl, copy);
}

static bool isHostGlobalTree(tree type)
{
   int i;
   for(i=0;i<TI_MAX;i++)
      if(type==global_trees[i])
         return true;
   for(i=0;i<itk_none;i++)
        if(type==integer_types[i])
           return true;
   for(i=0;i<(int) stk_type_kind_last;i++)
        if(type==sizetype_tab[i])
           return true;
   if(TREE_CODE(type)==INTEGER_TYPE){
      tree newtype = build_nonstandard_integer_type (TYPE_PRECISION (type), TYPE_UNSIGNED (type));
      if(newtype==type){
         n_debug("mtcsport.c isHostGlobalTree 找到一个非标准的整形:%p\n",type);
         return true;
      }
   }

   return false;
}

static tree hostToDeviceTree(MtcsPort *self,tree hostType)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   int i;
   for(i=0;i<TI_MAX;i++){
      if(hostType==global_trees[i]){
         n_debug("mtcsport.c  global_trees 找到类型 00 i:%d 主机:%p  MTCS:%p %s\n",
               i,hostType,mtcsTree->global_trees[i],get_tree_code_name(TREE_CODE(global_trees[i])));
          //aet_print_tree(global_trees[i]);
         return mtcsTree->global_trees[i];
      }
   }
   for(i=0;i<itk_none;i++)
      if(hostType==integer_types[i]){
         n_debug("mtcsport.c 在integer_types 找到类型 11 i:%d 主机:%p MTCS:%p %s\n",
               i,hostType,mtcsTree->integer_types[i],get_tree_code_name(TREE_CODE(integer_types[i])));
        // aet_print_tree(integer_types[i]);
         return mtcsTree->integer_types[i];
      }
   for(i=0;i<(int) stk_type_kind_last;i++)
      if(hostType==sizetype_tab[i])
         return mtcsTree->sizetype_tab[i];

   if(TREE_CODE(hostType)==INTEGER_TYPE){
        tree newtype = build_nonstandard_integer_type (TYPE_PRECISION (hostType), TYPE_UNSIGNED (hostType));
        if(newtype==hostType){
           n_debug("mtcsport.c  mtcsclones hostToDeviceTree 找到一个非标准的整形:%p\n",hostType);
           tree newtype = mtcs_tree_build_nonstandard_integer_type (mtcsTree,TYPE_PRECISION (hostType), TYPE_UNSIGNED (hostType));
           return newtype;
        }else{
           n_error("不合逻辑 hostToDeviceTree hostType:%p",hostType);
        }
    }

   return NULL_TREE;
}

/* Helper function for remap_type_2, called through walk_tree.  */
static tree remap_type_3 (tree *tp, int *walk_subtrees, void *data)
{
   copy_body_data *id = (copy_body_data *) data;
   tree *selfTree = id->decl_map->get (get_identifier (MTCS_CLONES));
   wide_int result=wi::to_wide(*selfTree);
   unsigned long long size=result.to_shwi();
   MtcsPort *self=(MtcsPort *)(size);
   n_debug("mtcsport.c remap_type_3 00 *type:%p self:%p\n",*tp,self);
   if (TYPE_P (*tp)){
      n_debug("mtcsport.c remap_type_3 11 *type:%p self:%p\n",*tp,self);
      *walk_subtrees = 0;
   }else if (DECL_P (*tp) && remapDecl(self,*tp, id) != *tp){
      n_debug("mtcsport.c remap_type_3 22 *type:%p self:%p\n",*tp,self);
      return *tp;
   }
   return NULL_TREE;
}

/* Return true if TYPE needs to be remapped because remap_decl on any
   needed embedded decl returns something other than that decl.  */
//原型 remap_type_2 tree-inline.cc
static bool remap_type_2 (MtcsPort *self,tree type, copy_body_data *id)
{
   tree t;
#define RETURN_TRUE_IF_VAR(T) \
  do                       \
    {                      \
      tree _t = (T);                \
      if (_t)                    \
   {                    \
     if (DECL_P (_t) && remapDecl(self,_t, id) != _t)    \
       return true;              \
     if (!TYPE_SIZES_GIMPLIFIED (type)       \
         && walk_tree (&_t, remap_type_3, id, NULL))  \
       return true;              \
   }                    \
    }                      \
  while (0)

   switch (TREE_CODE (type)){
      case POINTER_TYPE:
      case REFERENCE_TYPE:
      case FUNCTION_TYPE:
      case METHOD_TYPE:
         n_debug("mtcsport.c remap_type_2 00 type:%s\n",get_tree_code_name(TREE_CODE(type)));
         return remap_type_2(self,TREE_TYPE (type), id);

      case INTEGER_TYPE:
      case REAL_TYPE:
      case FIXED_POINT_TYPE:
      case ENUMERAL_TYPE:
      case BOOLEAN_TYPE:
         n_debug("mtcsport.c remap_type_2 11 type:%s\n",get_tree_code_name(TREE_CODE(type)));
         RETURN_TRUE_IF_VAR (TYPE_MIN_VALUE (type));
         RETURN_TRUE_IF_VAR (TYPE_MAX_VALUE (type));
         return false;

      case ARRAY_TYPE:
         n_debug("mtcsport.c remap_type_2 22 type:%s\n",get_tree_code_name(TREE_CODE(type)));

         if (remap_type_2(self,TREE_TYPE (type), id)
         || (TYPE_DOMAIN (type) && remap_type_2(self,TYPE_DOMAIN (type), id)))
            return true;
         break;

      case RECORD_TYPE:
      case UNION_TYPE:
      case QUAL_UNION_TYPE:
         n_debug("mtcsport.c remap_type_2 33 type:%s\n",get_tree_code_name(TREE_CODE(type)));
         for (t = TYPE_FIELDS (type); t; t = DECL_CHAIN (t))
            if (TREE_CODE (t) == FIELD_DECL){
               RETURN_TRUE_IF_VAR (DECL_FIELD_OFFSET (t));
                RETURN_TRUE_IF_VAR (DECL_SIZE (t));
                RETURN_TRUE_IF_VAR (DECL_SIZE_UNIT (t));
               if (TREE_CODE (type) == QUAL_UNION_TYPE)
                  RETURN_TRUE_IF_VAR (DECL_QUALIFIER (t));
            }
         break;

      default:
         return false;
   }

   RETURN_TRUE_IF_VAR (TYPE_SIZE (type));
   RETURN_TRUE_IF_VAR (TYPE_SIZE_UNIT (type));
   return false;
 #undef RETURN_TRUE_IF_VAR
}


/* Subprogram of following function.  Called by walk_tree.
   Return *TP if it is an automatic variable or parameter of the
   function passed in as DATA.  */
static tree find_var_from_fn (tree *tp, int *walk_subtrees, void *data)
{
   tree fn = (tree) data;
   if (TYPE_P (*tp))
      *walk_subtrees = 0;
   else if (DECL_P (*tp) && auto_var_in_fn_p (*tp, fn))
      return *tp;

   return NULL_TREE;
}

/* Returns true if T is, contains, or refers to a type with variable
   size.  For METHOD_TYPEs and FUNCTION_TYPEs we exclude the
   arguments, but not the return type.  If FN is nonzero, only return
   true if a modifier of the type or position of FN is a variable or
   parameter inside FN.

   This concept is more general than that of C99 'variably modified types':
   in C99, a struct type is never variably modified because a VLA may not
   appear as a structure member.  However, in GNU C code like:

     struct S { int i[f()]; };

   is valid, and other languages may define similar constructs.  */
//原型 tree.h tree.cc
static bool mtcs_variably_modified_type_p (MtcsPort *self,tree type, tree fn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree t;

   /* Test if T is either variable (if FN is zero) or an expression containing
   a variable in FN.  If TYPE isn't gimplified, return true also if
   gimplify_one_sizepos would gimplify the expression into a local
   variable.  */
   #define RETURN_TRUE_IF_VAR(T)                \
   do { tree _t = (T);                     \
   if (_t != NULL_TREE                   \
   && _t != error_mark_node               \
   && !CONSTANT_CLASS_P (_t)              \
   && TREE_CODE (_t) != PLACEHOLDER_EXPR           \
   && (!fn                       \
   || (!TYPE_SIZES_GIMPLIFIED (type)           \
   && (TREE_CODE (_t) != VAR_DECL            \
   && !CONTAINS_PLACEHOLDER_P (_t)))        \
   || walk_tree (&_t, find_var_from_fn, fn, NULL)))     \
   return true;  } while (0)

   n_debug("mtcsport.c mtcs_variably_modified_type_p 00 type:%p,name:%s\n",type,get_tree_code_name(TREE_CODE(type)));

   if (type == error_mark_node)
      return false;

   /* If TYPE itself has variable size, it is variably modified.  */
   RETURN_TRUE_IF_VAR (TYPE_SIZE (type));
   RETURN_TRUE_IF_VAR (TYPE_SIZE_UNIT (type));

   n_debug("mtcsport.c mtcs_variably_modified_type_p 11 type:%p,name:%s\n",type,get_tree_code_name(TREE_CODE(type)));

   switch (TREE_CODE (type)){
      case POINTER_TYPE:
      case REFERENCE_TYPE:
      case VECTOR_TYPE:
         /* Ada can have pointer types refering to themselves indirectly.  */
         if (TREE_VISITED (type))
            return false;
         TREE_VISITED (type) = true;
         if (mtcs_variably_modified_type_p(self,TREE_TYPE (type), fn)){
            TREE_VISITED (type) = false;
            return true;
         }
         TREE_VISITED (type) = false;
         break;

      case FUNCTION_TYPE:
      case METHOD_TYPE:
         /* If TYPE is a function type, it is variably modified if the
         return type is variably modified.  */
         if (mtcs_variably_modified_type_p(self,TREE_TYPE (type), fn))
            return true;
         break;

      case INTEGER_TYPE:
      case REAL_TYPE:
      case FIXED_POINT_TYPE:
      case ENUMERAL_TYPE:
      case BOOLEAN_TYPE:
         /* Scalar types are variably modified if their end points
         aren't constant.  */
         n_debug("mtcsport.c mtcs_variably_modified_type_p 22 type:%p,name:%s\n",type,get_tree_code_name(TREE_CODE(type)));
        RETURN_TRUE_IF_VAR (TYPE_MIN_VALUE (type));
        RETURN_TRUE_IF_VAR (TYPE_MAX_VALUE(type));
         n_debug("mtcsport.c mtcs_variably_modified_type_p 33 type:%p,name:%s\n",type,get_tree_code_name(TREE_CODE(type)));
         break;

      case RECORD_TYPE:
      case UNION_TYPE:
      case QUAL_UNION_TYPE:
         /* We can't see if any of the fields are variably-modified by the
         definition we normally use, since that would produce infinite
         recursion via pointers.  */
         /* This is variably modified if some field's type is.  */
         for (t = TYPE_FIELDS (type); t; t = DECL_CHAIN (t))
            if (TREE_CODE (t) == FIELD_DECL){
               n_debug("mtcsport.c mtcs_variably_modified_type_p 44 FIELD_DECL type:%p,name:%s\n",
                          type,get_tree_code_name(TREE_CODE(type)));
               RETURN_TRUE_IF_VAR (DECL_FIELD_OFFSET (t));
               RETURN_TRUE_IF_VAR (DECL_SIZE (t));
               RETURN_TRUE_IF_VAR (DECL_SIZE_UNIT (t));
                  /* If the type is a qualified union, then the DECL_QUALIFIER
               of fields can also be an expression containing a variable.  */
               if (TREE_CODE (type) == QUAL_UNION_TYPE)
                  RETURN_TRUE_IF_VAR (DECL_QUALIFIER (t));

               /* If the field is a qualified union, then it's only a container
               for what's inside so we look into it.  That's necessary in LTO
               mode because the sizes of the field tested above have been set
               to PLACEHOLDER_EXPRs by free_lang_data.  */
               if (TREE_CODE (TREE_TYPE (t)) == QUAL_UNION_TYPE  && mtcs_variably_modified_type_p(self,TREE_TYPE (t), fn))
                  return true;
            }
         break;

      case ARRAY_TYPE:
         /* Do not call ourselves to avoid infinite recursion.  This is
         variably modified if the element type is.  */
         RETURN_TRUE_IF_VAR (TYPE_SIZE (TREE_TYPE (type)));
         RETURN_TRUE_IF_VAR (TYPE_SIZE_UNIT (TREE_TYPE (type)));
         break;

      default:
         break;
   }

   /* The current language may have other cases to check, but in general,
   all other types are not variably modified.  */
   return lang_hooks.tree_inlining.var_mod_type_p (type, fn);
#undef RETURN_TRUE_IF_VAR

}

//原型 remap_type_1 tree-inline.cc
static tree remap_type_1 (MtcsPort *self,tree type, copy_body_data *id,nboolean deviceTree)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsAttribs  *mtcsAttribs=mtcs_target_get_attribs(mtcsTarget);

   tree new_tree, t;
   char *typeCodeName=get_tree_code_name(TREE_CODE (type));
   n_debug("mtsport.c remap_type_1 00 type:%s type:%p integer_type_node：%p,void_type_node：%p  deviceTree:%d\ typeMode:%d\n",
         typeCodeName,type,integer_type_node,void_type_node,deviceTree,TYPE_MODE(type));
   if(TREE_CODE (type) == VOID_TYPE){
      if(deviceTree)
         return  hostToDeviceTree(self,type);
      return type;
   }
   /* We do need a copy.  build and register it now.  If this is a pointer or
   reference type, remap the designated type and make a new pointer or
   reference type.  */
   if (TREE_CODE (type) == POINTER_TYPE){
      machine_mode mode=mtcs_mode_host2device(mtcsMode,TYPE_MODE (type));
      n_debug("mtsport.c remap_type_1 11 类型是指针 type:%s type:%p mode:%d deviceMode:%d\n",
               get_tree_code_name(TREE_CODE (TREE_TYPE (type))),TREE_TYPE (type),TYPE_MODE (type),mode);
      new_tree = mtcs_tree_build_pointer_type_for_mode/*!build_pointer_type_for_mode*/(mtcsTree,
            remapType(self,TREE_TYPE (type), id),  mode/*!TYPE_MODE (type)*/, TYPE_REF_CAN_ALIAS_ALL (type));
      if (TYPE_ATTRIBUTES (type) || TYPE_QUALS (type))
         new_tree = mtcs_attribs_build_type_attribute_qual_variant/*!build_type_attribute_qual_variant*/(mtcsAttribs,
                  new_tree,TYPE_ATTRIBUTES (type),TYPE_QUALS (type));
      insert_decl_map (id, type, new_tree);
      return new_tree;
   }else if (TREE_CODE (type) == REFERENCE_TYPE){
      machine_mode mode=mtcs_mode_host2device(mtcsMode,TYPE_MODE (type));
      new_tree = mtcs_tree_build_reference_type_for_mode/*!build_reference_type_for_mode*/(mtcsTree,
            remapType(self,TREE_TYPE (type), id),  mode/*!TYPE_MODE (type)*/,TYPE_REF_CAN_ALIAS_ALL (type));
      if (TYPE_ATTRIBUTES (type) || TYPE_QUALS (type))
         new_tree = mtcs_attribs_build_type_attribute_qual_variant/*!build_type_attribute_qual_variant*/(mtcsAttribs,
               new_tree,TYPE_ATTRIBUTES (type),TYPE_QUALS (type));
      insert_decl_map (id, type, new_tree);
      return new_tree;
   }else if(TREE_CODE (type) == FUNCTION_TYPE){
      tree newRetnType=remapType(self,TREE_TYPE(type),id);
      tree at=TYPE_ARG_TYPES (type);
      n_debug("mtcsport.c remap_type_1 22 类型是函数类型 FUNCTION_TYPE 创建 funciton_type old:%p new:%p\n",type,at);
      walk_tree (&at, mtcs_copy_tree_body_r, id, NULL);
      tree newFnType =mtcs_tree_build_function_type (mtcsTree,newRetnType, at);
      n_debug("mtcsport.c remap_type_1 33 类型是函数类型 FUNCTION_TYPE old:%p new:%p at:%p\n",type,newFnType,at);
      insert_decl_map (id, type, newFnType);//缓存function_type
      return newFnType;
   }else{
      if(!deviceTree){
         new_tree = copy_node (type);
         //解决 bug 026
         switch(TREE_CODE(new_tree)){
            case ENUMERAL_TYPE:
            {
               machine_mode deviceMode=mtcs_mode_host2device(mtcsMode,TYPE_MODE(new_tree));
               n_debug("mtcsport.c remap_type_1 这是枚举类型 设TYPE_MODE 老:%p mode:%d 新:%p mode:%d deviceTree:%d\n",
                    type,TYPE_MODE(type),new_tree,deviceMode,deviceTree);
               SET_TYPE_MODE(new_tree,deviceMode);
            }
               break;
            case COMPLEX_TYPE:
            {
               machine_mode deviceMode=mtcs_mode_host2device(mtcsMode,TYPE_MODE(new_tree));
               n_debug("mtcsport.c remap_type_1 这是复数类型 设TYPE_MODE 老:%p mode:%d 新:%p mode:%d deviceTree:%d\n",
                         type,TYPE_MODE(type),new_tree,deviceMode,deviceTree);
               SET_TYPE_MODE(new_tree,deviceMode);
               //COMPLEX_TYPE 的 TREE_TYPE 才是实数类型
               tree oldType=TREE_TYPE(new_tree);
               tree realType=remapType(self,oldType,id);
               TREE_TYPE(new_tree)=realType;
               insert_decl_map (id, oldType, realType);//缓存function_type
            }
               break;
            default:
               break;
         }
      }else
         new_tree = hostToDeviceTree(self,type);
   }
   insert_decl_map (id, type, new_tree);
   char *newName=get_tree_code_name(TREE_CODE (new_tree));

   /* This is a new type, not a copy of an old type.  Need to reassociate
   variants.  We can handle everything except the main variant lazily.  */
   t = TYPE_MAIN_VARIANT (type);
   if (type != t){
      n_debug("mtsport.c remap_type_1 44  type != TYPE_MAIN_VARIANT (type) new_tree:%s %p type:%s %p t:%s %p\n",
            newName,new_tree,typeCodeName,type,get_tree_code_name(TREE_CODE (t)),t);
      t = remapType(self,t, id);
      TYPE_MAIN_VARIANT (new_tree) = t;
      TYPE_NEXT_VARIANT (new_tree) = TYPE_NEXT_VARIANT (t);
      TYPE_NEXT_VARIANT (t) = new_tree;
      SET_TYPE_MODE(new_tree,TYPE_MODE(t));//重要 类型的mode由t决定 当 type有修饰如const volatile等时 类型的类型存在TYPE_MAIN_VARIANT中
   }else{
      TYPE_MAIN_VARIANT (new_tree) = new_tree;
      TYPE_NEXT_VARIANT (new_tree) = NULL;
   }

   if (TYPE_STUB_DECL (type))
      TYPE_STUB_DECL (new_tree) = remapDecl(self,TYPE_STUB_DECL (type), id);

   /* Lazily create pointer and reference types.  */
   TYPE_POINTER_TO (new_tree) = NULL;
   TYPE_REFERENCE_TO (new_tree) = NULL;

   /* Copy all types that may contain references to local variables; be sure to
   preserve sharing in between type and its main variant when possible.  */
   switch (TREE_CODE (new_tree)){
      case INTEGER_TYPE:
      case REAL_TYPE:
      case FIXED_POINT_TYPE:
      case ENUMERAL_TYPE:
      case BOOLEAN_TYPE:
         if (TYPE_MAIN_VARIANT (new_tree) != new_tree){
            n_debug("mtsport.c remap_type_1 55   new_tree:%s new_tree:%p type:%s %p \n",
                  newName,new_tree,typeCodeName,type);
            gcc_checking_assert (TYPE_MIN_VALUE (type) == TYPE_MIN_VALUE (TYPE_MAIN_VARIANT (type)));
            gcc_checking_assert (TYPE_MAX_VALUE (type) == TYPE_MAX_VALUE (TYPE_MAIN_VARIANT (type)));
            walk_tree (&TYPE_MIN_VALUE (TYPE_MAIN_VARIANT (new_tree)), mtcs_copy_tree_body_r, id, NULL);
            walk_tree (&TYPE_MAX_VALUE (TYPE_MAIN_VARIANT (new_tree)), mtcs_copy_tree_body_r, id, NULL);
            TYPE_MIN_VALUE (new_tree) = TYPE_MIN_VALUE (TYPE_MAIN_VARIANT (new_tree));
            TYPE_MAX_VALUE (new_tree) = TYPE_MAX_VALUE (TYPE_MAIN_VARIANT (new_tree));

         }else{
            n_debug("mtsport.c remap_type_1 66  新 new_tree:%s %p mode:%d 老 type:%s %p mode:%d\n",
                        newName,new_tree,TYPE_MODE(new_tree),typeCodeName,type,TYPE_MODE(type));
            t = TYPE_MIN_VALUE (new_tree);
            if (t && TREE_CODE (t) != INTEGER_CST){
               walk_tree (&TYPE_MIN_VALUE (new_tree), mtcs_copy_tree_body_r, id, NULL);
            }
            t = TYPE_MAX_VALUE (new_tree);
            if (t && TREE_CODE (t) != INTEGER_CST){
               walk_tree (&TYPE_MAX_VALUE (new_tree), mtcs_copy_tree_body_r, id, NULL);
            }
         }
         break;
         //return new_tree;

//      case FUNCTION_TYPE:
//        // if (TYPE_MAIN_VARIANT (new_tree) != new_tree   && TREE_TYPE (type) == TREE_TYPE (TYPE_MAIN_VARIANT (type)))
//         //   TREE_TYPE (new_tree) = TREE_TYPE (TYPE_MAIN_VARIANT (new_tree));
//        // else
//         n_debug("mtsport.c remap_type_1 66 function_type \n");
//          TREE_TYPE (new_tree) = remapType(self,TREE_TYPE (new_tree), id);
//        // if (TYPE_MAIN_VARIANT (new_tree) != new_tree
//        // && TYPE_ARG_TYPES (type) == TYPE_ARG_TYPES (TYPE_MAIN_VARIANT (type)))
//          //  TYPE_ARG_TYPES (new_tree) = TYPE_ARG_TYPES (TYPE_MAIN_VARIANT (new_tree));
//        // else
//            walk_tree (&TYPE_ARG_TYPES (new_tree), mtcs_copy_tree_body_r, id, NULL);
//         return new_tree;

      case ARRAY_TYPE:
         n_debug("mtsport.c remap_type_1 77   ARRAY_TYPE new_tree:%s new_tree:%p type:%s %p \n",
                          newName,new_tree,typeCodeName,type);
         if (TYPE_MAIN_VARIANT (new_tree) != new_tree
         && TREE_TYPE (type) == TREE_TYPE (TYPE_MAIN_VARIANT (type))){
            TREE_TYPE (new_tree) = TREE_TYPE (TYPE_MAIN_VARIANT (new_tree));
            n_debug("mtsport.c remap_type_1 88   ARRAY_TYPE new_tree:%s new_tree:%p type:%s %p \n",
                                newName,new_tree,typeCodeName,type);
         }else{
            n_debug("mtsport.c remap_type_1 99  remapType ARRAY_TYPE new_tree:%s new_tree:%p type:%s %p \n",
                                newName,new_tree,typeCodeName,type);
            TREE_TYPE (new_tree) = remapType(self,TREE_TYPE (new_tree), id);
         }

         if (TYPE_MAIN_VARIANT (new_tree) != new_tree){
            gcc_checking_assert (TYPE_DOMAIN (type) == TYPE_DOMAIN (TYPE_MAIN_VARIANT (type)));
            TYPE_DOMAIN (new_tree) = TYPE_DOMAIN (TYPE_MAIN_VARIANT (new_tree));
         }else{
            n_debug("mtsport.c remap_type_1 100 TYPE_DOMAIN  ARRAY_TYPE new_tree:%s new_tree:%p type:%s %p \n",
                                   newName,new_tree,typeCodeName,type);
            TYPE_DOMAIN (new_tree) = remapType(self,TYPE_DOMAIN (new_tree), id);
            /* For array bounds where we have decided not to copy over the bounds
            variable which isn't used in OpenMP/OpenACC region, change them to
            an uninitialized VAR_DECL temporary.  */
            if (id->adjust_array_error_bounds
            && TYPE_DOMAIN (new_tree)
            && TYPE_MAX_VALUE (TYPE_DOMAIN (new_tree)) == error_mark_node
            && TYPE_MAX_VALUE (TYPE_DOMAIN (type)) != error_mark_node){
               tree v = create_tmp_var (TREE_TYPE (TYPE_DOMAIN (new_tree)));
               DECL_ATTRIBUTES (v) = tree_cons (get_identifier ("omp dummy var"), NULL_TREE,DECL_ATTRIBUTES (v));
               TYPE_MAX_VALUE (TYPE_DOMAIN (new_tree)) = v;
            }
         }
         break;

      case RECORD_TYPE:
      case UNION_TYPE:
      case QUAL_UNION_TYPE:
         n_debug("mtsport.c remap_type_1 101 %s\n",get_tree_code_name(TREE_CODE(new_tree)));
         aet_print_tree(new_tree);
         if (TYPE_MAIN_VARIANT (type) != type && TYPE_FIELDS (type) == TYPE_FIELDS (TYPE_MAIN_VARIANT (type))){
            n_debug("mtsport.c remap_type_1 102 %s\n",get_tree_code_name(TREE_CODE(new_tree)));

            TYPE_FIELDS (new_tree) = TYPE_FIELDS (TYPE_MAIN_VARIANT (new_tree));
         }else{
            tree f, nf = NULL;
            for (f = TYPE_FIELDS (new_tree); f ; f = DECL_CHAIN (f)){
               n_debug("mtsport.c remap_type_1 103 %s\n",get_tree_code_name(TREE_CODE(new_tree)));

               t = remapDecl(self,f, id);
               n_debug("mtsport.c remap_type_1 104 %s\n",get_tree_code_name(TREE_CODE(new_tree)));
               DECL_ABSTRACT_ORIGIN (t)=NULL_TREE;
               aet_print_tree(t);

               DECL_CONTEXT (t) = new_tree;
               DECL_CHAIN (t) = nf;
               nf = t;
            }
            TYPE_FIELDS (new_tree) = nreverse (nf);
         }
         break;
      case COMPLEX_TYPE:
         n_debug("mtsport.c remap_type_1 是复数类型 :%s\n",get_tree_code_name(TREE_CODE (new_tree)));
         break;
      case OFFSET_TYPE:
      default:
         /* Shouldn't have been thought variable sized.  */
         n_debug("mtsport.c remap_type_1 出错 :%s mode:%d %s\n",get_tree_code_name(TREE_CODE (new_tree)),TYPE_MODE(new_tree),GET_MODE_NAME(TYPE_MODE(new_tree)));
         aet_print_tree(new_tree);
         gcc_unreachable ();
   }

   /* All variants of type share the same size, so use the already remaped data.  */
   if (TYPE_MAIN_VARIANT (new_tree) != new_tree){
      tree s = TYPE_SIZE (type);
      tree mvs = TYPE_SIZE (TYPE_MAIN_VARIANT (type));
      tree su = TYPE_SIZE_UNIT (type);
      tree mvsu = TYPE_SIZE_UNIT (TYPE_MAIN_VARIANT (type));
      gcc_checking_assert ((TREE_CODE (s) == PLACEHOLDER_EXPR && (TREE_CODE (mvs) == PLACEHOLDER_EXPR))  || s == mvs);
      gcc_checking_assert ((TREE_CODE (su) == PLACEHOLDER_EXPR  && (TREE_CODE (mvsu) == PLACEHOLDER_EXPR)) || su == mvsu);
      n_debug("mtsport.c remap_type_1 105 %s\n",get_tree_code_name(TREE_CODE(new_tree)));

      TYPE_SIZE (new_tree) = TYPE_SIZE (TYPE_MAIN_VARIANT (new_tree));
      TYPE_SIZE_UNIT (new_tree) = TYPE_SIZE_UNIT (TYPE_MAIN_VARIANT (new_tree));
   }else{
      //n_debug("mtsport.c remap_type_1 106 %s\n",get_tree_code_name(TREE_CODE(TYPE_SIZE (new_tree))));

      walk_tree (&TYPE_SIZE (new_tree), mtcs_copy_tree_body_r, id, NULL);
     // n_debug("mtsport.c remap_type_1 107 新 new_tree  的 TYPE_SIZE_UNIT %s %p new_tree :%p\n",
          //  get_tree_code_name(TREE_CODE(TYPE_SIZE_UNIT (new_tree))),TYPE_SIZE_UNIT (new_tree),new_tree);

      walk_tree (&TYPE_SIZE_UNIT (new_tree), mtcs_copy_tree_body_r, id, NULL);
   }

   return new_tree;
}


//原型 remap_type tree-inline.h
static tree remapType (MtcsPort *self,tree type, copy_body_data *id)
{
   tree *node;
   tree tmp;
   if (type == NULL){
      n_debug("mtcsport.c remapType 00 :类型是空的，返回\n");
      return type;
   }
   /* See if we have remapped this type.  */
   node = id->decl_map->get (type);
   if (node){
      n_debug("mtcsport.c remapType 11 :类型已缓存在decl_map中 type:%p name:%s mode:%d 新类型 node:%p %s mode:%d\n",
            type,get_tree_code_name(TREE_CODE(type)),TYPE_MODE(type),*node,get_tree_code_name(TREE_CODE(*node)),TYPE_MODE(*node));
      return *node;
   }

   //查找是不是主机全局节点，如果是 说明可以用mtcstree替换
   nboolean deviceTree=FALSE;
   if(isHostGlobalTree(type)){
      tree new_tree=hostToDeviceTree(self,type);
      n_debug("mtcsport.c remapType 22 类型是主机中的缺省类型，可以在mtcs找到对应的类型 type:%p name:%s new_tree:%p old mode:%d new mode:%d\n",
            type,get_tree_code_name(TREE_CODE(type)),new_tree,TYPE_MODE(type),TYPE_MODE(new_tree));
       deviceTree=TRUE;
   }

   /* The type only needs remapping if it's variably modified.  */
   if (! mtcs_variably_modified_type_p (self,type, id->src_fn)
       /* Don't remap if copy_decl method doesn't always return a new
     decl and for all embedded decls returns the passed in decl.  */
       || (id->dont_remap_vla_if_no_change && !remap_type_2 (self,type, id))){
       //insert_decl_map (id, type, type);
       //return type;
       n_debug("mtcsport.c remapType 33 在主机中是不用map，因为:%s mode:%d\n",get_tree_code_name(TREE_CODE(type)),TYPE_MODE(type));
   }

   n_debug("mtcsport.c remapType 44 执行 remap_type_1 type:%p %s deviceTree:%d\n",
         type,get_tree_code_name(TREE_CODE(type)),deviceTree);

   id->remapping_type_depth++;
   tmp = remap_type_1(self,type, id,deviceTree);
   id->remapping_type_depth--;
   n_debug("mtcsport.c remapType 55 执行 remap_type_1 完成 type:%p 新类型:%p id->remapping_type_depth:%d\n",
         type,tmp,id->remapping_type_depth);

   return tmp;
}


/* Remap the dependence CLIQUE from the source to the destination function
   as specified in ID.  */

static unsigned short remap_dependence_clique (MtcsPort *self,copy_body_data *id, unsigned short clique)
{
   if (clique == 0 || self->processing_debug_stmt)
      return 0;
   if (!id->dependence_map)
      id->dependence_map = new hash_map<dependence_hash, unsigned short>;
   bool existed;
   unsigned short &newc = id->dependence_map->get_or_insert (clique, &existed);
   if (!existed){
      /* Clique 1 is reserved for local ones set by PTA.  */
      if (cfun->last_clique == 0)
         cfun->last_clique = 1;
      newc = get_new_clique (cfun);
   }
   return newc;
}

/* Decide if DECL can be put into BLOCK_NONLOCAL_VARs.  */
static bool can_be_nonlocal (tree decl, copy_body_data *id)
{
  /* We cannot duplicate function decls.  */
  if (TREE_CODE (decl) == FUNCTION_DECL)
    return true;

  /* Local static vars must be non-local or we get multiple declaration
     problems.  */
  if (VAR_P (decl) && !auto_var_in_fn_p (decl, id->src_fn))
    return true;

  return false;
}


static tree remap_decls (MtcsPort *self,tree decls, vec<tree, va_gc> **nonlocalized_list,
         copy_body_data *id)
{
   tree old_var;
   tree new_decls = NULL_TREE;

   /* Remap its variables.  */
   for (old_var = decls; old_var; old_var = DECL_CHAIN (old_var)){
      tree new_var;

      if (can_be_nonlocal (old_var, id)){
         /* We need to add this variable to the local decls as otherwise
         nothing else will do so.  */
         if (VAR_P (old_var) && ! DECL_EXTERNAL (old_var) && cfun)
            add_local_decl (cfun, old_var);
         if ((!optimize || debug_info_level > DINFO_LEVEL_TERSE)
         && !DECL_IGNORED_P (old_var) && nonlocalized_list)
            vec_safe_push (*nonlocalized_list, old_var);
         continue;
      }

      /* Remap the variable.  */
      new_var = remapDecl(self,old_var, id);

      /* If we didn't remap this variable, we can't mess with its
      TREE_CHAIN.  If we remapped this variable to the return slot, it's
      already declared somewhere else, so don't declare it here.  */

      if (new_var == old_var || new_var == id->retvar)
         ;
      else if (!new_var){
         if ((!optimize || debug_info_level > DINFO_LEVEL_TERSE)
         && !DECL_IGNORED_P (old_var)
         && nonlocalized_list)
            vec_safe_push (*nonlocalized_list, old_var);
      }else{
         gcc_assert (DECL_P (new_var));
         DECL_CHAIN (new_var) = new_decls;
         new_decls = new_var;

         /* Also copy value-expressions.  */
         if (VAR_P (new_var) && DECL_HAS_VALUE_EXPR_P (new_var)){
            tree tem = DECL_VALUE_EXPR (new_var);
            bool old_regimplify = id->regimplify;
            id->remapping_type_depth++;
            walk_tree (&tem, mtcs_copy_tree_body_r, id, NULL);
            id->remapping_type_depth--;
            id->regimplify = old_regimplify;
            SET_DECL_VALUE_EXPR (new_var, tem);
         }
      }
   }

   return nreverse (new_decls);
}

/* Return true if DECL is a parameter or a SSA_NAME for a parameter.  */
/* Copy the BLOCK to contain remapped versions of the variables
   therein.  And hook the new block into the block-tree.  */
//原型 remap_block tree-inline.cc
static void remap_block (MtcsPort *self,tree *block, copy_body_data *id)
{
  tree old_block;
  tree new_block;

  /* Make the new block.  */
  old_block = *block;
  new_block = make_node (BLOCK);
  TREE_USED (new_block) = TREE_USED (old_block);
  BLOCK_ABSTRACT_ORIGIN (new_block) = BLOCK_ORIGIN (old_block);
  BLOCK_SOURCE_LOCATION (new_block) = BLOCK_SOURCE_LOCATION (old_block);
  BLOCK_NONLOCALIZED_VARS (new_block) = vec_safe_copy (BLOCK_NONLOCALIZED_VARS (old_block));
  *block = new_block;

  /* Remap its variables.  */
  BLOCK_VARS (new_block) = remap_decls(self,BLOCK_VARS (old_block), &BLOCK_NONLOCALIZED_VARS (new_block),id);

  /* Remember the remapped block.  */
  insert_decl_map (id, old_block, new_block);
}

static bool is_parm (tree decl)
{
   if (TREE_CODE (decl) == SSA_NAME){
      decl = SSA_NAME_VAR (decl);
      if (!decl)
         return false;
   }

   return (TREE_CODE (decl) == PARM_DECL);
}

static void copy_bind_expr (MtcsPort *self,tree *tp, int *walk_subtrees, copy_body_data *id)
{
   tree block = BIND_EXPR_BLOCK (*tp);
   /* Copy (and replace) the statement.  */
   mtcs_copy_tree_r(self,tp, walk_subtrees, id);
   if (block){
      remap_block(self,&block, id);
      BIND_EXPR_BLOCK (*tp) = block;
   }

   if (BIND_EXPR_VARS (*tp))
      /* This will remap a lot of the same decls again, but this should be
      harmless.  */
      BIND_EXPR_VARS (*tp) = remap_decls(self,BIND_EXPR_VARS (*tp), NULL, id);
}

/* The SAVE_EXPR pointed to by TP is being copied.  If ST contains
   information indicating to what new SAVE_EXPR this one should be mapped,
   use that one.  Otherwise, create a new node and enter it in ST.  FN is
   the function into which the copy will be placed.  */

static void remap_save_expr (tree *tp, hash_map<tree, tree> *st, int *walk_subtrees)
{
   tree *n;
   tree t;
   /* See if we already encountered this SAVE_EXPR.  */
   n = st->get (*tp);
   /* If we didn't already remap this SAVE_EXPR, do so now.  */
   if (!n){
      t = copy_node (*tp);
      /* Remember this SAVE_EXPR.  */
      st->put (*tp, t);
      /* Make sure we don't remap an already-remapped SAVE_EXPR.  */
      st->put (t, t);
   }else{
         /* We've already walked into this SAVE_EXPR; don't do it again.  */
      *walk_subtrees = 0;
      t = *n;
   }

   /* Replace this SAVE_EXPR with the copy.  */
   *tp = t;
}


static void copy_statement_list (tree *tp)
{
   tree_stmt_iterator oi, ni;
   tree new_tree;

   new_tree = alloc_stmt_list ();
   ni = tsi_start (new_tree);
   oi = tsi_start (*tp);
   TREE_TYPE (new_tree) = TREE_TYPE (*tp);
   *tp = new_tree;

   for (; !tsi_end_p (oi); tsi_next (&oi)){
      tree stmt = tsi_stmt (oi);
      if (TREE_CODE (stmt) == STATEMENT_LIST)
      /* This copy is not redundant; tsi_link_after will smash this
      STATEMENT_LIST into the end of the one we're building, and we
      don't want to do that with the original.  */
         copy_statement_list (&stmt);
      tsi_link_after (&ni, stmt, TSI_CONTINUE_LINKING);
   }
}

/* Construct new SSA name for old NAME. ID is the inline context.  */
//原型 remap_ssa_name tree-inline.cc
static tree remap_ssa_name(MtcsPort *self,tree name, copy_body_data *id)
{
   tree new_tree, var;
   tree *n;

   gcc_assert (TREE_CODE (name) == SSA_NAME);

   n = id->decl_map->get (name);
   if (n){
      /* When we perform edge redirection as part of CFG copy, IPA-SRA can
      remove an unused LHS from a call statement.  Such LHS can however
      still appear in debug statements, but their value is lost in this
      function and we do not want to map them.  */
      if (id->killed_new_ssa_names
      && id->killed_new_ssa_names->contains (*n)){
         gcc_assert (self->processing_debug_stmt);
         self->processing_debug_stmt = -1;
         return name;
      }
      return unshare_expr (*n);
   }

   if (self->processing_debug_stmt){
      if (SSA_NAME_IS_DEFAULT_DEF (name)
      && TREE_CODE (SSA_NAME_VAR (name)) == PARM_DECL
      && id->entry_bb == NULL
      && single_succ_p (ENTRY_BLOCK_PTR_FOR_FN (cfun))){
         gimple *def_temp;
         gimple_stmt_iterator gsi;
         tree val = SSA_NAME_VAR (name);
         n = id->decl_map->get (val);
         if (n != NULL)
            val = *n;
         if (TREE_CODE (val) != PARM_DECL && !(VAR_P (val) && DECL_ABSTRACT_ORIGIN (val))){
            self->processing_debug_stmt = -1;
            return name;
         }
         n = id->decl_map->get (val);
         if (n && TREE_CODE (*n) == DEBUG_EXPR_DECL)
            return *n;
         tree vexpr = build_debug_expr_decl (TREE_TYPE (name));
         /* FIXME: Is setting the mode really necessary? */
         SET_DECL_MODE (vexpr, DECL_MODE (SSA_NAME_VAR (name)));
         n_debug("mtcsport.c remap_ssa_name 00 SSA_NAME_VAR (name):mode:%d %p",DECL_MODE (SSA_NAME_VAR (name)),SSA_NAME_VAR (name));
         def_temp = gimple_build_debug_source_bind (vexpr, val, NULL);
         gsi = gsi_after_labels (single_succ (ENTRY_BLOCK_PTR_FOR_FN (cfun)));
         gsi_insert_before (&gsi, def_temp, GSI_SAME_STMT);
         insert_decl_map (id, val, vexpr);
         return vexpr;
      }
      self->processing_debug_stmt = -1;
      return name;
   }

   /* Remap anonymous SSA names or SSA names of anonymous decls.  */
   var = SSA_NAME_VAR (name);
   if (!var
   || (!SSA_NAME_IS_DEFAULT_DEF (name)
   && VAR_P (var)
   && !VAR_DECL_IS_VIRTUAL_OPERAND (var)
   && DECL_ARTIFICIAL (var)
   && DECL_IGNORED_P (var)
   && !DECL_NAME (var))){

      n_debug("mtcsport.c remap_ssa_name 11 var:%p name:%p \n",var,name);
      aet_print_tree(TREE_TYPE (name));

      struct ptr_info_def *pi;
      new_tree = name;//make_ssa_name (remapType(self,TREE_TYPE (name), id));
      //new_tree =make_ssa_name (remapType(self,TREE_TYPE (name), id));

      tree newtype=remapType(self,TREE_TYPE (name), id);
      if(TYPE_P(newtype)){
           TREE_TYPE (new_tree) = TYPE_MAIN_VARIANT (newtype);
           SET_SSA_NAME_VAR_OR_IDENTIFIER (new_tree, NULL_TREE);
      }
     // SET_SSA_NAME_VAR_OR_IDENTIFIER(new_tree,newtype);
      n_debug("mtcsport.c remap_ssa_name 22 var:%p name:%p new_tree:%p\n",var,name,new_tree);
      aet_print_tree(name);

      if (!var && SSA_NAME_IDENTIFIER (name)){
         n_debug("mtcsport.c remap_ssa_name 33 var:%p name:%p new_tree:%p %p\n",var,name,new_tree,SSA_NAME_IDENTIFIER (name));

         SET_SSA_NAME_VAR_OR_IDENTIFIER (new_tree, SSA_NAME_IDENTIFIER (name));
      }
      insert_decl_map (id, name, new_tree);
      SSA_NAME_OCCURS_IN_ABNORMAL_PHI (new_tree) = SSA_NAME_OCCURS_IN_ABNORMAL_PHI (name);
      /* At least IPA points-to info can be directly transferred.  */
      if (id->src_cfun->gimple_df
      && id->src_cfun->gimple_df->ipa_pta
      && POINTER_TYPE_P (TREE_TYPE (name))
      && (pi = SSA_NAME_PTR_INFO (name))
      && !pi->pt.anything){
         struct ptr_info_def *new_pi = get_ptr_info (new_tree);
         new_pi->pt = pi->pt;
      }
      /* So can range-info.  */
      if (!POINTER_TYPE_P (TREE_TYPE (name)) && SSA_NAME_RANGE_INFO (name)){
         n_debug("mtcsport.c remap_ssa_name 44 类型不是指针并且有range_info name:%p new_tree:%p\n",name,new_tree);
         if(new_tree!=name)
            duplicate_ssa_name_range_info (new_tree, name);
      }
      return new_tree;
   }

   /* Do not set DEF_STMT yet as statement is not copied yet. We do that
   in copy_bb.  */
   new_tree = remapDecl(self,var, id);
   n_debug("mtcsport.c remap_ssa_name 55 name:%p var:%p new_tree:%p\n",name,var,new_tree);
   aet_print_tree(new_tree);

   /* We might've substituted constant or another SSA_NAME for
   the variable.

   Replace the SSA name representing RESULT_DECL by variable during
   inlining:  this saves us from need to introduce PHI node in a case
   return value is just partly initialized.  */
   if ((VAR_P (new_tree) || TREE_CODE (new_tree) == PARM_DECL)
   && (!SSA_NAME_VAR (name)
   || TREE_CODE (SSA_NAME_VAR (name)) != RESULT_DECL
   || !id->transform_return_to_modify)){
      struct ptr_info_def *pi;
      //new_tree = make_ssa_name (new_tree);
      TREE_TYPE (name) = TREE_TYPE (new_tree);
      SET_SSA_NAME_VAR_OR_IDENTIFIER (name, new_tree);
      new_tree=name;
      insert_decl_map (id, name, new_tree);
      n_debug("mtcsport.c remap_ssa_name 66 创建新的ssa var:%p new_tree:%p\n",var,new_tree);
      SSA_NAME_OCCURS_IN_ABNORMAL_PHI (new_tree) = SSA_NAME_OCCURS_IN_ABNORMAL_PHI (name);
      /* At least IPA points-to info can be directly transferred.  */
      if (id->src_cfun->gimple_df
      && id->src_cfun->gimple_df->ipa_pta
      && POINTER_TYPE_P (TREE_TYPE (name))
      && (pi = SSA_NAME_PTR_INFO (name))
      && !pi->pt.anything){
         struct ptr_info_def *new_pi = get_ptr_info (new_tree);
         new_pi->pt = pi->pt;
      }
      /* So can range-info.  */
      if (!POINTER_TYPE_P (TREE_TYPE (name)) && SSA_NAME_RANGE_INFO (name))
         if(new_tree!=name)
            duplicate_ssa_name_range_info (new_tree, name);
      if (SSA_NAME_IS_DEFAULT_DEF (name)){
         n_debug("mtcsport.c remap_ssa_name 77 SSA_NAME_IS_DEFAULT_DEF (name) \n");

         /* By inlining function having uninitialized variable, we might
         extend the lifetime (variable might get reused).  This cause
         ICE in the case we end up extending lifetime of SSA name across
         abnormal edge, but also increase register pressure.

         We simply initialize all uninitialized vars by 0 except
         for case we are inlining to very first BB.  We can avoid
         this for all BBs that are not inside strongly connected
         regions of the CFG, but this is expensive to test.  */
         if (id->entry_bb  && SSA_NAME_OCCURS_IN_ABNORMAL_PHI (name)
         && (!SSA_NAME_VAR (name) || TREE_CODE (SSA_NAME_VAR (name)) != PARM_DECL)
         && (id->entry_bb != EDGE_SUCC (ENTRY_BLOCK_PTR_FOR_FN (cfun), 0)->dest
         || EDGE_COUNT (id->entry_bb->preds) != 1)){
            gimple_stmt_iterator gsi = gsi_last_bb (id->entry_bb);
            gimple *init_stmt;
            tree zero = build_zero_cst (TREE_TYPE (new_tree));

            init_stmt = gimple_build_assign (new_tree, zero);
            gsi_insert_after (&gsi, init_stmt, GSI_NEW_STMT);
            n_debug("mtcsport.c remap_ssa_name 88\n");
            SSA_NAME_IS_DEFAULT_DEF (new_tree) = 0;
         }else{
            n_debug("mtcsport.c remap_ssa_name 99 设 SSA_NAME_DEF_STMT\n");
            SSA_NAME_DEF_STMT (new_tree) = gimple_build_nop ();
            set_ssa_default_def (cfun, SSA_NAME_VAR (new_tree), new_tree);
         }
      }
   }else{
      n_debug("mtcsport.c remap_ssa_name 100 插入 name:%p new_tree:%p\n",name,new_tree);
      insert_decl_map (id, name, new_tree);
   }
   return new_tree;
}

static tree findFunction(MtcsPort *self,tree hostFndecl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   tree newdecl=NULL_TREE;
   symbol_table *save=symtab;
   symtab=mtcsTarget->symtab;
   const char *hostName =IDENTIFIER_POINTER(DECL_NAME(hostFndecl));
   struct cgraph_node *cnode;
   FOR_EACH_FUNCTION (cnode){
      tree decl= cnode->decl;
      const char *newName =IDENTIFIER_POINTER(DECL_NAME(decl));
      if(!strcmp(newName,hostName)){
         newdecl = decl;
         break;
      }
   }
   //切换到主机的符号表
   symtab=save;
   return newdecl;
}

//复制变量的初始化值
//static __device__ __attribute__ ((__used__)) void *_TFirst_deviceFuncPointers[]={_Z6TFirst10testkernelEPN6TFirstE};
//初始化的值是函数地址
static void cloneInit(MtcsPort *self, tree constructor ,copy_body_data *id)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   unsigned HOST_WIDE_INT cnt;
   constructor_elt *ce;
   for (cnt = 0;vec_safe_iterate (CONSTRUCTOR_ELTS (constructor), cnt, &ce);cnt++){
      tree val = ce->value;
      n_debug("mtcsport.c cloneInit 00 cnt:%d\n",cnt);
      aet_print_tree(val);
      if(TREE_CODE(val)==ADDR_EXPR){
         tree op=TREE_OPERAND(val,0);
         if(TREE_CODE(op)==FUNCTION_DECL){
            tree *n = id->decl_map->get (op);
            n_debug("mtcsport.c cloneInit 11 cnt:%d val:%p n:%p\n",cnt,val,n);
            if(!n){
               tree newdecl=findFunction(self,op);
               n_debug("mtcsport.c cloneInit 22 cnt:%d %p\n",cnt,newdecl);

               if(newdecl){
                  n_debug("mtcsport.c cloneInit 找到函数地址:新:%p 老:%p\n",newdecl,val);
                  insert_decl_map (id, op, newdecl);
                  //ce->value = newdecl;
                  TREE_OPERAND(val,0)=newdecl;
               }
            }
         }
      }
   }
}

/* Passed to walk_tree.  Copies the node pointed to, if appropriate.  */
//原型 copy_tree_r tree-inline.h tree-inline.cc
static tree mtcs_copy_tree_r (MtcsPort *self,tree *tp, int *walk_subtrees, void *data ATTRIBUTE_UNUSED)
{
   copy_body_data *id = (copy_body_data *) data;
   enum tree_code code = TREE_CODE (*tp);
   enum tree_code_class cl = TREE_CODE_CLASS (code);

   /* We make copies of most nodes.  */
   if (IS_EXPR_CODE_CLASS (cl)
   || code == TREE_LIST
   || code == TREE_VEC
   || code == TYPE_DECL
   || code == OMP_CLAUSE){
      /* Because the chain gets clobbered when we make a copy, we save it
      here.  */
      tree chain = NULL_TREE, new_tree;

      if (CODE_CONTAINS_STRUCT (code, TS_COMMON))
         chain = TREE_CHAIN (*tp);

      /* Copy the node.  */
      new_tree = copy_node (*tp);
      n_debug("mtcsport.c mtcs_copy_tree_r 00 NEW_TREE:%p *tp:%p code:%s\n",new_tree,*tp,get_tree_code_name(code));
      aet_print_tree(new_tree);
      if(code==TREE_LIST){
         tree type = TREE_VALUE (new_tree);
         tree newType=remapType(self,type,id);
         TREE_VALUE (new_tree)=newType;
      }
      *tp = new_tree;
      /* Now, restore the chain, if appropriate.  That will cause
      walk_tree to walk into the chain as well.  */
      if (code == PARM_DECL
      || code == TREE_LIST
      || code == OMP_CLAUSE)
         TREE_CHAIN (*tp) = chain;

      /* For now, we don't update BLOCKs when we make copies.  So, we
      have to nullify all BIND_EXPRs.  */
      if (TREE_CODE (*tp) == BIND_EXPR)
         BIND_EXPR_BLOCK (*tp) = NULL_TREE;
   }else if (code == CONSTRUCTOR){
      /* CONSTRUCTOR nodes need special handling because
      we need to duplicate the vector of elements.  */

      tree new_tree;
      new_tree = copy_node (*tp);
      CONSTRUCTOR_ELTS (new_tree) = vec_safe_copy (CONSTRUCTOR_ELTS (*tp));
      n_debug("mtcsport.c mtcs_copy_tree_r 11 NEW_TREE:%p *tp:%p code:%s\n",new_tree,*tp,get_tree_code_name(code));
      cloneInit(self,new_tree,id);
      *tp = new_tree;
   }else if (code == STATEMENT_LIST)
      /* We used to just abort on STATEMENT_LIST, but we can run into them
      with statement-expressions (c++/40975).  */
      copy_statement_list (tp);
   else if (TREE_CODE_CLASS (code) == tcc_type)
      *walk_subtrees = 0;
   else if (TREE_CODE_CLASS (code) == tcc_declaration)
      *walk_subtrees = 0;
   else if (TREE_CODE_CLASS (code) == tcc_constant)
      *walk_subtrees = 0;

   return NULL_TREE;
}


/* Called from copy_body_id via walk_tree.  DATA is really a
   `copy_body_data *'.  */
static tree mtcs_copy_tree_body_r (tree *tp, int *walk_subtrees, void *data)
{
   copy_body_data *id = (copy_body_data *) data;
   tree fn = id->src_fn;
   tree new_block;

   tree *selfTree = id->decl_map->get (get_identifier (MTCS_CLONES));
   wide_int result=wi::to_wide(*selfTree);
   unsigned long long size=result.to_shwi();
   MtcsPort *self=(MtcsPort *)(size);
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   n_debug("mtcsport.c mtcs_copy_tree_body_r 00  *tp:%p %s\n",*tp,get_tree_code_name(TREE_CODE(*tp)));

   /* Begin by recognizing trees that we'll completely rewrite for the
   inlining context.  Our output for these trees is completely
   different from out input (e.g. RETURN_EXPR is deleted, and morphs
   into an edge).  Further down, we'll handle trees that get
   duplicated and/or tweaked.  */

   /* When requested, RETURN_EXPRs should be transformed to just the
   contained MODIFY_EXPR.  The branch semantics of the return will
   be handled elsewhere by manipulating the CFG rather than a statement.  */
   if (TREE_CODE (*tp) == RETURN_EXPR && id->transform_return_to_modify){
      n_debug("mtcsport.c mtcs_copy_tree_body_r 11 RETURN_EXPR %p\n",*tp);

      tree assignment = TREE_OPERAND (*tp, 0);

      /* If we're returning something, just turn that into an
      assignment into the equivalent of the original RESULT_DECL.
      If the "assignment" is just the result decl, the result
      decl has already been set (e.g. a recent "foo (&result_decl,
      ...)"); just toss the entire RETURN_EXPR.  */
      if (assignment && TREE_CODE (assignment) == MODIFY_EXPR){
         /* Replace the RETURN_EXPR with (a copy of) the
         MODIFY_EXPR hanging underneath.  */
         *tp = copy_node (assignment);
      }else /* Else the RETURN_EXPR returns no value.  */{
         *tp = NULL;
         return (tree) (void *)1;
      }
   }else if (TREE_CODE (*tp) == SSA_NAME){
      n_debug("mtcsport.c mtcs_copy_tree_body_r 22 SSA_NAME *tp:%p %s\n",*tp,get_tree_code_name(TREE_CODE(*tp)));
      *tp = remap_ssa_name(self,*tp, id);
      *walk_subtrees = 0;
      return NULL;
   }
   /* Local variables and labels need to be replaced by equivalent
   variables.  We don't want to copy static variables; there's only
   one of those, no matter how many times we inline the containing
   function.  Similarly for globals from an outer function.  */
   else if (auto_var_in_fn_p (*tp, fn)){
      tree new_decl;

      /* Remap the declaration.  */
      new_decl = remapDecl(self,*tp, id);
      n_debug("mtcsport.c mtcs_copy_tree_body_r 33  auto_var_in_fn_p (*tp, fn) *tp:%p %s new_decl:%p\n",
            *tp,get_tree_code_name(TREE_CODE(*tp)),new_decl);

      gcc_assert (new_decl);
      /* Replace this variable with the copy.  */
      STRIP_TYPE_NOPS (new_decl);
      *tp = new_decl;
      *walk_subtrees = 0;
   }else if (TREE_CODE (*tp) == STATEMENT_LIST)
      copy_statement_list (tp);
   else if (TREE_CODE (*tp) == SAVE_EXPR || TREE_CODE (*tp) == TARGET_EXPR)
      remap_save_expr (tp, id->decl_map, walk_subtrees);
   else if (TREE_CODE (*tp) == LABEL_DECL  && (! DECL_CONTEXT (*tp) || decl_function_context (*tp) == id->src_fn)){
      /* These may need to be remapped for EH handling.  */
      *tp = remapDecl(self,*tp, id);
     n_debug("mtcsport.c mtcs_copy_tree_body_r 44 *tp:%p %s \n",*tp,get_tree_code_name(TREE_CODE(*tp)));
   }else if(TREE_CODE (*tp) == VAR_DECL && ( DECL_EXTERNAL(*tp) ||   TREE_STATIC(*tp) ||  TREE_PUBLIC(*tp))){
      struct cgraph_node *cnode = (*tp)->decl_with_vis.symtab_node;
      varpool_node *vnode;
      tree old = *tp;
      tree map=NULL_TREE;
      FOR_EACH_VARIABLE (vnode){

        char *varName = IDENTIFIER_POINTER(DECL_NAME(old));
        char *globalName = IDENTIFIER_POINTER(DECL_NAME(vnode->decl));
        n_debug("mtcsport.c mtcs_copy_tree_body_r 44aa 找全局变量所在的节点 old:%p tp:%p node:%p decl:%p node:%p %s %s %d %d %d %d %d %d %d %d %d %d %d %d\n",
               old,*tp,cnode,vnode->decl,vnode,varName,globalName,DECL_EXTERNAL(old),DECL_EXTERNAL(vnode->decl),
               TREE_PUBLIC(old) , TREE_PUBLIC(vnode->decl),TREE_STATIC(old),TREE_STATIC(vnode->decl),
               HAS_DECL_ASSEMBLER_NAME_P(old),HAS_DECL_ASSEMBLER_NAME_P(vnode->decl),
               DECL_ASSEMBLER_NAME_SET_P (old),DECL_ASSEMBLER_NAME_SET_P (vnode->decl),DECL_ASSEMBLER_NAME (old) != DECL_NAME (old),
               DECL_ASSEMBLER_NAME (vnode->decl) != DECL_NAME (vnode->decl));

        if(!strcmp(varName,globalName)
              && HAS_DECL_ASSEMBLER_NAME_P(old)
              && HAS_DECL_ASSEMBLER_NAME_P(vnode->decl)
              && DECL_ASSEMBLER_NAME_SET_P(old)
              && DECL_ASSEMBLER_NAME_SET_P(vnode->decl)
              && !strcmp(IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME (old)),IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME (vnode->decl)))
              /*&& DECL_ASSEMBLER_NAME (old) == DECL_NAME (old)
              && DECL_ASSEMBLER_NAME (vnode->decl) == DECL_NAME (vnode->decl)*/
              && ((DECL_EXTERNAL(old)==DECL_EXTERNAL(vnode->decl))
             || (TREE_PUBLIC(old)==TREE_PUBLIC(vnode->decl))
             || (TREE_STATIC(old)==TREE_STATIC(vnode->decl))) ){
           n_debug("mtcsport.c mtcs_copy_tree_body_r 44bb 找全局变量所在的节点 old:%p tp:%p node:%p decl:%p node:%p\n",
                 old,*tp,cnode,vnode->decl,vnode);
          cnode = vnode->decl->decl_with_vis.symtab_node;
          map = remapDecl(self,vnode->decl, id);
          break;
        }
      }
      if(map)
         *tp=map;
      else{

         *tp = remapDecl(self,*tp, id);
         n_debug("mtcsport.c mtcs_copy_tree_body_r 44cc 在 varpool中没有找到节点 old:%p new:%p cnode:%p\n",
                old,*tp,cnode);
      }
      (*tp)->decl_with_rtl.rtl =NULL;
      (*tp)->decl_with_vis.symtab_node = cnode;       //不加这两句，出现 bug 013
      if(cnode)
         (*tp)->decl_with_vis.symtab_node->decl = (*tp);

//
//
//
//                 if(vnode->decl==old){
//                    n_debug("mtcsport.c mtcs_copy_tree_body_r 44dd 找全局变量所在的节点 old:%p tp:%p node:%p\n",old,*tp,cnode);
//
//                    (*tp)->decl_with_vis.symtab_node = vnode;
//                    (*tp)->decl_with_vis.symtab_node->decl = (*tp);
//                    break;
//                 }
//              }
//
//      struct cgraph_node *cnode = (*tp)->decl_with_vis.symtab_node;
//      tree old= *tp;
//      n_debug("mtcsport.c mtcs_copy_tree_body_r 44aa 这是一个全局变量 *tp:%p node:%p\n",*tp,cnode);
//      //这里还是主机的 rtl 所以设为空留到 mtcs_expr_expand_expr_real_1 调用  mtcs_asm_decl_rtl 生成平台相关的 rtl
//      *tp = remapDecl(self,*tp, id);
//      (*tp)->decl_with_rtl.rtl =NULL;
//      (*tp)->decl_with_vis.symtab_node = cnode;       //不加这两句，出现 bug 013
//      if(cnode){
//         (*tp)->decl_with_vis.symtab_node = cnode;
//         (*tp)->decl_with_vis.symtab_node->decl = (*tp);
//      }else{
//         n_debug("mtcsport.c mtcs_copy_tree_body_r 44bb 找全局变量所在的节点 old:%p tp:%p node:%p\n",old,*tp,cnode);
//         varpool_node *vnode;
//         FOR_EACH_VARIABLE (vnode){
//            n_debug("mtcsport.c mtcs_copy_tree_body_r 44cc 找全局变量所在的节点 old:%p tp:%p node:%p decl:%p node:%p\n",
//                  old,*tp,cnode,vnode->decl,vnode);
//
//            if(vnode->decl==old){
//               n_debug("mtcsport.c mtcs_copy_tree_body_r 44dd 找全局变量所在的节点 old:%p tp:%p node:%p\n",old,*tp,cnode);
//
//               (*tp)->decl_with_vis.symtab_node = vnode;
//               (*tp)->decl_with_vis.symtab_node->decl = (*tp);
//               break;
//            }
//         }
//      }
   }else if (TREE_CODE (*tp) == BIND_EXPR)
      copy_bind_expr(self,tp, walk_subtrees, id);
   /* Types may need remapping as well.  */
   else if (TYPE_P (*tp))
      *tp = remapType(self,*tp, id);
   /* If this is a constant, we have to copy the node iff the type will be
   remapped.  copy_tree_r will not copy a constant.  */
   else if (CONSTANT_CLASS_P (*tp)){
      tree new_type = remapType(self,TREE_TYPE (*tp), id);
      n_debug("mtcsport.c mtcs_copy_tree_body_r 55 常数 *tp:%p %s new_type:%p mode:%d\n",
            *tp,get_tree_code_name(TREE_CODE(*tp)),new_type,TYPE_MODE(new_type));

      if (new_type == TREE_TYPE (*tp))
         *walk_subtrees = 0;
      else if (TREE_CODE (*tp) == INTEGER_CST){
         n_debug("mtcsport.c mtcs_copy_tree_body_r 66 用新类型创建常数 *tp:%p %s\n",*tp,get_tree_code_name(TREE_CODE(*tp)));
         *tp = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,new_type, wi::to_wide (*tp));
      }else{
         n_debug("mtcsport.c mtcs_copy_tree_body_r 77 常数 *tp:%p %s\n",*tp,get_tree_code_name(TREE_CODE(*tp)));

         *tp = copy_node (*tp);
         TREE_TYPE (*tp) = new_type;
      }
   }
   /* Otherwise, just copy the node.  Note that copy_tree_r already
   knows not to copy VAR_DECLs, etc., so this is safe.  */
   else{
      n_debug("mtcsport.c mtcs_copy_tree_body_r 88 处理剩于类型 *tp:%p %s \n",*tp,get_tree_code_name(TREE_CODE(*tp)));

      /* Here we handle trees that are not completely rewritten.
      First we detect some inlining-induced bogosities for
      discarding.  */
      if (TREE_CODE (*tp) == MODIFY_EXPR && TREE_OPERAND (*tp, 0) == TREE_OPERAND (*tp, 1)
      && (auto_var_in_fn_p (TREE_OPERAND (*tp, 0), fn))){
         /* Some assignments VAR = VAR; don't generate any rtl code
         and thus don't count as variable modification.  Avoid
         keeping bogosities like 0 = 0.  */
         tree decl = TREE_OPERAND (*tp, 0), value;
         tree *n;

         n = id->decl_map->get (decl);
         if (n){
            value = *n;
            STRIP_TYPE_NOPS (value);
            if (TREE_CONSTANT (value) || TREE_READONLY (value)){
               *tp = build_empty_stmt (EXPR_LOCATION (*tp));
               return mtcs_copy_tree_body_r(tp, walk_subtrees, data);
            }
         }
      }else if (INDIRECT_REF_P (*tp)){
         /* Get rid of *& from inline substitutions that can happen when a
         pointer argument is an ADDR_EXPR.  */
         tree decl = TREE_OPERAND (*tp, 0);
         tree *n = id->decl_map->get (decl);
         n_debug("mtcsport.c mtcs_copy_tree_body_r 99 剩于类型中的 INDIRECT_REF_P *tp:%p %s %p\n",
               *tp,get_tree_code_name(TREE_CODE(*tp)),n);

         if (n){
            /* If we happen to get an ADDR_EXPR in n->value, strip
            it manually here as we'll eventually get ADDR_EXPRs
            which lie about their types pointed to.  In this case
            build_fold_indirect_ref wouldn't strip the INDIRECT_REF,
            but we absolutely rely on that.  As fold_indirect_ref
            does other useful transformations, try that first, though.  */
            tree type = TREE_TYPE (*tp);
            tree ptr = id->do_not_unshare ? *n : unshare_expr (*n);
            tree old = *tp;
            *tp = id->do_not_fold ? NULL : gimple_fold_indirect_ref (ptr);
            if (! *tp){
               type = remapType(self,type, id);
               if (TREE_CODE (ptr) == ADDR_EXPR && !id->do_not_fold){
                  *tp = fold_indirect_ref_1 (EXPR_LOCATION (ptr), type, ptr);
                  /* ???  We should either assert here or build
                  a VIEW_CONVERT_EXPR instead of blindly leaking
                  incompatible types to our IL.  */
                  if (! *tp)
                     *tp = TREE_OPERAND (ptr, 0);
               }else{
                  *tp = build1 (INDIRECT_REF, type, ptr);
                  TREE_THIS_VOLATILE (*tp) = TREE_THIS_VOLATILE (old);
                  TREE_SIDE_EFFECTS (*tp) = TREE_SIDE_EFFECTS (old);
                  TREE_READONLY (*tp) = TREE_READONLY (old);
                  /* We cannot propagate the TREE_THIS_NOTRAP flag if we
                  have remapped a parameter as the property might be
                  valid only for the parameter itself.  */
                  if (TREE_THIS_NOTRAP (old) && (!is_parm (TREE_OPERAND (old, 0)) || (!id->transform_parameter && is_parm (ptr))))
                     TREE_THIS_NOTRAP (*tp) = 1;
               }
            }
            *walk_subtrees = 0;
            return NULL;
         }
      }else if (TREE_CODE (*tp) == MEM_REF && !id->do_not_fold){
         n_debug("mtcsport.c mtcs_copy_tree_body_r 100 剩于类型中的 MEM_REF *tp:%p %s \n",
               *tp,get_tree_code_name(TREE_CODE(*tp)));

         /* We need to re-canonicalize MEM_REFs from inline substitutions
         that can happen when a pointer argument is an ADDR_EXPR.
         Recurse here manually to allow that.  */
         tree ptr = TREE_OPERAND (*tp, 0);
         tree type = remapType(self,TREE_TYPE (*tp), id);
         tree old = *tp;
         walk_tree (&ptr, mtcs_copy_tree_body_r, data, NULL);
         *tp = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MEM_REF, type, ptr, TREE_OPERAND (*tp, 1));
         TREE_THIS_VOLATILE (*tp) = TREE_THIS_VOLATILE (old);
         TREE_SIDE_EFFECTS (*tp) = TREE_SIDE_EFFECTS (old);
         copy_warning (*tp, old);
         if (MR_DEPENDENCE_CLIQUE (old) != 0){
            MR_DEPENDENCE_CLIQUE (*tp) = remap_dependence_clique(self,id, MR_DEPENDENCE_CLIQUE (old));
            MR_DEPENDENCE_BASE (*tp) = MR_DEPENDENCE_BASE (old);
         }
         /* We cannot propagate the TREE_THIS_NOTRAP flag if we have
         remapped a parameter as the property might be valid only
         for the parameter itself.  */
         if (TREE_THIS_NOTRAP (old) && (!is_parm (TREE_OPERAND (old, 0)) || (!id->transform_parameter && is_parm (ptr))))
            TREE_THIS_NOTRAP (*tp) = 1;
         REF_REVERSE_STORAGE_ORDER (*tp) = REF_REVERSE_STORAGE_ORDER (old);
         *walk_subtrees = 0;
         return NULL;
      }

      /* Here is the "usual case".  Copy this tree node, and then
      tweak some special cases.  */
      n_debug("mtcsport.c mtcs_copy_tree_body_r 101 处理剩于类型 *tp:%p %s\n",*tp,get_tree_code_name(TREE_CODE(*tp)));

      mtcs_copy_tree_r(self,tp, walk_subtrees, (void*)id);
      n_debug("mtcsport.c mtcs_copy_tree_body_r 102 处理剩于类型 *tp:%p %s\n",*tp,get_tree_code_name(TREE_CODE(*tp)));

      /* If EXPR has block defined, map it to newly constructed block.
      When inlining we want EXPRs without block appear in the block
      of function call if we are not remapping a type.  */
      if (EXPR_P (*tp)){
         new_block = id->remapping_type_depth == 0 ? id->block : NULL;
         n_debug("mtcsport.c mtcs_copy_tree_body_r 103 处理剩于类型 *tp:%p %s %p\n",
               *tp,get_tree_code_name(TREE_CODE(*tp)),new_block);

         if (TREE_BLOCK (*tp)){
            tree *n;
            n = id->decl_map->get (TREE_BLOCK (*tp));
            n_debug("mtcsport.c mtcs_copy_tree_body_r 104 处理剩于类型 *tp:%p %s %p %p\n",
                          *tp,get_tree_code_name(TREE_CODE(*tp)),new_block,n);
            if (n)
               new_block = *n;
         }
         TREE_SET_BLOCK (*tp, new_block);
      }
      n_debug("mtcsport.c mtcs_copy_tree_body_r 105xx 处理剩于类型 *tp type:%p %p\n",TREE_TYPE (*tp),
            TREE_TYPE (*tp)? TYPE_MAIN_VARIANT (TREE_TYPE (*tp)):NULL_TREE);

      if (TREE_CODE (*tp) != OMP_CLAUSE)
         TREE_TYPE (*tp) = remapType(self,TREE_TYPE (*tp), id);
      n_debug("mtcsport.c mtcs_copy_tree_body_r 105 处理剩于类型 *tp:%p %s type:%p typemainvariant:%p\n",
            *tp,get_tree_code_name(TREE_CODE(*tp)), TREE_TYPE (*tp),TREE_TYPE (*tp)?TYPE_MAIN_VARIANT (TREE_TYPE (*tp)):NULL_TREE);

      /* The copied TARGET_EXPR has never been expanded, even if the
      original node was expanded already.  */
      if (TREE_CODE (*tp) == TARGET_EXPR && TREE_OPERAND (*tp, 3)){
         TREE_OPERAND (*tp, 1) = TREE_OPERAND (*tp, 3);
         TREE_OPERAND (*tp, 3) = NULL_TREE;
      }
      /* Variable substitution need not be simple.  In particular, the
      INDIRECT_REF substitution above.  Make sure that TREE_CONSTANT
      and friends are up-to-date.  */
      else if (TREE_CODE (*tp) == ADDR_EXPR){
         int invariant = is_gimple_min_invariant (*tp);
         n_debug("mtcsport.c mtcs_copy_tree_body_r 106 处理 ADDR_EXPR *tp:%p %s invariant:%d\n",
               *tp,get_tree_code_name(TREE_CODE(*tp)),invariant);

         walk_tree (&TREE_OPERAND (*tp, 0), mtcs_copy_tree_body_r, id, NULL);
         n_debug("mtcsport.c mtcs_copy_tree_body_r 107 *tp:%p %s invariant:%d\n",*tp,get_tree_code_name(TREE_CODE(*tp)),invariant);

         /* Handle the case where we substituted an INDIRECT_REF
         into the operand of the ADDR_EXPR.  */
         if (INDIRECT_REF_P (TREE_OPERAND (*tp, 0))   && !id->do_not_fold){
            n_debug("mtcsport.c mtcs_copy_tree_body_r 108 *tp:%p %s invariant:%d\n",*tp,get_tree_code_name(TREE_CODE(*tp)),invariant);

            tree t = TREE_OPERAND (TREE_OPERAND (*tp, 0), 0);
            if (TREE_TYPE (t) != TREE_TYPE (*tp))
               t = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,remapType(self,TREE_TYPE (*tp), id), t);
            *tp = t;
         }else{
            n_debug("mtcsport.c mtcs_copy_tree_body_r 109 *tp:%p %s invariant:%d\n",*tp,get_tree_code_name(TREE_CODE(*tp)),invariant);
            recompute_tree_invariant_for_addr_expr (*tp);
         }
         /* If this used to be invariant, but is not any longer,
         then regimplification is probably needed.  */
         if (invariant && !is_gimple_min_invariant (*tp)){
            n_debug("mtcsport.c mtcs_copy_tree_body_r 110 *tp:%p %s invariant:%d\n",*tp,get_tree_code_name(TREE_CODE(*tp)),invariant);

            id->regimplify = true;
         }

         *walk_subtrees = 0;
      }else if (TREE_CODE (*tp) == OMP_CLAUSE
      && (OMP_CLAUSE_CODE (*tp) == OMP_CLAUSE_AFFINITY
      || OMP_CLAUSE_CODE (*tp) == OMP_CLAUSE_DEPEND)){
         tree t = OMP_CLAUSE_DECL (*tp);
         if (t && TREE_CODE (t) == TREE_LIST && TREE_PURPOSE (t) && TREE_CODE (TREE_PURPOSE (t)) == TREE_VEC){
            *walk_subtrees = 0;
            OMP_CLAUSE_DECL (*tp) = copy_node (t);
            t = OMP_CLAUSE_DECL (*tp);
            TREE_PURPOSE (t) = copy_node (TREE_PURPOSE (t));
            for (int i = 0; i <= 4; i++)
               walk_tree (&TREE_VEC_ELT (TREE_PURPOSE (t), i),mtcs_copy_tree_body_r, id, NULL);
            if (TREE_VEC_ELT (TREE_PURPOSE (t), 5))
               remap_block(self,&TREE_VEC_ELT (TREE_PURPOSE (t), 5), id);
            walk_tree (&TREE_VALUE (t), mtcs_copy_tree_body_r, id, NULL);
         }
      }else if(TREE_CODE (*tp) == COMPONENT_REF){

         tree field=  TREE_OPERAND (*tp, 1);
         n_debug("mtcsport.c mtcs_copy_tree_body_r 111 *tp:%p map COMMPONENT_REr的field——decl %s\n",
               *tp,get_tree_code_name(TREE_CODE(field)));
         if(TREE_CODE(field)==FIELD_DECL)
            TREE_OPERAND (*tp, 1) = remapDecl(self,field,id);
      }else if(TREE_CODE (*tp) == FUNCTION_DECL){
         n_debug("mtcsport.c mtcs_copy_tree_body_r 112  remapdecl funciton_decl old:%p\n",*tp);
         *tp = remapDecl(self,*tp, id);
      }

      /*
      else if(TREE_CODE (*tp) == MEM_REF){
         n_debug("mtcsport.c mtcs_copy_tree_body_r 111 处理mem_ref :%p\n",*tp);
         tree node=*tp;
         tree mainvariant0 = TYPE_MAIN_VARIANT (TREE_TYPE (node));
         tree op0 = TREE_OPERAND (node, 0);
         tree op1 = TREE_OPERAND (node, 1);
         int mode= TYPE_MODE (TREE_TYPE (op0));
         n_debug("mtcsport.c mtcs_copy_tree_body_r 112 TREE_TYPE (op0):%p mode:%d %s mainvariant0:%p op1:%p\n",
               TREE_TYPE (op0),TYPE_MODE (TREE_TYPE (op0)),get_tree_code_name(TREE_CODE(TREE_TYPE (op0))),mainvariant0,op1);

         if(mainvariant0 && op1){
            n_debug("mtcsport.c mtcs_copy_tree_body_r 112aa TREE_TYPE (op0):%p mode:%d %s %s\n",
                     TREE_TYPE (op1),TYPE_MODE (TREE_TYPE (op1)),get_tree_code_name(TREE_CODE(TREE_TYPE (op1)))
                     ,mainvariant0?get_tree_code_name(TREE_CODE(mainvariant0)):"null");
            tree type = TREE_TYPE(TREE_TYPE(op1));
            if(type){
               tree mainvarint1 = TYPE_MAIN_VARIANT(type);
               n_debug("mtcsport.c mtcs_copy_tree_body_r 112bb  mainvarint1:%p %s\n",
                     mainvarint1,get_tree_code_name(TREE_CODE(mainvarint1)));

               if(mainvariant0!=mainvarint1){
                  TYPE_MAIN_VARIANT(type) = mainvariant0;
               }
            }
         }
         n_debug("mtcsport.c mtcs_copy_tree_body_r 113 TREE_TYPE (op0):%p mode:%d\n",TREE_TYPE (op0),TYPE_MODE (TREE_TYPE (op0)));

         //*walk_subtrees = 0;

         tree mainvariant1 = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1))));
         fprintf(stderr,"xxx mainvariant0 mainvariant1 :%p %p %d\n",mainvariant0,mainvariant1,mainvariant0==mainvariant1);
      }
      */

   }

   /* Keep iterating.  */
   return NULL_TREE;
}

/**
 * 加类型是bitsizetype的field到bufferArray中
 */
static void addBitSizeTypeTree(MtcsPort *self,tree t)
{
  tree bitpos=DECL_FIELD_BIT_OFFSET (t);
  if(!bitpos)
     return;
  tree bitpostype = TREE_TYPE (bitpos);
  if(bitpostype!=bitsizetype)
     return;

   int i;
   nboolean find=FALSE;
   for(i=0;i<self->bufferArray->len;i++){
      if(n_ptr_array_index(self->bufferArray,i)==t){
         find=TRUE;
         break;
      }
   }
   if(!find)
      n_ptr_array_add(self->bufferArray,t);
}
/* Remap DECL during the copying of the BLOCK tree for the function.  */
//原型 remap_decl tree-inline.cc
static tree remapDecl (MtcsPort *self,tree decl, copy_body_data *id)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree *n;
   /* We only remap local variables in the current function.  */
   /* See if we have remapped this declaration.  */
   n = id->decl_map->get (decl);
   n_debug("mtcsport.c remapDecl 00  decl:%p %s processing_debug_stmt:%d decl是否有map值:%p\n",
         decl,get_tree_code_name(TREE_CODE(decl)),self->processing_debug_stmt,n);

   if (!n && self->processing_debug_stmt){
      n_debug("mtcsport.c remapDecl 11 直接返回 decl  processing_debug_stmt:%d\n",self->processing_debug_stmt);
      self->processing_debug_stmt = -1;
      return decl;
   }
   /* When remapping a type within copy_gimple_seq_and_replace_locals, all
   necessary DECLs have already been remapped and we do not want to duplicate
   a decl coming from outside of the sequence we are copying.  */
   if (!n
   && id->prevent_decl_creation_for_types
   && id->remapping_type_depth > 0
   && (VAR_P (decl) || TREE_CODE (decl) == PARM_DECL)){
      n_debug("mtcsport.c remapDecl 22 直接返回 decl  processing_debug_stmt:%d\n",self->processing_debug_stmt);
      return decl;
   }

   /* If we didn't already have an equivalent for this declaration, create one
   now.  */
   if (!n){
      /* Make a copy of the variable or label.  */
      tree t = id->copy_decl (decl, id);
      n_debug("mtcsport.c ----remapDecl 00 新:%p 原decl:%p new rtl:%p old rtl:%p %s %s USE:%d\n",
            t,decl,decl->decl_with_rtl.rtl,t->decl_with_rtl.rtl,
            DECL_NAME(decl)?IDENTIFIER_POINTER(DECL_NAME(decl)):"null",
                  get_tree_code_name(TREE_CODE(decl)),TREE_USED(t));
      DECL_ABSTRACT_ORIGIN (t)=NULL_TREE;//原始的声明也会复制到新的声明中，这里设为空。
      /* Remember it, so that if we encounter this local entity again
      we can reuse this copy.  Do this early because remap_type may
      need this decl for TYPE_STUB_DECL.  */
      insert_decl_map (id, decl, t);
      if (!DECL_P (t) || t == decl)
         return t;
      /* Remap types, if necessary.  */
      n_debug("mtcsport.c remapDecl 33 新DECL的类型:%p mode:%d name:%s 原DECL的类型:%p 如果新类型与原类型相同说明类型没有克隆\n",
            TREE_TYPE (t),TYPE_MODE(TREE_TYPE (t)),get_tree_code_name(TREE_CODE(TREE_TYPE (t))),TREE_TYPE (decl));
      TREE_TYPE (t) = remapType(self,TREE_TYPE (t), id);
      n_debug("mtcsport.c remapDecl 44 新decl:%p type:%p mode:%d name:%s 老decl类型的mode:%d\n",
            t,TREE_TYPE (t),TYPE_MODE(TREE_TYPE (t)),get_tree_code_name(TREE_CODE(TREE_TYPE (t))),TYPE_MODE(TREE_TYPE (decl)));

      if (TREE_CODE (t) == TYPE_DECL){
         n_debug("mtcsport.c remapDecl 55 新decl:%p map新类型:%p %s\n",
               t,TREE_TYPE (t),get_tree_code_name(TREE_CODE(TREE_TYPE (t))));

         DECL_ORIGINAL_TYPE (t) = remapType(self,DECL_ORIGINAL_TYPE (t), id);
         /* Preserve the invariant that DECL_ORIGINAL_TYPE != TREE_TYPE,
         which is enforced in gen_typedef_die when DECL_ABSTRACT_ORIGIN
         is not set on the TYPE_DECL, for example in LTO mode.  */
         if (DECL_ORIGINAL_TYPE (t) == TREE_TYPE (t)){
            tree x = build_variant_type_copy (TREE_TYPE (t));
            TYPE_STUB_DECL (x) = TYPE_STUB_DECL (TREE_TYPE (t));
            TYPE_NAME (x) = TYPE_NAME (TREE_TYPE (t));
            DECL_ORIGINAL_TYPE (t) = x;
         }
      }

      /* Remap sizes as necessary.  */
      walk_tree (&DECL_SIZE (t), mtcs_copy_tree_body_r, id, NULL);
      walk_tree (&DECL_SIZE_UNIT (t), mtcs_copy_tree_body_r, id, NULL);

      /* If fields, do likewise for offset and qualifier.  */
      if (TREE_CODE (t) == FIELD_DECL){
         n_debug("mtcsport.c remapDecl 66 decl:%p map 域类型 :%p  %s\n",
               t,TREE_TYPE (t),get_tree_code_name(TREE_CODE(TREE_TYPE (t))));
        // bitsize_unit_node = mtcs_bitsize_unit_node;
        // bitsizetype = mtcs_bitsizetype;
         tree bitpos=DECL_FIELD_BIT_OFFSET (t);
         tree bitpostype = TREE_TYPE (bitpos);
         if(bitpostype==bitsizetype){
            int i;
            for(i=0;i<self->bufferArray->len;i++){
               if(n_ptr_array_index(self->bufferArray,i)==t){
                  break;
               }
            }
         }
        // TREE_TYPE (bitpos)=mtcs_bitsizetype;
         n_debug("mtcsport.c remapDecl 66-- map 域类型 %p typemode:%d %s %p %d\n",
                   bitpostype,TYPE_MODE(bitpostype),get_tree_code_name(TREE_CODE(bitpostype)),bitsizetype,TYPE_MODE(mtcs_bitsizetype));
         tree offset=DECL_FIELD_OFFSET (t);
         tree offsettype = TREE_TYPE (offset);
         aet_print_tree(offset);
         aet_print_tree(offsettype);

         walk_tree (&DECL_FIELD_OFFSET (t), mtcs_copy_tree_body_r, id, NULL);
         //walk_tree (&DECL_FIELD_BIT_OFFSET (t), mtcs_copy_tree_body_r, id, NULL);
         bitpos=DECL_FIELD_BIT_OFFSET (t);
         bitpostype = TREE_TYPE (bitpos);
         addBitSizeTypeTree(self,t);
         n_debug("mtcsport.c remapDecl 66++ map 域类型 %p bitsizetype:%p typemode:%d %s\n",
               bitpostype,bitsizetype,TYPE_MODE(bitpostype),get_tree_code_name(TREE_CODE(bitpostype)));
         n_debug("mtcsport.c remapDecl 66aa map 域类型 field_decl:%p type:%p %s\n",
               t,TREE_TYPE (t),get_tree_code_name(TREE_CODE(TREE_TYPE (t))));
         aet_print_tree(DECL_FIELD_OFFSET (t));
         aet_print_tree(DECL_FIELD_BIT_OFFSET (t));

         if (TREE_CODE (DECL_CONTEXT (t)) == QUAL_UNION_TYPE)
            walk_tree (&DECL_QUALIFIER (t), mtcs_copy_tree_body_r, id, NULL);
      }
      n_debug("mtcsport.c remapDecl 77aa 返回新的decl:%p %s type:%p USED:%d\n",
                    t,get_tree_code_name(TREE_CODE(TREE_TYPE (t))),TREE_TYPE (t),TREE_USED(t));
      return t;
   }
   if (id->do_not_unshare){
      n_debug("mtcsport.c remapDecl 77  decl:%p %s processing_debug_stmt:%d n:%p %p\n",
              decl,get_tree_code_name(TREE_CODE(decl)),self->processing_debug_stmt,n,*n);
      return *n;
   }else{
      n_debug("mtcsport.c remapDecl 88  decl:%p %s processing_debug_stmt:%d n:%p %p\n",
                   decl,get_tree_code_name(TREE_CODE(decl)),self->processing_debug_stmt,n,*n);
      return unshare_expr (*n);
   }
}

static tree remapFunctionType(MtcsPort *self, tree fnType,copy_body_data *id)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree *node;
   node = id->decl_map->get (fnType);
   n_debug("mtcsport.c rmapFunctionSize 00 fntype:%p name:%s 已存在:%p\n",fnType,get_tree_code_name(TREE_CODE(fnType)),node);
   if (node)
       return *node;
   tree retnType=TREE_TYPE(fnType);
   tree newRetnType=remapType(self,retnType,id);
   tree newTypeArgsList = NULL_TREE;
   for (tree al = TYPE_ARG_TYPES (fnType); al; al = TREE_CHAIN (al)){
       tree type = TREE_VALUE (al);
       tree newType=remapType(self,type,id);
       newTypeArgsList = tree_cons (NULL_TREE, newType, newTypeArgsList);
   }
   newTypeArgsList = nreverse (newTypeArgsList);
   tree newFnType =mtcs_tree_build_function_type (mtcsTree,newRetnType, newTypeArgsList);
   n_debug("mtcsport.c rmapFunctionSize 11 创建 funciton_type old:%p new:%p\n",fnType,newFnType);
   insert_decl_map (id, fnType, newFnType);//缓存function_type
   return newFnType;
}

/* Install new lexical TREE_BLOCK underneath 'current_block'.  */
//原型 prepend_lexical_block tree-inline.cc
static void prepend_lexical_block (tree current_block, tree new_block)
{
  BLOCK_CHAIN (new_block) = BLOCK_SUBBLOCKS (current_block);
  BLOCK_SUBBLOCKS (current_block) = new_block;
  BLOCK_SUPERCONTEXT (new_block) = current_block;
}

/* Copy the whole block tree and root it in id->block.  */
//原型 remap_blocks tree-inline.cc
static tree remap_blocks (MtcsPort *self,tree block, copy_body_data *id)
{
  tree t;
  tree new_tree = block;
  if (!block)
    return NULL;

  remap_block (self,&new_tree, id);
  gcc_assert (new_tree != block);
  for (t = BLOCK_SUBBLOCKS (block); t ; t = BLOCK_CHAIN (t))
    prepend_lexical_block (new_tree, remap_blocks (self,t, id));
  /* Blocks are in arbitrary order, but make things slightly prettier and do
     not swap order when producing a copy.  */
  BLOCK_SUBBLOCKS (new_tree) = blocks_nreverse (BLOCK_SUBBLOCKS (new_tree));
  n_debug("mtcsport.c remap_blocks new_tree :%p old:%p\n",new_tree,block);
  return new_tree;
}

static tree replaceFndecl(MtcsPort *self, tree fndecl,copy_body_data *id)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);

   tree saved_parms = DECL_ARGUMENTS (fndecl);
   tree saved_result = DECL_RESULT (fndecl);
   n_debug("mtcsport.c replaceFndecl 00 主机到设备的函数声明 fndecl:%p mode:%d DECL_RESULT:%p DECL_INITIAL:%p rtl:%p\n",
         fndecl,DECL_MODE(fndecl),DECL_RESULT (fndecl),DECL_INITIAL (fndecl),fndecl->decl_with_rtl.rtl);
   tree *n;
   /* We only remap local variables in the current function.  */
   /* See if we have remapped this declaration.  */
   n = id->decl_map->get (fndecl);
   if(n){
      return *n;
   }
   tree newfndecl = NULL_TREE;
   MtcsFuncType mtcsFuncType = mtcs_info_get_func_type(fndecl);

   if (fndecl_built_in_p (fndecl)){
      built_in_function code = DECL_FUNCTION_CODE (fndecl);
      tree decl= builtin_decl_explicit(code);
      n_debug("mtcsport.c replaceFndecl 00aa 主机的内建函数 复制函数声明 :%p %s fncode:%d\n",decl,IDENTIFIER_POINTER(DECL_NAME(decl)),code);
      if(decl==fndecl){
         n_debug("mtcsport.c replaceFndecl 00bb 主机的内建函数:%p %s\n",fndecl,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
         tree mtcsBuiltFn = mtcs_tree_builtin_decl_explicit(mtcsTree,code);
         n_debug("mtcsport.c replaceFndecl 00cc 主机的内建函数:%p %s mtcsBuiltFn mode:%d\n",
               mtcsBuiltFn,IDENTIFIER_POINTER(DECL_NAME(mtcsBuiltFn)),DECL_MODE(mtcsBuiltFn));
         insert_decl_map (id, fndecl, mtcsBuiltFn);
         return mtcsBuiltFn;
      }
      newfndecl = copy_node (fndecl);
      insert_decl_map (id, fndecl, newfndecl);
   }else if(mtcsFuncType!=MTCS_FUNC_NOT){ //如果不进这里 decl的function会变成空的
      n_debug("mtcsport.c replaceFndecl 是一个MTCS函数 不改变函数声明:%p %s\n",fndecl,IDENTIFIER_POINTER(DECL_NAME(fndecl)));
      newfndecl = fndecl;
      insert_decl_map (id, fndecl, newfndecl);
   }else{
      newfndecl = copy_node (fndecl);
      insert_decl_map (id, fndecl, newfndecl);
   }

   tree newParamList =NULL_TREE;
   tree param;
   /* Remap the parameters and result and return them to the caller.  */
   for (param = DECL_ARGUMENTS (fndecl); param; param = DECL_CHAIN (param)){
      n_debug("mtcsport.c replaceFndecl 11 map参数开始 old param fndel mode:%d\n",DECL_MODE(fndecl));
      tree type=TREE_TYPE(param);
      tree copyParam = remapDecl(self,param, id);
      DECL_ARG_TYPE(copyParam)=TREE_TYPE(copyParam);
      n_debug("mtcsport.c replaceFndecl 22 map参数结束 old param fndel mode:%d %p type:%p old:%p\n",
            DECL_MODE(fndecl),copyParam,TREE_TYPE(copyParam),param);
      DECL_ABSTRACT_ORIGIN (copyParam)=NULL_TREE;
      newParamList = chainon (newParamList, copyParam);
      type=TREE_TYPE(copyParam);
   }
   n_debug("mtcsport.c replaceFndecl 33 打印 fndecl:%p\n",fndecl);
   if(DECL_RESULT (fndecl))
      DECL_ABSTRACT_ORIGIN (DECL_RESULT (newfndecl))=NULL_TREE;
   n_debug("mtcsport.c replaceFndecl 44 原返回值被置为NULL 打印 DECL_RESULT (fndecl):%p\n",DECL_RESULT (fndecl));
   aet_print_tree(DECL_RESULT (fndecl));
   tree newResultDecl=NULL_TREE;
   if (DECL_RESULT (fndecl))
      newResultDecl = remapDecl(self,DECL_RESULT (fndecl), id);
   else
      newResultDecl = NULL_TREE;
   n_debug("mtcsport.c replaceFndecl 55 设置了新的返回置 打印 newResultDecl:%p fndecl:%p\n",newResultDecl,fndecl);
   aet_print_tree(DECL_INITIAL (fndecl));
   aet_print_tree(newResultDecl);

   if(newResultDecl)
      DECL_ABSTRACT_ORIGIN (newResultDecl)=NULL_TREE;

   DECL_RESULT (newfndecl)=newResultDecl;//函数返回值声明
   DECL_ARGUMENTS (newfndecl)=newParamList;  //函数参数声明

   tree fnType=TREE_TYPE(fndecl);
   tree newFnType=remapType(self,fnType,id);//remapFunctionType(self,fnType,id);
   n_debug("mtcsport.c replaceFndecl 66 设置了新的函数类型 old:%p new:%p\n",fnType,newFnType);
   TREE_TYPE(newfndecl)=newFnType;
   //替换主机的mode
   int funcitonBoundary=mtcs_func_get_function_boundary(mtcsFunc);
   int align=mtcs_align_get_function_alignment(mtcsAlign,funcitonBoundary);
   SET_DECL_ALIGN (newfndecl, align/*!FUNCTION_ALIGNMENT (FUNCTION_BOUNDARY)*/);
   SET_DECL_MODE (newfndecl, mtcs_mode_get_function_mode/*!FUNCTION_MODE*/(mtcsMode));
   SET_DECL_RTL (newfndecl, 0);//设声明的rtl为空

   DECL_INITIAL (newfndecl) = remap_blocks (self,DECL_INITIAL (fndecl), id);

   tree_function_decl &fd = FUNCTION_DECL_CHECK (newfndecl)->function_decl;
//       fndecl.built_in_class = NOT_BUILT_IN;
//       fndecl.function_code = (BUILTINS_CODE_OFFSET+i);
   n_debug("mtcsport.c replaceFndecl 77 newfndecl:%p fndel:%p %d %d rtl:%p\n",newfndecl, fndecl,
         fd.built_in_class,fd.function_code,newfndecl->decl_with_rtl.rtl);
   aet_print_tree(newfndecl);

//   fprintf(stderr,"主机到设备了函数声明 11 DECL_RESULT (fn)\n");
//   aet_print_tree(DECL_RESULT (fndecl));
//   fprintf(stderr,"主机到设备了函数声明 11 DECL_ARGUMENTS (fn)\n");
//   aet_print_tree(DECL_ARGUMENTS (fndecl));
   //遍历bb块中的gimple中的树的DECL_MODE和TYPE_MODE 并替换
   return newfndecl;

}

static tree copy_tree (MtcsPort *self,tree t,copy_body_data *id)
{
   if(t==NULL_TREE)
      return t;
   walk_tree (&t, mtcs_copy_tree_body_r, id, NULL);
   return t;
}

/*
 * 有一个tree
 * use_optype_d 定义在 tree-ssa-operands.h中 use_operand_p就是ssa_use_operand_t 定义在tree-core.h中
 * ssa_use_operand_t中有一个 tree *成员
struct GTY(()) gimple_statement_with_ops_base : public gimple
{
  struct use_optype_d GTY((skip (""))) *use_ops;
};

/**
 * 以下是 extend gimple
 * replaceBaseOps暂时无用
 */
static void replaceBaseOps(MtcsPort *self,gimple *stmt)
{
   use_optype_p old_ops, ptr, last;
   old_ops = gimple_use_ops (stmt);
   if (old_ops){
       for (ptr = old_ops; ptr->next; ptr = ptr->next){
          tree *var = USE_OP_PTR (ptr)->use;
          n_debug("mtcsport.c replaceBaseOps --- %p tree:%p *tree:%p\n",stmt,*var,var);
       }
   }
}

static void replaceBind(MtcsPort *self,gimple *stmt,copy_body_data *id)
{
    n_debug("mtcsport.c replaceBind 00\n");
    gbind *bindStmt = as_a <gbind *> (stmt);
    tree vars=gimple_bind_vars(bindStmt);
    n_debug("mtcsport.c replaceBind 11 vars:%s\n",get_tree_code_name(TREE_CODE(vars)));
    tree block=gimple_bind_block(bindStmt);
    n_debug("mtcsport.c replaceBind 22 block:%s\n",get_tree_code_name(TREE_CODE(block)));
}

static void replaceEH_MUST_NOT_THROW(MtcsPort *self,gimple *gs)
{
    n_debug("mtcsport.c replaceEH_MUST_NOT_THROW 00\n");
    geh_mnt *ehmntStmt = as_a <geh_mnt *> (gs);
    tree fndecl=gimple_eh_must_not_throw_fndecl(ehmntStmt);
}


static void replaceASSUME(MtcsPort *self,gimple *gs)
{
    n_debug("mtcsport.c replaceASSUME 00\n");
    tree guard=gimple_assume_guard(gs);
}

static void replacePHI(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
    n_debug("mtcsport.c replacePHI 00 \n");
    gphi *phiStmt = as_a <gphi *> (gs);
    tree result=gimple_phi_result (gs);
    if(result){
       tree newResult=copy_tree(self,result,id);
       n_debug("mtcsport.c replacePHI 11 检查新的 newResult--- newLhs:%p type:%p\n",newResult,TREE_TYPE(newResult));
       gimple_phi_set_result(phiStmt,newResult);
       update_stmt_fn (fun,gs);
    }
    const unsigned nums =gimple_phi_num_args (phiStmt);
    for (unsigned i = 0; i < nums; ++i){
       //tree t = gimple_phi_arg_def (phiStmt, i);
       struct phi_arg_d *d= gimple_phi_arg (phiStmt, i);
       if(d->def){
             tree newDef=copy_tree(self,d->def,id);
             n_debug("mtcsport.c replacePHI 22 检查新的 newDef--- newLhs:%p type:%p\n",newDef,TREE_TYPE(newDef));
             d->def = newDef;
             gimple_phi_set_arg(phiStmt,i,d);
             update_stmt_fn (fun,gs);
       }
    }
}

static void replaceCATCH(MtcsPort *self,gimple *gs)
{
   n_debug("mtcsport.c replaceCATCH 00\n");
    gcatch *catchStmt = as_a <gcatch *> (gs);
    tree types=gimple_catch_types (catchStmt);
}

static void replaceEH_FILTER(MtcsPort *self,gimple *gs)
{
   n_debug("mtcsport.c replaceEH_FILTER 00\n");
   geh_filter *ehfilterStmt = as_a <geh_filter *> (gs);
   tree types=gimple_eh_filter_types (ehfilterStmt);
}

/**
 * 以下是 extend gimple_statement_with_ops
 *              extend gimple_statement_with_ops_base
 *                     extend gimple

struct GTY((tag("GSS_WITH_OPS")))  gimple_statement_with_ops : public gimple_statement_with_ops_base
{
  tree GTY((length ("%h.num_ops"))) op[1];
};
*/
static void replaceCOND(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
   replaceBaseOps(self,gs);
   tree lhs=gimple_cond_lhs(gs);
   tree rhs=gimple_cond_rhs(gs);
   n_debug("mtcsport.c replaceCOND 00 \n");
   aet_print_tree(lhs);
   aet_print_tree(rhs);

   const gcond *gc = GIMPLE_CHECK2<const gcond *> (gs);

   if(lhs){
      tree newLhs=copy_tree(self,lhs,id);
      n_debug("mtcsport.c replaceCOND 11 检查新的 lhs--- newLhs:%p type:%p\n",newLhs,TREE_TYPE(newLhs));
      gimple_cond_set_lhs(gc,newLhs);
      update_stmt_fn (fun,gs);
   }

   if(rhs){
      tree newRhs=copy_tree(self,rhs,id);
      n_debug("mtcsport.c replaceCOND 22 检查新的 rhs--- rhs:%p type:%p\n",newRhs,TREE_TYPE(newRhs));
      gimple_cond_set_rhs(gc,newRhs);
      update_stmt_fn (fun,gs);
   }
   tree trueLabel=gimple_cond_true_label(gc);
   if(trueLabel){
      tree newTrueLabel=copy_tree(self,trueLabel,id);
      gimple_cond_set_true_label(gc,newTrueLabel);
      update_stmt_fn (fun,gs);
   }
   tree falseLabel=gimple_cond_false_label(gc);
   if(falseLabel){
      tree newFalseLabel=copy_tree(self,falseLabel,id);
      gimple_cond_set_false_label(gc,newFalseLabel);
      update_stmt_fn (fun,gs);
   }
   n_debug("mtcsport.c replaceCOND 33 trueLabel:%p falseLabel:%p\n",trueLabel,falseLabel);
   aet_print_tree(trueLabel);
   aet_print_tree(falseLabel);
}

static void replaceGOTO(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
    replaceBaseOps(self,gs);
    tree dest=gimple_goto_dest(gs);
    n_debug("mtcsport.c replaceGOTO 00 \n");
    aet_print_tree(dest);
}

/**
 * gs->op[0] 存放的是index
 * gs->op[1] 存放的是缺省label
 */
static void replaceSWITCH(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
    replaceBaseOps(self,gs);
    gswitch *switchStmt = as_a <gswitch *> (gs);
    tree switchIndex=gimple_switch_index (switchStmt);
    if(switchIndex){
       tree newSwitchIndex=copy_tree(self,switchIndex,id);
       gimple_switch_set_index(switchStmt,newSwitchIndex);
       update_stmt_fn (fun,gs);
    }
    //0号就是缺省的
//    tree defaultLabel= gimple_switch_default_label (switchStmt);
//    if(defaultLabel){
//          tree newDefaultLabel=copy_tree(self,defaultLabel,id);
//          gimple_switch_set_default_label(switchStmt,newDefaultLabel);
//          update_stmt_fn (fun,gs);
//    }
    nuint nums=gimple_switch_num_labels (switchStmt);
    n_debug("mtcsport.c replaceSWITCH 00 nums:%d\n",nums);

    int i;
    for (i = 0; i < nums; i++){
        tree caseLabel = gimple_switch_label (switchStmt, i);
        tree label = CASE_LABEL (caseLabel);
        int uid = LABEL_DECL_UID (label);
        n_debug("mtcsport.c replaceSWITCh 11 i:%d uid:%d newlabel:%p cfun->cfg->last_label_uid :%d\n",i,uid,label,cfun->cfg->last_label_uid );
        aet_print_tree(caseLabel);
        aet_print_tree(label);
        tree newCaseLabel=copy_tree(self,caseLabel,id);
        tree newlabel=copy_tree(self,label,id);
        LABEL_DECL_UID (newlabel) = uid;
        CASE_LABEL (newCaseLabel) = newlabel;
        //tree newlabel = CASE_LABEL (caseLabel);
        uid = LABEL_DECL_UID (newlabel);
        n_debug("mtcsport.c replaceSWITCh 22 i:%d uid:%d newlabel:%p cfun->cfg->last_label_uid :%d\n",i,uid,newlabel,cfun->cfg->last_label_uid );
        aet_print_tree(newCaseLabel);
        aet_print_tree(newlabel);
        gimple_switch_set_label(switchStmt,i,newCaseLabel);
        update_stmt_fn (fun,gs);
    }
}

static void replaceDEBUG(MtcsPort *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    printf("replaceDEBUG--- %p\n",gs);

    GIMPLE_CHECK (gs, GIMPLE_DEBUG);
    printf("replaceDEBUG 11 --- %p\n",gs);
    printf("replaceDEBUG 22 --- %d %d\n",is_gimple_debug (gs),gs->subcode);
    printf("replaceDEBUG 22xx --- %d %s\n",is_gimple_debug (gs),gimple_code_name[gs->subcode]);


    gcc_gimple_checking_assert (gimple_debug_bind_p (gs));
    printf("replaceDEBUG 33 --- %p\n",gs);

    tree var= gimple_debug_bind_get_var(gs);
    tree value= gimple_debug_source_bind_get_value (gs);
}

static void replaceLABEL(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
   replaceBaseOps(self,gs);
   glabel *labelStmt = as_a <glabel *> (gs);
   tree llabel= gimple_label_label(labelStmt);
   if(llabel){
      int olduid = LABEL_DECL_UID (llabel);

      tree newLabel=copy_tree(self,llabel,id);
      gimple_label_set_label(labelStmt,newLabel);
      int newuid = LABEL_DECL_UID (newLabel);
      n_debug("mtcsport.c replaceLABEL newLabel:%p %d old:%p %d\n",newLabel,newuid,llabel,olduid);
      aet_print_tree(newLabel);
      update_stmt_fn (fun,gs);
   }
}

/**
 * 以下是 extend gimple_statement_with_memory_ops
 *              extend gimple_statement_with_memory_ops_base
 *                      extend gimple_statement_with_ops_base
 *                             extend gimple
 */
/**
 * GIMPLE_RETURN
 * GIMPLE_ASSIGN:
 * 这两个gimple继承自
 * gimple_statement_with_memory_ops 有一个tree op[1]
 * gimple_statement_with_memory_ops_base有两个 tree vdef vuse
 * gimple_statement_with_memory_ops继承自 gimple_statement_with_memory_ops_base
 * gimple_statement_with_memory_ops_base继承自gimple_statement_with_ops_base
 *
 * struct GTY((tag("GSS_WITH_MEM_OPS")))
  gimple_statement_with_memory_ops :  public gimple_statement_with_memory_ops_base
{
  tree GTY((length ("%h.num_ops"))) op[1];
};

struct GTY((tag("GSS_WITH_MEM_OPS_BASE"))) gimple_statement_with_memory_ops_base : public gimple_statement_with_ops_base
{
  tree GTY((skip (""))) vdef;
  tree GTY((skip (""))) vuse;
};
 */
static void replaceBaseMemory(MtcsPort *self,gimple *stmt)
{
    tree  vdef=gimple_vdef(stmt);
    tree  vuse=gimple_vuse(stmt);
    n_debug("mtcsport.c replaceBaseMemory 00 vdef\n");
    aet_print_tree(vdef);
    n_debug("mtcsport.c replaceBaseMemory 11 vuse\n");
}

static void replaceRETURN(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
   replaceBaseOps(self,gs);
   replaceBaseMemory(self,gs);
   greturn *returnStmt = as_a <greturn *> (gs);
   tree retval= gimple_return_retval (returnStmt);
   n_debug("mtcsport.c replaceRETURN retval fun:%s %p\n",function_name(fun),fun);
   aet_print_tree(retval);
   if(retval!=NULL_TREE){
      tree new_retval=copy_tree(self,retval,id);
      n_debug("mtcsport.c retval new_retval\n");
      aet_print_tree(new_retval);
      gimple_return_set_retval(returnStmt,new_retval);
      update_stmt(gs);
   }
}

static void printfAssign(gimple *gs)
{
   gassign *assignStmt = as_a <gassign *> (gs);
   tree lhs= gimple_assign_lhs (assignStmt);
   tree rhs1=gimple_assign_rhs1 (assignStmt);
   tree rhs2=gimple_assign_rhs2 (assignStmt);
   tree rhs3=gimple_assign_rhs3 (assignStmt);
   n_debug("mtcsport.c printfAssign 00 基本情况\n");
   aet_print_gimple(gs);

   aet_print_tree(lhs);
   aet_print_tree(rhs1);
   aet_print_tree(rhs2);
   int n =gimple_num_ops (gs);
   int i;
   for(i=0;i<n;i++){
      tree op = gimple_op(gs,i);
      n_debug("mtcsport.c printfAssign 11 操作数 i:%d n:%d op:%p %s\n",i,n,op,get_tree_code_name(TREE_CODE(op)));
      if(TREE_CODE(op)==SSA_NAME){
         tree  exp = SSA_NAME_VAR (op);
         n_debug("mtcsport.c printfAssign 22 操作数 i:%d n:%d SSA_NAME_VAR (op):%p %s\n",i,n,exp,
               exp?get_tree_code_name(TREE_CODE(exp)):"NULL");
      }
   }

   use_operand_p use_p ;
   ssa_op_iter iter;
   FOR_EACH_SSA_USE_OPERAND (use_p, gs, iter, SSA_OP_USE){
      tree use = USE_FROM_PTR (use_p);
      gimple *def_stmt;
      n_debug("mtcsport.c printfAssign 33 使用实操作数 use_p:%p use:%p\n",use_p,use);
      aet_print_tree(use);
      def_stmt = get_gimple_for_ssa_name (use);
      if(def_stmt){
         n_debug("mtcsport.c printfAssign 44 使用实操作数 use_p:%p def_stmt:%p\n",use_p,def_stmt);
         aet_print_gimple(def_stmt);
      }else{
         n_debug("mtcsport.c printfAssign 55 使用实操作数 use_p:%p def_stmt是空的\n",use_p);
      }
   }
   tree var;
    FOR_EACH_SSA_TREE_OPERAND (var, gs, iter, SSA_OP_ALL_OPERANDS){
       n_debug("mtcsport.c printAssign 66 要打印语句的所有操作数：var:%p\n",var,get_tree_code_name(TREE_CODE(var)));
       aet_print_tree(var);
    }
}

static void checkLhs(tree node)
{
   if(TREE_CODE(node)!=MEM_REF)
      return;

   n_debug("mtcsport.c checkLhs integer_zerop (TREE_OPERAND (node, 1):%d\n",integer_zerop (TREE_OPERAND (node, 1)));
   n_debug("mtcsport.c checkLhs TREE_CODE (TREE_OPERAND (node, 0)) != INTEGER_CS:%d\n",TREE_CODE (TREE_OPERAND (node, 0)) != INTEGER_CST);
   n_debug("mtcsport.c checkLhs TREE_TYPE (TREE_OPERAND (node, 0)) != NULL_TREE:%d\n",TREE_TYPE (TREE_OPERAND (node, 0)) != NULL_TREE);
   n_debug("mtcsport.c checkLhs TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 0))) == TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1)))):%d\n",
         (TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 0)))  == TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1)))));
   n_debug("mtcsport.c checkLhs (TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 0))) ==TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 1)))):%d\n",
         (TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 0)))  == TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 1)))));
   n_debug("mtcsport.c checkLhs (TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (node, 0)))== TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (node, 1)))):%d\n",
         (TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (node, 0))) == TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (node, 1)))));
   n_debug("mtcsport.c checkLhs (TYPE_MAIN_VARIANT (TREE_TYPE (node))== TYPE_MAIN_VARIANT(TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1))))):%d\n",
         (TYPE_MAIN_VARIANT (TREE_TYPE (node)) == TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1))))));
   n_debug("mtcsport.c checkLhs mode is :node type:%p type op0 %p,%d\n",
         TREE_TYPE (node),TREE_TYPE (TREE_OPERAND (node, 0)),TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 0))));
   tree mainvariant0 = TYPE_MAIN_VARIANT (TREE_TYPE (node));
   tree mainvariant1 = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1))));
   n_debug("mtcsport.c checkLhs mainvariant0 mainvariant1 :%p %p %d\n",mainvariant0,mainvariant1,mainvariant0==mainvariant1);
   aet_print_tree(mainvariant0);
   aet_print_tree(mainvariant1);
}

/**
 * 如果右值有操作数存在 gimple 的 gimple_statement_with_ops_base 变量 use_ops 中
 * gimple_assign_lhs、gimple_assign_rhs1等获取的 tree与  gimple_op(gs,0)、gimple_op(gs,1)..是相同的
 * gimple_op_ptr(gs,0)获取的 tree 指针 *tp 中的 tp[0]就是 gimple_assign_lhs  gimple_op(gs,0)的树是同一个
 * gssign 的操作数类型是 MEM_REF 还有操用数 subop=TREE_OPERAND (newRhs1, 0),subop以指针形式存在 use_ops 中
 * use_ops 是 use_optype_p 也是一个链表 下一个subop存在  use_ops->next的 use中
 */
static void replaceASSIGN(MtcsPort *self,struct function *fun,gimple *gs,copy_body_data *id)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gassign *assignStmt = as_a <gassign *> (gs);
    tree lhs= gimple_assign_lhs (assignStmt);
    tree rhs1=gimple_assign_rhs1 (assignStmt);
    tree rhs2=gimple_assign_rhs2 (assignStmt);
    tree rhs3=gimple_assign_rhs3 (assignStmt);
    n_debug("mtcsport.c replaceASSIGN 00 替换前的 gs fun:%p cfun:%p\n",fun,cfun);
    printfAssign(gs);
    if(lhs){
       checkLhs(lhs);
       tree newLhs=copy_tree(self,lhs,id);
       n_debug("mtcsport.c replaceASSIGN 11 检查新的lhs--- newLhs:%p type:%p\n",newLhs,TREE_TYPE(newLhs));
       checkLhs(newLhs);
       gimple_assign_set_lhs(assignStmt,newLhs);
       update_stmt_fn (fun,gs);
    }

    n_debug("mtcsport.c replaceASSIGN 22 替换lhs后的情况 gs\n");
    printfAssign(gs);

    if(rhs1){
       tree newRhs1=copy_tree(self,rhs1,id);
       gimple_assign_set_rhs1(assignStmt,newRhs1);
       update_stmt_fn (fun,gs);
    }

    if(rhs2){
       tree newRhs2=copy_tree(self,rhs2,id);
       gimple_assign_set_rhs2(assignStmt,newRhs2);
    }
    n_debug("mtcsport.c replaceASSIGN 33 :%p %p %p %p %p\n",gs,lhs,rhs1,rhs2,rhs3);
    verify_ssa_operands(fun,gs);
    if(rhs3){
       tree newRhs3=copy_tree(self,rhs3,id);
       gimple_assign_set_rhs3(assignStmt,newRhs3);
    }
}

/**
 * 以下是 extend gimple_statement_with_memory_ops_base
 *              extend gimple_statement_with_ops_base
 *                     extend gimple
 */
static void replaceASM(MtcsPort *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gasm *asmStmt = as_a <gasm *> (gs);
    const unsigned noutputs = gimple_asm_noutputs (asmStmt);
    const unsigned ninputs = gimple_asm_ninputs (asmStmt);
    const unsigned nclobbers = gimple_asm_nclobbers (asmStmt);
    const unsigned nlabels = gimple_asm_nlabels (asmStmt);
}


static void replaceCALL(MtcsPort *self,gimple *gs,copy_body_data *id)
{
   replaceBaseOps(self,gs);
   replaceBaseMemory(self,gs);
   gcall *callStmt = as_a <gcall *> (gs);
   tree lhs=gimple_call_lhs (callStmt);
   n_debug("mtcsport.c replaceCall 00 lhs :%p \n",lhs);
   if(lhs){
      tree new_lhs=copy_tree(self,lhs,id);
      if(new_lhs && DECL_P (new_lhs)){
         DECL_ABSTRACT_ORIGIN (new_lhs)=NULL_TREE;//清除老的声明
      }
      gimple_call_set_lhs(callStmt,new_lhs);
   }
   tree fntype=gimple_call_fntype (callStmt);
   if(fntype!=NULL_TREE){
      tree new_fntype=copy_tree(self,fntype,id);
      n_debug("mtcsport.c replaceCall 11 new_fntype :%p builtin:%d\n",new_fntype,gimple_call_builtin_p(callStmt));
      gimple_call_set_fntype(callStmt,new_fntype);
   }

   tree fn= gimple_call_fn (callStmt);
   if(fn!=NULL_TREE){
      tree fndecl = gimple_call_fndecl(callStmt);
      tree new_fn=copy_tree(self,fn,id);
      //在这里也改变了 gimple_call_fndecl(callStmt); 中的fndecl 因为引用的也是 gimple_call_fn
      n_debug("mtcsport.c replaceCall 22 new_fn %p %s builtin:%d\n",
            new_fn,fndecl?IDENTIFIER_POINTER(DECL_NAME(fndecl)):"NULL",gimple_call_builtin_p(callStmt));
      gimple_call_set_fn(callStmt,new_fn);
   }

   tree returnType=gimple_call_return_type(callStmt);
   const unsigned argCount = gimple_call_num_args (callStmt);
   for (unsigned i = 0; i < argCount; ++i){
      tree t = gimple_call_arg (callStmt, i);
      n_debug("mtcsport.c replaceCall 33 参数--- t:%p callStmt:%p\n",t);
      tree new_arg=copy_tree(self,t,id);
      gimple_call_set_arg(callStmt,i,new_arg);
   }

}

static void replaceTRANSACTION(MtcsPort *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gtransaction *transactionStmt = as_a <gtransaction *> (gs);
    tree norm=gimple_transaction_label_norm (transactionStmt);
    tree uninst=gimple_transaction_label_uninst (transactionStmt);
    tree over=gimple_transaction_label_over (transactionStmt);
}

static void replaceGimple(MtcsPort *self,struct function *fun,basic_block bb,gimple *stmt,copy_body_data *id)
{
   enum gimple_code code = gimple_code (stmt);
   n_debug("mtcsport.c replaceGimple bb:%p name:%s\n",bb,gimple_code_name[code]);
   switch (code){
      //以下是extend gimple
      case GIMPLE_BIND:
         replaceBind(self,stmt,id);
         return;
      case GIMPLE_EH_MUST_NOT_THROW:
         replaceEH_MUST_NOT_THROW(self,stmt);
         return;
      case GIMPLE_ASSUME:
         replaceASSUME(self,stmt);
         return;
      case GIMPLE_PHI:
         replacePHI(self,fun,stmt,id);
         return;
      case GIMPLE_CATCH:
         replaceCATCH(self,stmt);
         return;
      case GIMPLE_EH_FILTER:
         replaceEH_FILTER(self,stmt);
         return;
      /*以下是
      extend gimple_statement_with_ops
      extend gimple_statement_with_ops_base
      extend gimple
      */
      case GIMPLE_COND:
         replaceCOND(self,fun,stmt,id);
         return;
      case GIMPLE_GOTO:
         replaceGOTO(self,fun,stmt,id);
         return;
      case GIMPLE_SWITCH:
         replaceSWITCH(self,fun,stmt,id);
         return;
      case GIMPLE_DEBUG:
         replaceDEBUG(self,stmt);
         return;
      case GIMPLE_LABEL:
         replaceLABEL(self,fun,stmt,id);
         return;
      /**
      * 以下是 extend gimple_statement_with_memory_ops
      *              extend gimple_statement_with_memory_ops_base
      *                      extend gimple_statement_with_ops_base
      *                             extend gimple
      */
      case GIMPLE_RETURN:
         replaceRETURN(self,fun,stmt,id);
         return;
      case GIMPLE_ASSIGN:
         replaceASSIGN(self,fun,stmt,id);
         return;
      /**
      * 以下是 extend gimple_statement_with_memory_ops_base
      *              extend gimple_statement_with_ops_base
      *                     extend gimple
      */
      case GIMPLE_ASM:
         replaceASM(self,stmt);
         return;

      case GIMPLE_CALL:
         replaceCALL(self,stmt,id);
         return;
      case GIMPLE_TRANSACTION:
         replaceTRANSACTION(self,stmt);
         return;
      //没有tree
      case GIMPLE_TRY:
      //extend gtry expand gimple
      case GIMPLE_EH_ELSE:
      //extend gimple
      case GIMPLE_NOP:
      //extend gimple
      case GIMPLE_PREDICT:
      //extend gimple
      case GIMPLE_ERROR_MARK:
         //extend gimple
         fprintf(stderr,"没有tree GIMPLE_ERROR_MARK code:%d name:%s\n",code,gimple_code_name[code]);
         return;
      case GIMPLE_RESX:
      case GIMPLE_EH_DISPATCH:
         //extend gimple_statement_eh_ctrl extend gimple
         fprintf(stderr,"没有tree GIMPLE_RESX GIMPLE_EH_DISPATCH code:%d name:%s\n",code,gimple_code_name[code]);
         return;
      case GIMPLE_WITH_CLEANUP_EXPR:
         //extend gimple_statement_wce extend gimple
         fprintf(stderr,"没有tree GIMPLE_WITH_CLEANUP_EXPR code:%d name:%s\n",code,gimple_code_name[code]);
         return;
      case GIMPLE_OMP_STRUCTURED_BLOCK:
      case GIMPLE_OMP_FOR:
      case GIMPLE_OMP_SCOPE:
      case GIMPLE_OMP_SECTIONS:
      case GIMPLE_OMP_SECTIONS_SWITCH:
      case GIMPLE_OMP_SECTION:
      case GIMPLE_OMP_SINGLE:
      case GIMPLE_OMP_MASTER:
      case GIMPLE_OMP_MASKED:
      case GIMPLE_OMP_TASKGROUP:
      case GIMPLE_OMP_ORDERED:
      case GIMPLE_OMP_SCAN:
      case GIMPLE_OMP_CRITICAL:
      case GIMPLE_OMP_RETURN:
      case GIMPLE_OMP_ATOMIC_LOAD:
      case GIMPLE_OMP_ATOMIC_STORE:
      case GIMPLE_OMP_CONTINUE:
      case GIMPLE_OMP_PARALLEL:
      case GIMPLE_OMP_TASK:
      case GIMPLE_OMP_TARGET:
      case GIMPLE_OMP_TEAMS:
         fprintf(stderr,"mtcs函数中不应该出现omp相关的代码。\n");
         gcc_unreachable ();
      default:
         fprintf(stderr,"mtcs函数中还没替换的gimple类型。code:%d name:%s\n",code,gimple_code_name[code]);
         gcc_unreachable ();
   }
}

/**
 * 移植主机的选项到mtcs
 * 原型 tree-streamer-in.cc lto_input_ts_function_decl_tree_pointers
 * 要执行创建DECL_FUNCTION_SPECIFIC_OPTIMIZATION的代码，需要 flag_generate_offload 设为1，
 * i编译opendacc或omap时，会设全局参数flag_generate_offload=1,当执行all_small_ipa_passes
 * 中的pass  ipa-free-lang-data.cc free_lang_data 时会为每个函数设编译选项。
 * 具体做法，在cgraphunit.cc中加入代码。
     static void
      ipa_passes (void)
    ....
      nboolean haveMtcsFunc=mtcs_compile_have_mtcs_func(mtcs_compile_get());
      int save= flag_generate_offload ;
      if (haveMtcsFunc && !save){
          fprintf(stderr,"cgraphunit.cc ipa_passes ---11--ww 有核函数 flag_generate_offload:%d\n",flag_generate_offload);
          flag_generate_offload = 1;
      }
      execute_ipa_pass_list (passes->all_small_ipa_passes);
      if(haveMtcsFunc && !save){
          flag_generate_offload=save;
      }
    删除该方法。改为直接在mtcscompile.c中为mtcs函数设主机的optimization_default_node
 */
static void setOptions(MtcsPort *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsOptions  *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOpts  *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
    MtcsOptionsItem *globalOpts=mtcsOptions->global_options;

    struct cgraph_node *cnode;
    FOR_EACH_FUNCTION (cnode){
        tree fndecl=cnode->decl;
        tree opts = DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fndecl);
        n_debug("mtcsport.c setOptions 移植函数的选项 00 :%s opts is:%p\n",cnode->name(),opts);
        if (opts){
            n_debug("mtcsport.c setOptions 移植函数的选项 11 重要的方法 在这里把在c中的opts替换成nvptx的opts %s x_flag_ipa_pta:%d opts:%p\n",
                    IDENTIFIER_POINTER(DECL_NAME(fndecl)),globalOpts->x_flag_ipa_pta,globalOpts);
            MtcsOptionsItem *tmp=mtcs_options_create_item(mtcsOptions);
            MtcsOptionsItem *tmp_set=mtcs_options_create_item(mtcsOptions);
            mtcs_options_init_options_struct/*!init_options_struct*/(mtcsOptions,tmp,NULL);
            // memset (&tmp_set, 0, sizeof (tmp_set));
            mtcs_options_cl_optimization_restore/*!cl_optimization_restore*/(mtcsOptions,tmp, tmp_set, TREE_OPTIMIZATION (opts));
            n_debug("mtcsport.c setOptions 移植函数的选项 22 重要的方法 在这里把在c中的opts替换成nvptx的opts %s x_flag_ipa_pta:%d opts:%p\n",
                    IDENTIFIER_POINTER(DECL_NAME(fndecl)),globalOpts->x_flag_ipa_pta,globalOpts);
            mtcs_opts_finish_options/*!finish_options*/(mtcsOpts,tmp, tmp_set, UNKNOWN_LOCATION);
            n_debug("mtcsport.c setOptions 移植函数的选项 33  重要的方法 在这里把在c中的opts替换成nvptx的opts %s x_flag_ipa_pta:%d opts:%p\n",
                    IDENTIFIER_POINTER(DECL_NAME(fndecl)),globalOpts->x_flag_ipa_pta,globalOpts);
            opts = mtcs_opts_build_optimization_node/*!build_optimization_node*/(mtcsOpts,tmp, tmp_set);
            DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fndecl) = opts;
            mtcs_options_free_item(mtcsOptions,tmp);
            mtcs_options_free_item(mtcsOptions,tmp_set);

        }
    }
}


/* Add local variables from CALLEE to CALLER.  */
//原型 add_local_variables tree-inline.cc
static  void add_local_variables (MtcsPort *self,struct function *fn,  copy_body_data *id)
{
   tree var;
   unsigned ix=0;

   FOR_EACH_LOCAL_DECL (fn, ix, var)
      if (!can_be_nonlocal (var, id)){
         tree new_var = remapDecl(self,var, id);
         /* Remap debug-expressions.  */
         if (VAR_P (new_var)  && DECL_HAS_DEBUG_EXPR_P (var) && new_var != var){
            tree tem = DECL_DEBUG_EXPR (var);
            bool old_regimplify = id->regimplify;
            id->remapping_type_depth++;
            walk_tree (&tem, mtcs_copy_tree_body_r/*!copy_tree_body_r*/, id, NULL);
            id->remapping_type_depth--;
            id->regimplify = old_regimplify;
            SET_DECL_DEBUG_EXPR (new_var, tem);
            DECL_HAS_DEBUG_EXPR_P (new_var) = 1;
         }
         gcc_assert (VAR_P (new_var));
         //add_local_decl (caller, new_var);
         (*fn->local_decls)[ix]=new_var;
      }
}

/**
 * 用 mtcstree 替换原 tree
 * 调用该方法时，符号表已被替换为mtcs符号表。
 * 如果fndecl是内建函数，在create_version_clone_with_body可能没有复制。
 * 判断如果是内建函数，直接替换成mtcs的同个内建函数
 * 注意在ipa的方法 symbol_table::remove_unreachable_nodes (FILE *file)可能移走 fndec
 * l中的fndecl->decl_with_vis.symtab_node
 */
static void portFuncAndBasickBlock(MtcsPort *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   n_ptr_array_remove_range(self->bufferArray,0,self->bufferArray->len);

   copy_body_data id;
   hash_map<tree, tree> decl_map;

   memset (&id, 0, sizeof (id));
   id.decl_map = &decl_map;
   id.copy_decl = mtcs_copy_decl_no_change;
   id.transform_call_graph_edges = CB_CGE_DUPLICATE;
   id.transform_new_cfg = false;
   id.transform_return_to_modify = false;
   id.transform_parameter = true;

   /* Make sure not to unshare trees behind the front-end's back
   since front-end specific mechanisms may rely on sharing.  */
   id.regimplify = false;
   id.do_not_unshare = true;
   id.do_not_fold = true;

   /* We're not inside any EH region.  */
   id.eh_lp_nr = 0;

   insert_decl_map (&id, get_identifier (MTCS_CLONES),
         mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,mtcs_long_long_unsigned_type_node,(unsigned long long)self));
   edge_iterator ei;
   struct cgraph_node *cnode;
   FOR_EACH_FUNCTION (cnode){
      tree fndecl=cnode->decl;
      id.src_fn = fndecl;
      id.dst_fn = fndecl;
      id.src_cfun = DECL_STRUCT_FUNCTION (fndecl);
      n_debug("mtcsport.c portFuncAndBasickBlock 00 %s fndecl:%p %s cnode:%p use:%d decl_preserve_p:%d\n",
            function_name(id.src_cfun ),fndecl,IDENTIFIER_POINTER(DECL_NAME(fndecl)),cnode,TREE_USED (fndecl),DECL_PRESERVE_P (fndecl));
      mtcs_func_push_cfun(mtcsFunc,id.src_cfun);
      aet_print_cgraph_node(cnode);
      n_debug("mtcsport.c portFuncAndBasickBlock 11 替换函数声明:cnode:%p fndec:%p %s node:%p function:%p 本地变量数量:%d\n",
            cnode,fndecl,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fndecl->decl_with_vis.symtab_node,id.src_cfun,
            id.src_cfun?vec_safe_length (id.src_cfun->local_decls):0);
      tree deviceHostAtt = lookup_attribute (MTCS_DEVICE_HOST_STRING, DECL_ATTRIBUTES (fndecl));
      tree deviceAtt = lookup_attribute (MTCS_DEVICE_STRING, DECL_ATTRIBUTES (fndecl));

      n_debug("mtcsport.c portFuncAndBasickBlock 22 deviceHostAtt:%p deviceAtt:%p\n",deviceHostAtt,deviceAtt);
      aet_print_tree(fndecl);

      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,fndecl, NULL);//清除主机中生成的RTL
      tree newdecl= replaceFndecl(self,fndecl,&id);
      n_debug("mtcsport.c portFuncAndBasickBlock 33 fndecl:%p %s cnode:%p use:%d decl_preserve_p:%d\n",
            newdecl,IDENTIFIER_POINTER(DECL_NAME(newdecl)),cnode,TREE_USED (newdecl),DECL_PRESERVE_P (newdecl));
      cnode->decl=newdecl;
      newdecl->decl_with_vis.symtab_node= cnode;
      mtcs_func_pop_cfun(mtcsFunc);
      deviceHostAtt = lookup_attribute (MTCS_DEVICE_HOST_STRING, DECL_ATTRIBUTES (newdecl));
      deviceAtt = lookup_attribute (MTCS_DEVICE_STRING, DECL_ATTRIBUTES (newdecl));
      n_debug("mtcsport.c portFuncAndBasickBlock 44 deviceHostAtt:%p deviceAtt:%p %p 老:fndecl:%p rtl:%p  新的fndecl:%p rtl:%p\n",
            deviceHostAtt,deviceAtt,DECL_STRUCT_FUNCTION (newdecl),fndecl,
            fndecl->decl_with_rtl.rtl,newdecl,newdecl->decl_with_rtl.rtl);

      if(id.src_cfun)
        add_local_variables(self,id.src_cfun,&id);
   }

   FOR_EACH_FUNCTION (cnode){
      tree fndecl=cnode->decl;
      id.src_fn = fndecl;
      id.dst_fn = fndecl;
      id.src_cfun = DECL_STRUCT_FUNCTION (fndecl);
      mtcs_func_push_cfun(mtcsFunc,id.src_cfun);
      basic_block bb;
      struct function *nodeFun=id.src_cfun;
      n_debug("mtcsport.c  portFuncAndBasickBlock 44 替换 %s gimple cnode:%p fndec:%p self:%p function:%p\n",
            function_name(id.src_cfun ),cnode,fndecl,self,nodeFun);

      if(!nodeFun){
         mtcs_func_pop_cfun(mtcsFunc);
         continue;
      }

      if(id.src_cfun){
         int len = vec_safe_length (id.src_cfun->local_decls);
         n_debug("mtcsport.c  portFuncAndBasickBlock 55 替换gimple cnode 本地变量:%d id.src_cfun:%p fndecl:%p\n",len,id.src_cfun,fndecl);
      }

      FOR_EACH_BB_FN (bb, nodeFun){
         gimple_stmt_iterator gsi, seq_gsi;
         for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
            gimple *stmt = gsi_stmt (gsi);
            n_debug("mtcsport.c  portFuncAndBasickBlock 66 替换 %s gimple 开始 stmt:%p\n",function_name(id.src_cfun ),stmt);
            aet_print_gimple(stmt);
            printDef(id.src_cfun);
            replaceGimple(self,nodeFun,bb,stmt,&id);
            n_debug("mtcsport.c  portFuncAndBasickBlock 77 替换 %s gimple 结束 stmt:%p\n",function_name(id.src_cfun ),stmt);
            printDef(id.src_cfun);
            aet_print_gimple(stmt);
            location_t loc = gimple_location (stmt);
            tree oldBlock = LOCATION_BLOCK (loc);
            tree newBlock=DECL_INITIAL (nodeFun->decl);
            n_debug("mtcsport.c  portFuncAndBasickBlock 88 stmt:%p oldBlock:%p newBlock:%p loc:%ld\n",stmt,oldBlock,newBlock,loc);
            //重要
            if(oldBlock && oldBlock!=newBlock){
               location_t newloc=set_block(loc,newBlock);
               tree nn = LOCATION_BLOCK (newloc);
               gimple_set_location(stmt,newloc);
               n_debug("mtcsport.c  portFuncAndBasickBlock  99 stmt ee:%p %p %p nn:%p loc:%ld newloc:%ld ==:%d\n",
                     stmt,oldBlock,newBlock,nn,loc,newloc,newloc==UNKNOWN_LOCATION);
            }
         }

         edge old_edge;

         /* Use the indices from the original blocks to create edges for the
            new ones.  */
         FOR_EACH_EDGE (old_edge, ei, bb->succs){
            if (!(old_edge->flags & EDGE_EH)){
               int flags = old_edge->flags;
               location_t loc = old_edge->goto_locus;
               tree oldBlock = LOCATION_BLOCK (loc);
               tree newBlock=DECL_INITIAL (nodeFun->decl);
               n_debug("mtcsport.c  portFuncAndBasickBlock 100 old_edge:%p %p %p loc:%ld\n",old_edge,oldBlock,newBlock,loc);
               if(oldBlock && oldBlock!=newBlock){
                  location_t newloc=set_block(loc,newBlock);
                  tree nn = LOCATION_BLOCK (newloc);
                  old_edge->goto_locus = newloc;
                  n_debug("mtcsport.c  portFuncAndBasickBlock 101 old_edge ee:%p %p %p nn:%p loc:%ld newloc:%ld ==:%d\n",
                        old_edge,oldBlock,newBlock,nn,loc,newloc,newloc==UNKNOWN_LOCATION);
               }
            }
         }
      } //end  FOR_EACH_BB_FN (bb, nodeFun){

      //原型 mtcs_outof_ssa_expand_phi_nodes mtcsoutofssa.c 匹配 expand_phi的需要
      //result需要reamp吗？，SSA_NAME类型的 tree 需要remap吗？
      FOR_BB_BETWEEN (bb, ENTRY_BLOCK_PTR_FOR_FN (nodeFun)->next_bb,EXIT_BLOCK_PTR_FOR_FN (nodeFun), next_bb){
         if (gimple_seq_empty_p (phi_nodes (bb)))
            continue;
         edge e;
         FOR_EACH_EDGE (e, ei, bb->preds){
            gphi_iterator gsi;
            for (gsi = gsi_start_phis (e->dest); !gsi_end_p (gsi); gsi_next (&gsi)){
               gphi *phi = gsi.phi ();
               tree result = gimple_phi_result (phi);
               tree Ti = PHI_ARG_DEF (phi,e->dest_idx);
               n_debug("mtcsport.c 替换 gimple_phi 00 %p %s\n",result,function_name(nodeFun));
               aet_print_gimple(phi);
               aet_print_tree(result);
               aet_print_tree(Ti);
               if(result){
                  tree newResult=copy_tree(self,result,&id);
                  gimple_phi_set_result(phi,newResult);
                  n_debug("mtcsport.c 替换 gimple_phi 11 %p %s\n",result,function_name(nodeFun));
               }
               /*这段代码会引起进入这里，Ti=size_zero_node(主机),如果只对类型remapType
               * 主机的size_zero_node类型将变成 MTCS 的类型，这是错误的。
               * 原来的处理是这样:
               *  if(TREE_CODE(Ti)!=SSA_NAME){
               *    TREE_TYPE(Ti)=remapType(self,TREE_TYPE(Ti),&id);
               * }else{
               *   tree newResult=copy_tree(self,Ti,&id);
               *   SET_PHI_ARG_DEF(phi,e->dest_idx,newResult);
               * }
               * 现改为不判断 Ti类型 ，全复制。
               __global__ void setdata(float *bias_updates, float *delta, int batch)
               {
               int b;
               for(b = 0; b < batch; ++b){
               bias_updates[b] = delta[b];
               }
               }
               */
               tree newResult=copy_tree(self,Ti,&id);
               SET_PHI_ARG_DEF(phi,e->dest_idx,newResult);
            }
         }
      }

      mtcs_func_pop_cfun(mtcsFunc);
      n_debug("mtcsport.c  portFuncAndBasickBlock 102 完成替换gimple的树:\n");
   }// end    FOR_EACH_FUNCTION (cnode){

   //替换全局变量
   id.src_fn = NULL;
   id.dst_fn = NULL;
   varpool_node *vnode;
   FOR_EACH_VARIABLE (vnode){
      varpool_node *node = vnode->decl->decl_with_vis.symtab_node;
      tree decl = vnode->decl;
      n_debug("mtcsport.c portFuncAndBasickBlock 103aa\n");
      aet_print_tree(decl);
      tree newdecl = remapDecl(self,decl, &id);
      vnode->decl = newdecl;
      vnode->decl->decl_with_vis.symtab_node = node;
      n_debug("mtcsport.c portFuncAndBasickBlock 103 全局变量 vnode:%p var:%p %s node:%p force_output:%d rtl:%p\n",
            vnode,vnode->decl,IDENTIFIER_POINTER(DECL_NAME(vnode->decl)),node,vnode->force_output,newdecl->decl_with_rtl.rtl);
      tree init = DECL_INITIAL(vnode->decl);
      if(init){
         n_debug("mtcsport.c  portFuncAndBasickBlock 104 全局变量 有初始化 decl:%p node:%p\n",vnode->decl,node);
         aet_print_tree(init);
         tree newInit=copy_tree(self,init,&id);
         DECL_INITIAL(vnode->decl) = newInit;
      }
      int newalign = TYPE_ALIGN(TREE_TYPE(newdecl));
      int align = TYPE_ALIGN(TREE_TYPE(decl));
      TREE_USED(vnode->decl) = 1;
      DECL_PRESERVE_P (vnode->decl) = 1; //重要,否则经过gimple后，变量被移走
      n_debug("mtcsport.c portFuncAndBasickBlock 105 全局变量 align %d %d %d %d\n",
            align,newalign,DECL_ALIGN(decl),DECL_ALIGN(newdecl));
   }
   setOptions(self);
   n_debug("mtcsport.cc 移植后的节点:\n");
   FOR_EACH_FUNCTION (cnode){
      aet_print_cgraph_node(cnode);
   }
   remove_unreachble_bb(self);
   n_debug("mtcsport.cc 移走第一个bb后的节点:\n");
   FOR_EACH_FUNCTION (cnode){
      aet_print_cgraph_node(cnode);
   }
}

static void remove_unreachble_bb(MtcsPort *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsCfg *mtcsCfg=mtcs_target_get_cfg(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);

   gcc_assert(mtcs_cfg_context_get_state(mtcsCfgContext)==IR_GIMPLE);
   struct cgraph_node *cnode;
   FOR_EACH_FUNCTION (cnode){
      tree fndecl=cnode->decl;
      struct function *fn = DECL_STRUCT_FUNCTION (fndecl);
      if(!fn || !fn->cfg)
         continue;
      mtcs_func_push_cfun(mtcsFunc,fn);
      basic_block entrybb=fn->cfg->x_entry_block_ptr;
      gcc_assert(single_succ_p(entrybb));
      int oldflags=single_succ_edge(entrybb)->flags;
      n_debug("mtcsport.c remove_unreachble_bb 00 :%s ssa_names:%d\n",cnode->name(),vec_safe_length (fn->gimple_df->ssa_names));
      basic_block bb;
      nboolean remove=FALSE;
      FOR_EACH_BB_FN (bb, fn){
         n_debug("mtcsport.c remove_unreachble_bb 11 :%s %p %p %p\n",cnode->name(),bb,entrybb,bb->prev_bb);
         if(bb->prev_bb==entrybb){
            gimple_seq *seq= bb_seq_addr(bb);
            n_debug("mtcsport.c remove_unreachble_bb 22 :%s %p %p %p seq:%p\n",cnode->name(),bb,entrybb,bb->prev_bb,seq);
            aet_print_seq(*seq);
            if(*seq==NULL){
               n_debug("mtcsport.c remove_unreachble_bb 33 这是一个空的 bb:%p 可以移走。\n",bb);
               mtcs_cfg_context_delete_basic_block(mtcsCfgContext,bb);
               remove=TRUE;
            }
         }
         break;
      }

      if(remove){
         n_debug("mtcsport.c remove_unreachble_bb 44 向进入块的 succs 加入边:%p %d\n",
               entrybb,vec_safe_length (fn->gimple_df->ssa_names));
         gcc_assert(EDGE_COUNT(entrybb->succs)==0);
         //加入succs
         basic_block dest= fn->cfg->x_entry_block_ptr->next_bb;
         edge newe=mtcs_cfg_make_edge (mtcsCfg, entrybb, dest,oldflags);
//         int i=0;
//         FOR_EACH_BB_FN (bb, fn){
//            if(i==2){
//               n_debug("change pos----\n");
//               edge e=EDGE_PRED(bb,0);
//               bb->preds->ordered_remove(0);
//               vec_safe_push(bb->preds,e);
//               break;
//            }
//            i++;
//         }
      }
      n_debug("mtcsport.c remove_unreachble_bb 55 加入边:%p %d fn:%p\n",entrybb,vec_safe_length (fn->gimple_df->ssa_names),fn);
      mtcs_func_pop_cfun(mtcsFunc);
   }
}

/**
 * 把bufferArray中的bitsizetype替换为mtcs的bitsizetype
 */
void  mtcs_port_replace_bitsizetype(MtcsPort *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   int i;
   nboolean find=FALSE;
   for(i=0;i<self->bufferArray->len;i++){
      tree t=n_ptr_array_index(self->bufferArray,i);
      tree bitpos=DECL_FIELD_BIT_OFFSET (t);
      tree bitpostype = TREE_TYPE (bitpos);
      n_debug("mtcsport.c port 替换 bitsizetype %p %p %p mtcs_bitsizetype:%p\n",t,bitpostype,bitsizetype,mtcs_bitsizetype);
      if(bitpostype!=bitsizetype && bitpostype!=mtcs_bitsizetype){
         n_error("mtcsport.c mtcs_port_replace_bitsizetype 报告此错误！\n");
      }
      TREE_TYPE(bitpos)=mtcs_bitsizetype;
   }
}

static void printBB(basic_block bb)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   gimple_stmt_iterator gsi;
   for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
      gimple *stmt = gsi_stmt (gsi);
      fprintf(stderr,"打印 basic_block中的gimple:%p bb:%p\n",stmt,bb);
      aet_print_gimple(stmt);
  }
}

static void printNodeBB(struct cgraph_node *cnode)
{
   tree fndecl=  cnode->decl;
   struct function *nodeFun;
   nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   if(!nodeFun)
      return;
   n_debug("mtcsport.c printNodeBB 00 开始 name:%s nodeFun:%p cfun:%p\n",cnode->name(),nodeFun,cfun);
   basic_block bb;
   FOR_EACH_BB_FN (bb, nodeFun){
      gimple_stmt_iterator gsi, seq_gsi;
      int i=0;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         enum gimple_code code = gimple_code (stmt);
         n_debug("mtcsport.c printNodeBB 11 i:%d bb:%p name:%s stmt:%p\n",i++,bb,cnode->name(),stmt);
         aet_print_gimple(stmt);

      }
   }
   n_debug("mtcsport.c printNodeBB 33 结束 gimple bb:%p name:%s\n",bb,cnode->name());
}

static void printNode()
{
   struct cgraph_node *cnode;
   basic_block bb;
   FOR_EACH_FUNCTION (cnode){
      printNodeBB(cnode);
   }
}

/**
 * 移植函数中的语句，内容主要是mode和tree替换为目标平台。
 */
void mtcs_port_port(MtcsPort *self)
{
   printNode();
   portFuncAndBasickBlock(self);
}

void  mtcs_port_restore_bitsizetype(MtcsPort *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   int i;
   nboolean find=FALSE;
   for(i=0;i<self->bufferArray->len;i++){
      tree t=n_ptr_array_index(self->bufferArray,i);
      tree bitpos=DECL_FIELD_BIT_OFFSET (t);
      tree bitpostype = TREE_TYPE (bitpos);
      n_debug("mtcsport.c port 恢复 bitsizetype %p %p %p mtcs_bitsizetype:%p\n",t,bitpostype,bitsizetype,mtcs_bitsizetype);
      if(bitpostype!=bitsizetype && bitpostype!=mtcs_bitsizetype){
         n_error("mtcsport.c mtcs_port_restore_bitsizetype 报告此错误！\n");
      }
      TREE_TYPE(bitpos)=bitsizetype;
   }
}

/**
 * 如果新克隆的函数中的第一个块的 prev =newfn->cfg->x_entry_block_ptr
 * 并且gimple_seq是空的，删除这个块
 */
static void deleteBasickBlock(struct cgraph_node *newNode)
{
   struct function *nodeFun = DECL_STRUCT_FUNCTION (newNode->decl);
   basic_block entrybb=nodeFun->cfg->x_entry_block_ptr;
   basic_block bb;
   FOR_EACH_BB_FN (bb, nodeFun){
       if(bb->prev_bb==entrybb){
          gimple_seq *seq= bb_seq_addr(bb);
          if(seq==NULL){
             fprintf(stderr,"这是一个空的 bb:%p\n",bb);
             delete_basic_block(bb);
          }
       }
       break;
   }
}

MtcsPort *mtcs_port_new(MtcsMode *mtcsMode)
{
   MtcsPort *self = n_slice_alloc0 (sizeof(MtcsPort));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsPortInit(self);
   return self;
}
