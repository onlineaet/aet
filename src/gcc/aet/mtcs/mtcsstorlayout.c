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
 * base on store-layout.cc
 */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "rtl.h"
#include "tree.h"
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
#include "print-tree.h"
#include "langhooks.h"
#include "tree-inline.h"
#include "dumpfile.h"
#include "gimplify.h"
#include "attribs.h"
#include "debug.h"
#include "calls.h"

#include "mtcstarget.h"
#include "mtcsstorlayout.h"

#include "../aetprinttree.h"
#include "../aetprintgimple.h"

static void  mtcsStorLayoutInit(MtcsStorLayout *self)
{

}

/* Compute TYPE_SIZE and TYPE_ALIGN for TYPE, once it has been laid
   out.  */
//原型 finalize_type_size stor-layout.cc
static void finalize_type_size (MtcsStorLayout *self,tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   /* Normally, use the alignment corresponding to the mode chosen.
   However, where strict alignment is not required, avoid
   over-aligning structures, since most compilers do not do this
   alignment.  */
   bool tua_cleared_p = false;
   if (TYPE_MODE (type) != mtcsMode->modes.M_BLKmode
   && TYPE_MODE (type) != VOIDmode
   && (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign) || !AGGREGATE_TYPE_P (type))){
      unsigned mode_align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,TYPE_MODE (type));

      /* Don't override a larger alignment requirement coming from a user
      alignment of one of the fields.  */
      if (mode_align >= TYPE_ALIGN (type)){
         SET_TYPE_ALIGN (type, mode_align);
         /* Remember that we're about to reset this flag.  */
         tua_cleared_p = TYPE_USER_ALIGN (type);
         TYPE_USER_ALIGN (type) = false;
      }
   }

   /* Do machine-dependent extra alignment.  */
#ifdef ROUND_TYPE_ALIGN //host=0 nvptx=0
   SET_TYPE_ALIGN (type,
   ROUND_TYPE_ALIGN (type, TYPE_ALIGN (type), BITS_PER_UNIT));
#endif

   /* If we failed to find a simple way to calculate the unit size
   of the type, find it by division.  */
   if (TYPE_SIZE_UNIT (type) == 0 && TYPE_SIZE (type) != 0)
      /* TYPE_SIZE (type) is computed in bitsizetype.  After the division, the
      result will fit in sizetype.  We will get more efficient code using
      sizetype, so we force a conversion.  */
      TYPE_SIZE_UNIT (type) = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,
            sizetype, mtcs_const_size_binop/*!size_binop*/(mtcsConst,
            FLOOR_DIV_EXPR, TYPE_SIZE (type),bitsize_unit_node));

   if (TYPE_SIZE (type) != 0){
      TYPE_SIZE (type) = round_up (TYPE_SIZE (type), TYPE_ALIGN (type));
      TYPE_SIZE_UNIT (type) = round_up (TYPE_SIZE_UNIT (type), TYPE_ALIGN_UNIT (type));
   }

   /* Evaluate nonconstant sizes only once, either now or as soon as safe.  */
   if (TYPE_SIZE (type) != 0 && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
      TYPE_SIZE (type) = mtcs_stor_layout_variable_size/*!variable_size*/(self,TYPE_SIZE (type));
   if (TYPE_SIZE_UNIT (type) != 0  && TREE_CODE (TYPE_SIZE_UNIT (type)) != INTEGER_CST)
      TYPE_SIZE_UNIT (type) =  mtcs_stor_layout_variable_size/*!variable_size*/(self,TYPE_SIZE_UNIT (type));

   /* Handle empty records as per the x86-64 psABI.  */
   TYPE_EMPTY_P (type) = target_calls_empty_record_p/*!targetm.calls.empty_record_p*/(mtcsMachine->calls,type);

   /* Also layout any other variants of the type.  */
   if (TYPE_NEXT_VARIANT (type)  || type != TYPE_MAIN_VARIANT (type)){
      tree variant;
      /* Record layout info of this variant.  */
      tree size = TYPE_SIZE (type);
      tree size_unit = TYPE_SIZE_UNIT (type);
      unsigned int align = TYPE_ALIGN (type);
      unsigned int precision = TYPE_PRECISION (type);
      unsigned int user_align = TYPE_USER_ALIGN (type);
      machine_mode mode = TYPE_MODE (type);
      bool empty_p = TYPE_EMPTY_P (type);
      bool typeless = AGGREGATE_TYPE_P (type) && TYPE_TYPELESS_STORAGE (type);

      /* Copy it into all variants.  */
      for (variant = TYPE_MAIN_VARIANT (type);  variant != NULL_TREE; variant = TYPE_NEXT_VARIANT (variant)){
         TYPE_SIZE (variant) = size;
         TYPE_SIZE_UNIT (variant) = size_unit;
         unsigned valign = align;
         if (TYPE_USER_ALIGN (variant)){
            valign = MAX (valign, TYPE_ALIGN (variant));
            /* If we reset TYPE_USER_ALIGN on the main variant, we might
            need to reset it on the variants too.  TYPE_MODE will be set
            to MODE in this variant, so we can use that.  */
            if (tua_cleared_p && mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode) >= valign)
               TYPE_USER_ALIGN (variant) = false;
         }else
            TYPE_USER_ALIGN (variant) = user_align;
         SET_TYPE_ALIGN (variant, valign);
         TYPE_PRECISION (variant) = precision;
         SET_TYPE_MODE (variant, mode);
         fprintf(stderr,"mtcsstorlayout.c finalize_type_size 00 mode:%d\n",mode);
         TYPE_EMPTY_P (variant) = empty_p;
         if (AGGREGATE_TYPE_P (variant))
            TYPE_TYPELESS_STORAGE (variant) = typeless;
      }
   }
}


/* Return true if T is a self-referential component reference.  */
//原型 self_referential_component_ref_p stor-layout.cc
static bool self_referential_component_ref_p (tree t)
{
   if (TREE_CODE (t) != COMPONENT_REF)
      return false;

   while (REFERENCE_CLASS_P (t))
      t = TREE_OPERAND (t, 0);

   return (TREE_CODE (t) == PLACEHOLDER_EXPR);
}

/* Similar to copy_tree_r but do not copy component references involving
   PLACEHOLDER_EXPRs.  These nodes are spotted in find_placeholder_in_expr
   and substituted in substitute_in_expr.  */

static tree copy_self_referential_tree_r (tree *tp, int *walk_subtrees, void *data)
{
   enum tree_code code = TREE_CODE (*tp);

   /* Stop at types, decls, constants like copy_tree_r.  */
   if (TREE_CODE_CLASS (code) == tcc_type
   || TREE_CODE_CLASS (code) == tcc_declaration
   || TREE_CODE_CLASS (code) == tcc_constant){
      *walk_subtrees = 0;
      return NULL_TREE;
   }
   /* This is the pattern built in ada/make_aligning_type.  */
   else if (code == ADDR_EXPR && TREE_CODE (TREE_OPERAND (*tp, 0)) == PLACEHOLDER_EXPR){
      *walk_subtrees = 0;
      return NULL_TREE;
   }
   /* Default case: the component reference.  */
   else if (self_referential_component_ref_p (*tp)){
      *walk_subtrees = 0;
      return NULL_TREE;
   }

   /* We're not supposed to have them in self-referential size trees
   because we wouldn't properly control when they are evaluated.
   However, not creating superfluous SAVE_EXPRs requires accurate
   tracking of readonly-ness all the way down to here, which we
   cannot always guarantee in practice.  So punt in this case.  */
   else if (code == SAVE_EXPR)
      return error_mark_node;
   else if (code == STATEMENT_LIST)
      gcc_unreachable ();

   return copy_tree_r (tp, walk_subtrees, data);
}




/* Given a SIZE expression that is self-referential, return an equivalent
   expression to serve as the actual size expression for a type.  */
//原型 self_referential_size stor-layout.cc
static tree self_referential_size (MtcsStorLayout *self,tree size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);

   static unsigned HOST_WIDE_INT fnno = 0;
   vec<tree> self_refs = vNULL;
   tree param_type_list = NULL, param_decl_list = NULL;
   tree t, ref, return_type, fntype, fnname, fndecl;
   unsigned int i;
   char buf[128];
   vec<tree, va_gc> *args = NULL;

   /* Do not factor out simple operations.  */
   t = skip_simple_constant_arithmetic (size);
   if (TREE_CODE (t) == CALL_EXPR || self_referential_component_ref_p (t))
      return size;

   /* Collect the list of self-references in the expression.  */
   find_placeholder_in_expr (size, &self_refs);
   gcc_assert (self_refs.length () > 0);

   /* Obtain a private copy of the expression.  */
   t = size;
   if (walk_tree (&t, copy_self_referential_tree_r, NULL, NULL) != NULL_TREE)
      return size;
   size = t;

   /* Build the parameter and argument lists in parallel; also
   substitute the former for the latter in the expression.  */
   vec_alloc (args, self_refs.length ());
   FOR_EACH_VEC_ELT (self_refs, i, ref){
      tree subst, param_name, param_type, param_decl;

      if (DECL_P (ref)){
         /* We shouldn't have true variables here.  */
         gcc_assert (TREE_READONLY (ref));
         subst = ref;
      }
      /* This is the pattern built in ada/make_aligning_type.  */
      else if (TREE_CODE (ref) == ADDR_EXPR)
         subst = ref;
      /* Default case: the component reference.  */
      else
         subst = TREE_OPERAND (ref, 1);

      sprintf (buf, "p%d", i);
      param_name = get_identifier (buf);
      param_type = TREE_TYPE (ref);
      param_decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,input_location, PARM_DECL, param_name, param_type);
      DECL_ARG_TYPE (param_decl) = param_type;
      DECL_ARTIFICIAL (param_decl) = 1;
      TREE_READONLY (param_decl) = 1;

      size = substitute_in_expr (size, subst, param_decl);

      param_type_list = tree_cons (NULL_TREE, param_type, param_type_list);
      param_decl_list = chainon (param_decl, param_decl_list);
      args->quick_push (ref);
   }

   self_refs.release ();
   /* Append 'void' to indicate that the number of parameters is fixed.  */
   param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);
    /* The 3 lists have been created in reverse order.  */
   param_type_list = nreverse (param_type_list);
   param_decl_list = nreverse (param_decl_list);
    /* Build the function type.  */
   return_type = TREE_TYPE (size);
   fntype = build_function_type (return_type, param_type_list);
    /* Build the function declaration.  */
   sprintf (buf, "SZ" HOST_WIDE_INT_PRINT_UNSIGNED, fnno++);
   fnname = get_file_function_name (buf);
   fndecl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,input_location, FUNCTION_DECL, fnname, fntype);
   for (t = param_decl_list; t; t = DECL_CHAIN (t))
      DECL_CONTEXT (t) = fndecl;
   DECL_ARGUMENTS (fndecl) = param_decl_list;
   DECL_RESULT (fndecl)  = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,input_location, RESULT_DECL, 0, return_type);
   DECL_CONTEXT (DECL_RESULT (fndecl)) = fndecl;

   /* The function has been created by the compiler and we don't
   want to emit debug info for it.  */
   DECL_ARTIFICIAL (fndecl) = 1;
   DECL_IGNORED_P (fndecl) = 1;

   /* It is supposed to be "const" and never throw.  */
   TREE_READONLY (fndecl) = 1;
   TREE_NOTHROW (fndecl) = 1;

   /* We want it to be inlined when this is deemed profitable, as
   well as discarded if every call has been integrated.  */
   DECL_DECLARED_INLINE_P (fndecl) = 1;

   /* It is made up of a unique return statement.  */
   DECL_INITIAL (fndecl) = make_node (BLOCK);
   BLOCK_SUPERCONTEXT (DECL_INITIAL (fndecl)) = fndecl;
   t = build2 (MODIFY_EXPR, return_type, DECL_RESULT (fndecl), size);
   DECL_SAVED_TREE (fndecl) = build1 (RETURN_EXPR, void_type_node, t);
   TREE_STATIC (fndecl) = 1;
   /* Put it onto the list of size functions.  */
   vec_safe_push (self->size_functions, fndecl);
   /* Replace the original expression with a call to the size function.  */
   return build_call_expr_loc_vec (UNKNOWN_LOCATION, fndecl, args);
}


/* Similar, except passed a tree node.  */
//原型 mode_for_size_tree stor-layout.h stor-layout.cc
opt_machine_mode mtcs_stor_layout_mode_for_size_tree (MtcsStorLayout *self,const_tree size, enum mode_class mclass, int limit)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   unsigned HOST_WIDE_INT uhwi;
   unsigned int ui;

   if (!tree_fits_uhwi_p (size))
      return opt_machine_mode ();
   uhwi = tree_to_uhwi (size);
   ui = uhwi;
   if (uhwi != ui)
      return opt_machine_mode ();
   return mtcs_mode_mode_for_size/*!mode_for_size*/(mtcsMode,ui, mclass, limit);
}

/* Return the natural mode of an array, given that it is SIZE bytes in
   total and has elements of type ELEM_TYPE.  */
//原型 mode_for_array stor-layout.h stor-layout.cc
static machine_mode mode_for_array (MtcsStorLayout *self,tree elem_type, tree size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   tree elem_size;
   poly_uint64 int_size, int_elem_size;
   unsigned HOST_WIDE_INT num_elems;
   bool limit_p;

   /* One-element arrays get the component type's mode.  */
   elem_size = TYPE_SIZE (elem_type);
   if (simple_cst_equal (size, elem_size))
      return TYPE_MODE (elem_type);

   limit_p = true;
   if (poly_int_tree_p (size, &int_size)
   && poly_int_tree_p (elem_size, &int_elem_size)
   && maybe_ne (int_elem_size, 0U)
   && constant_multiple_p (int_size, int_elem_size, &num_elems)){
      machine_mode elem_mode = TYPE_MODE (elem_type);
      machine_mode mode;
      if (mtcsTarget/*!targetm.array_mode*/->array_mode(mtcsTarget,elem_mode, num_elems).exists (&mode))
         return mode;
      if (mtcsTarget/*!targetm.array_mode_supported_p*/->array_mode_supported_p(mtcsTarget,elem_mode, num_elems))
         limit_p = false;
   }
   return mtcs_stor_layout_mode_for_size_tree/*!mode_for_size_tree*/(self,size, MODE_INT, limit_p).else_blk ();
}


/* Fold sizetype value X to bitsizetype, given that X represents a type
   size or offset.  */
//原型 bits_from_bytes stor-layout.h stor-layout.cc

static tree bits_from_bytes (MtcsStorLayout *self,tree x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst = mtcs_target_get_const(mtcsTarget);

  if (POLY_INT_CST_P (x))
    /* The runtime calculation isn't allowed to overflow sizetype;
       increasing the runtime values must always increase the size
       or offset of the object.  This means that the object imposes
       a maximum value on the runtime parameters, but we don't record
       what that is.  */
    return build_poly_int_cst
      (bitsizetype,
       poly_wide_int::from (poly_int_cst_value (x),
             TYPE_PRECISION (bitsizetype),
             TYPE_SIGN (TREE_TYPE (x))));
  x = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,bitsizetype, x);
  gcc_checking_assert (x);
  return x;
}

/* Calculate the mode, size, and alignment for TYPE.
   For an array type, calculate the element separation as well.
   Record TYPE on the chain of permanent or temporary types
   so that dbxout will find out about it.

   TYPE_SIZE of a type is nonzero if the type has been laid out already.
   layout_type does nothing on such a type.

   If the type is incomplete, its TYPE_SIZE remains zero.  */
//原型 layout_type stor-layout.h stor-layout.cc
void mtcs_stor_layout_layout_type (MtcsStorLayout *self ,tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   gcc_assert (type);
   if (type == mtcs_error_mark_node)
      return;
   /* We don't want finalize_type_size to copy an alignment attribute to
   variants that don't have it.  */
  // fprintf(stderr,"mtcs_stor_layout_layout_type 00 type:%p\n",type);
   type = TYPE_MAIN_VARIANT (type);
  // fprintf(stderr,"mtcs_stor_layout_layout_type 11 type:%p TYPE_SIZE (type):%p\n",type,TYPE_SIZE (type));
   /* Do nothing if type has been laid out before.  */
   if (TYPE_SIZE (type))
      return;

   switch (TREE_CODE (type)){
      case LANG_TYPE:
         /* This kind of type is the responsibility
         of the language-specific code.  */
         gcc_unreachable ();

      case BOOLEAN_TYPE:
      case INTEGER_TYPE:
      case ENUMERAL_TYPE:
      {
         scalar_int_mode mode = mtcs_mode_smallest_int_mode_for_size/*!smallest_int_mode_for_size*/(mtcsMode,
               TYPE_PRECISION (type));
         SET_TYPE_MODE (type, mode);
        // fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 00 mode:%d\n",mode);
         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode));
         /* Don't set TYPE_PRECISION here, as it may be set by a bitfield.  */
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
         break;
      }

      case BITINT_TYPE:
      {
         struct bitint_info info;
         int cnt;
         bool ok =target_c_bitint_type_info/*!targetm.c.bitint_type_info*/(mtcsMachine->c,TYPE_PRECISION (type), &info);
         gcc_assert (ok);
         scalar_int_mode limb_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,info.abi_limb_mode);
        // fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 11 mode:%d\n",limb_mode);

         if (TYPE_PRECISION (type) <= mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode)){
            SET_TYPE_MODE (type, limb_mode);
            gcc_assert (info.abi_limb_mode == info.limb_mode);
            cnt = 1;
         }else{
            SET_TYPE_MODE (type, mtcsMode->modes.M_BLKmode);
            cnt = CEIL (TYPE_PRECISION (type), mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode));
            gcc_assert (info.abi_limb_mode == info.limb_mode || !info.big_endian == !WORDS_BIG_ENDIAN);
         }
         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               cnt * mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,limb_mode));
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               cnt * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,limb_mode));
         SET_TYPE_ALIGN (type, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,limb_mode));
         if (cnt > 1){
            /* Use same mode as compute_record_mode would use for a structure
            containing cnt limb_mode elements.  */
            machine_mode mode = mtcs_stor_layout_mode_for_size_tree/*!mode_for_size_tree*/(self,TYPE_SIZE (type),
                     MODE_INT, 1).else_blk ();
            if (mode == mtcsMode->modes.M_BLKmode)
               break;
          //  fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 22 mode:%d\n",mode);

            finalize_type_size(self,type);
            SET_TYPE_MODE (type, mode);
            if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
                  && !(TYPE_ALIGN (type) >= mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)
            || TYPE_ALIGN (type) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))){
               /* If this is the only reason this type is BLKmode, then
               don't force containing types to be BLKmode.  */
               TYPE_NO_FORCE_BLK (type) = 1;
               SET_TYPE_MODE (type, mtcsMode->modes.M_BLKmode);
            }

            if (TYPE_NEXT_VARIANT (type) || type != TYPE_MAIN_VARIANT (type))
               for (tree variant = TYPE_MAIN_VARIANT (type); variant != NULL_TREE; variant = TYPE_NEXT_VARIANT (variant)){
                  SET_TYPE_MODE (variant, mode);
                  if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
                  && !(TYPE_ALIGN (variant) >= mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)
                  || (TYPE_ALIGN (variant)  >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)))){
                     TYPE_NO_FORCE_BLK (variant) = 1;
                     SET_TYPE_MODE (variant, mtcsMode->modes.M_BLKmode);
                  }
               }
            return;
         }
         break;
      }

      case REAL_TYPE:
      {
         /* Allow the caller to choose the type mode, which is how decimal
         floats are distinguished from binary ones.  */
         if (TYPE_MODE (type) == VOIDmode){
            machine_mode mode= mtcs_mode_float_mode_for_size/*!float_mode_for_size*/(mtcsMode,
                  TYPE_PRECISION (type)).require ();
            //fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 33 mode:%d\n",mode);
            SET_TYPE_MODE(type, mode);
         }
         scalar_float_mode mode = mtcs_mode_as_a <scalar_float_mode>(mtcsMode,TYPE_MODE (type));
         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode));
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
         break;
      }

      case FIXED_POINT_TYPE:
      {
         /* TYPE_MODE (type) has been set already.  */
         scalar_mode mode = mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,type);
         //fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 44 mode:%d\n",mode);
         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode));
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
         break;
      }

      case COMPLEX_TYPE:
         TYPE_UNSIGNED (type) = TYPE_UNSIGNED (TREE_TYPE (type));
         if (TYPE_MODE (TREE_TYPE (type)) == mtcsMode->modes.M_BLKmode){
            gcc_checking_assert (TREE_CODE (TREE_TYPE (type)) == BITINT_TYPE);
            //fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 55 mode:%d\n",mtcsMode->modes.M_BLKmode);

            SET_TYPE_MODE (type, mtcsMode->modes.M_BLKmode);
            TYPE_SIZE (type) = mtcs_const_int_const_binop/*!int_const_binop*/(mtcsConst,MULT_EXPR, TYPE_SIZE (TREE_TYPE (type)),
                  mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,2));
            TYPE_SIZE_UNIT (type) = mtcs_const_int_const_binop/*!int_const_binop*/(mtcsConst,MULT_EXPR, TYPE_SIZE_UNIT (TREE_TYPE (type)),
                  mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,2));
            break;
         }
         SET_TYPE_MODE (type, mtcs_mode_get_complex/*!GET_MODE_COMPLEX_MODE*/(mtcsMode,TYPE_MODE (TREE_TYPE (type))));

         TYPE_SIZE (type) =mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,TYPE_MODE (type)));
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,TYPE_MODE (type)));
         break;

      case VECTOR_TYPE:
      {
         poly_uint64 nunits = TYPE_VECTOR_SUBPARTS (type);
         tree innertype = TREE_TYPE (type);

         /* Find an appropriate mode for the vector type.  */
         if (TYPE_MODE (type) == VOIDmode){

            machine_mode mode=mtcs_mode_mode_for_vector/*!mode_for_vector*/(mtcsMode,
                  mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,innertype), nunits).else_blk ();
           // fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 66 mode:%d\n",mode);

            SET_TYPE_MODE (type, mode);
         }

         TYPE_SATURATING (type) = TYPE_SATURATING (TREE_TYPE (type));
         TYPE_UNSIGNED (type) = TYPE_UNSIGNED (TREE_TYPE (type));
         /* Several boolean vector elements may fit in a single unit.  */
         if (VECTOR_BOOLEAN_TYPE_P (type) && type->type_common.mode != mtcsMode->modes.M_BLKmode)
            TYPE_SIZE_UNIT (type)= mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
                  mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,type->type_common.mode));
         else
            TYPE_SIZE_UNIT (type) = mtcs_const_int_const_binop/*!int_const_binop*/(mtcsConst,MULT_EXPR, TYPE_SIZE_UNIT (innertype),
                  mtcs_tree_get_size_int/*!size_int*/(mtcsTree,nunits));
         TYPE_SIZE (type) = mtcs_const_int_const_binop/*!int_const_binop*/(mtcsConst,
               MULT_EXPR, bits_from_bytes(self,TYPE_SIZE_UNIT (type)),
               mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,BITS_PER_UNIT));

         /* For vector types, we do not default to the mode's alignment.
         Instead, query a target hook, defaulting to natural alignment.
         This prevents ABI changes depending on whether or not native
         vector modes are supported.  */
         SET_TYPE_ALIGN (type, mtcsTarget/*!targetm.vector_alignment*/->vector_alignment(mtcsTarget,type));
         /* However, if the underlying mode requires a bigger alignment than
         what the target hook provides, we cannot use the mode.  For now,
         simply reject that case.  */
         gcc_assert (TYPE_ALIGN (type) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,TYPE_MODE (type)));
         break;
      }

      case VOID_TYPE:
         /* This is an incomplete type and so doesn't have a size.  */
         SET_TYPE_ALIGN (type, 1);
         TYPE_USER_ALIGN (type) = 0;
         SET_TYPE_MODE (type, VOIDmode);
         break;

      case OFFSET_TYPE:
         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               mtcs_tree_get_pointer_size/*!POINTER_SIZE*/(mtcsTree));
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               mtcs_tree_get_pointer_size_units/*!POINTER_SIZE_UNITS*/(mtcsTree));
         /* A pointer might be MODE_PARTIAL_INT, but ptrdiff_t must be
         integral, which may be an __intN.  */
         fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 77 mode:%d\n",
               mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,
               mtcs_tree_get_pointer_size/*!POINTER_SIZE*/(mtcsTree), 0).require ());

         SET_TYPE_MODE (type, mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,
               mtcs_tree_get_pointer_size/*!POINTER_SIZE*/(mtcsTree), 0).require ());
         TYPE_PRECISION (type) = mtcs_tree_get_pointer_size/*!POINTER_SIZE*/(mtcsTree);
         break;

      case FUNCTION_TYPE:
      case METHOD_TYPE:
      {
         /* It's hard to see what the mode and size of a function ought to
         be, but we do know the alignment is FUNCTION_BOUNDARY, so
         make it consistent with that.  */
         int functionBoundary=mtcs_func_get_function_boundary(mtcsFunc);
         machine_mode mode=mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,
               functionBoundary/*!FUNCTION_BOUNDARY*/, 0).else_blk ();
        // fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 88 mode:%d functionBoundary:%d\n",mode,functionBoundary);

         SET_TYPE_MODE (type,mode);
         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,functionBoundary/*!FUNCTION_BOUNDARY*/);
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,functionBoundary/*!FUNCTION_BOUNDARY*/ / BITS_PER_UNIT);
         break;
      }
      case POINTER_TYPE:
      case REFERENCE_TYPE:
      {
         scalar_int_mode mode = mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,type);
         //fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 99 mode:%d\n",mode);

         TYPE_SIZE (type) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,
               mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode));
         TYPE_SIZE_UNIT (type) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,
               mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode));
         TYPE_UNSIGNED (type) = 1;
         TYPE_PRECISION (type) = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode);
      }
      break;

      case ARRAY_TYPE:
      {
         tree index = TYPE_DOMAIN (type);
         tree element = TREE_TYPE (type);

         /* We need to know both bounds in order to compute the size.  */
         if (index && TYPE_MAX_VALUE (index) && TYPE_MIN_VALUE (index)  && TYPE_SIZE (element)){
            tree ub = TYPE_MAX_VALUE (index);
            tree lb = TYPE_MIN_VALUE (index);
            tree element_size = TYPE_SIZE (element);
            tree length;

            /* Make sure that an array of zero-sized element is zero-sized
            regardless of its extent.  */
            if (integer_zerop (element_size))
               length = size_zero_node;

            /* The computation should happen in the original signedness so
            that (possible) negative values are handled appropriately
            when determining overflow.  */
            else{
               /* ???  When it is obvious that the range is signed
               represent it using ssizetype.  */
               if (TREE_CODE (lb) == INTEGER_CST
               && TREE_CODE (ub) == INTEGER_CST
               && TYPE_UNSIGNED (TREE_TYPE (lb))
               && tree_int_cst_lt (ub, lb)){
                  lb = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,ssizetype,offset_int::from (wi::to_wide (lb),SIGNED));
                  ub = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,ssizetype,offset_int::from (wi::to_wide (ub),SIGNED));
               }
               length = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,
                     sizetype, mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR,
                     mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,TREE_TYPE (lb), 1),
                     mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, ub, lb)));
            }

            /* ??? We have no way to distinguish a null-sized array from an
            array spanning the whole sizetype range, so we arbitrarily
            decide that [0, -1] is the only valid representation.  */
            if (integer_zerop (length)  && TREE_OVERFLOW (length)  && integer_zerop (lb))
               length = size_zero_node;

            TYPE_SIZE (type) = mtcs_const_size_binop/*!size_binop*/(mtcsConst,
                  MULT_EXPR, element_size, bits_from_bytes(self,length));

            /* If we know the size of the element, calculate the total size
            directly, rather than do some division thing below.  This
            optimization helps Fortran assumed-size arrays (where the
            size of the array is determined at runtime) substantially.  */
            if (TYPE_SIZE_UNIT (element))
               TYPE_SIZE_UNIT (type)  = mtcs_const_size_binop/*!size_binop*/(mtcsConst,
                     MULT_EXPR, TYPE_SIZE_UNIT (element), length);
         }

         /* Now round the alignment and size,
         using machine-dependent criteria if any.  */

         unsigned align = TYPE_ALIGN (element);
         if (TYPE_USER_ALIGN (type))
            align = MAX (align, TYPE_ALIGN (type));
         else
            TYPE_USER_ALIGN (type) = TYPE_USER_ALIGN (element);
         if (!TYPE_WARN_IF_NOT_ALIGN (type))
            SET_TYPE_WARN_IF_NOT_ALIGN (type, TYPE_WARN_IF_NOT_ALIGN (element));
   #ifdef ROUND_TYPE_ALIGN //host=0 nvptx=0
         align = ROUND_TYPE_ALIGN (type, align, BITS_PER_UNIT);
   #else
         align = MAX (align, BITS_PER_UNIT);
   #endif
         SET_TYPE_ALIGN (type, align);
         SET_TYPE_MODE (type, mtcsMode->modes.M_BLKmode);
       //  fprintf(stderr,"mtcsstorlayout.c mtcs_stor_layout_layout_type 100 mode:%d\n",mtcsMode->modes.M_BLKmode);

         if (TYPE_SIZE (type) != 0
         && ! mtcsTarget/*!targetm.member_type_forces_blk*/->member_type_forces_blk(mtcsTarget,type, VOIDmode)
         /* BLKmode elements force BLKmode aggregate;
         else extract/store fields may lose.  */
         && (TYPE_MODE (TREE_TYPE (type)) != mtcsMode->modes.M_BLKmode  || TYPE_NO_FORCE_BLK (TREE_TYPE (type)))){
            SET_TYPE_MODE (type, mode_for_array(self,TREE_TYPE (type),TYPE_SIZE (type)));
            if (TYPE_MODE (type) != mtcsMode->modes.M_BLKmode
            && mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
            && TYPE_ALIGN (type) < mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)
            && TYPE_ALIGN (type) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,TYPE_MODE (type))){
               TYPE_NO_FORCE_BLK (type) = 1;
               SET_TYPE_MODE (type, mtcsMode->modes.M_BLKmode);
            }
         }
         if (AGGREGATE_TYPE_P (element))
            TYPE_TYPELESS_STORAGE (type) = TYPE_TYPELESS_STORAGE (element);
         /* When the element size is constant, check that it is at least as
         large as the element alignment.  */
         if (TYPE_SIZE_UNIT (element)
         && TREE_CODE (TYPE_SIZE_UNIT (element)) == INTEGER_CST
         /* If TYPE_SIZE_UNIT overflowed, then it is certainly larger than
         TYPE_ALIGN_UNIT.  */
         && !TREE_OVERFLOW (TYPE_SIZE_UNIT (element))
         && !integer_zerop (TYPE_SIZE_UNIT (element))){
            if (compare_tree_int (TYPE_SIZE_UNIT (element),TYPE_ALIGN_UNIT (element)) < 0)
               error ("alignment of array elements is greater than element size");
            else if (TYPE_ALIGN_UNIT (element) > 1 && (wi::zext (wi::to_wide (TYPE_SIZE_UNIT (element)),
                  ffs_hwi (TYPE_ALIGN_UNIT (element)) - 1)!= 0))
               error ("size of array element is not a multiple of its alignment");
         }
         break;
      }

      case RECORD_TYPE:
      case UNION_TYPE:
      case QUAL_UNION_TYPE:
      {
         tree field;
         record_layout_info rli;
         /* Initialize the layout information.  */
         rli = start_record_layout (type);
         /* If this is a QUAL_UNION_TYPE, we want to process the fields
         in the reverse order in building the COND_EXPR that denotes
         its size.  We reverse them again later.  */
         if (TREE_CODE (type) == QUAL_UNION_TYPE)
            TYPE_FIELDS (type) = nreverse (TYPE_FIELDS (type));

         /* Place all the fields.  */
         for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
            place_field (rli, field);

         if (TREE_CODE (type) == QUAL_UNION_TYPE)
            TYPE_FIELDS (type) = nreverse (TYPE_FIELDS (type));

         /* Finish laying out the record.  */
         mtcs_stor_layout_finish_record_layout/*!finish_record_layout*/(self,rli, /*free_p=*/true);
      }
      break;

      default:
         gcc_unreachable ();
   }

   /* Compute the final TYPE_SIZE, TYPE_ALIGN, etc. for TYPE.  For
   records and unions, finish_record_layout already called this
   function.  */
   if (!RECORD_OR_UNION_TYPE_P (type))
      finalize_type_size(self,type);

   /* We should never see alias sets on incomplete aggregates.  And we
   should not call layout_type on not incomplete aggregates.  */
   if (AGGREGATE_TYPE_P (type))
      gcc_assert (!TYPE_ALIAS_SET_KNOWN_P (type));
}

/* Given a size SIZE that may not be a constant, return a SAVE_EXPR
   to serve as the actual size-expression for a type or decl.  */
//原型 variable_size stor-layout.h stor-layout.cc
tree mtcs_stor_layout_variable_size (MtcsStorLayout *self,tree size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsLang   *mtcsLang=mtcs_target_get_lang(mtcsTarget);

  /* Obviously.  */
  if (TREE_CONSTANT (size))
    return size;

  /* If the size is self-referential, we can't make a SAVE_EXPR (see
     save_expr for the rationale).  But we can do something else.  */
  if (CONTAINS_PLACEHOLDER_P (size))
    return self_referential_size(self,size);

  /* If we are in the global binding level, we can't make a SAVE_EXPR
     since it may end up being shared across functions, so it is up
     to the front-end to deal with this case.  */
  if (mtcs_lang_global_bindings_p/*!lang_hooks.decls.global_bindings_p*/(mtcsLang))
    return size;

  return save_expr (size);
}

/* Create and return a type for signed integers of PRECISION bits.  */
//原型 make_signed_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_signed_type (MtcsStorLayout *self,int precision)
{
  tree type = make_node (INTEGER_TYPE);
  TYPE_PRECISION (type) = precision;
  mtcs_stor_layout_fixup_signed_type/*!fixup_signed_type*/(self,type);
  return type;
}

/* Create and return a type for unsigned integers of PRECISION bits.  */
//原型 make_unsigned_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_unsigned_type (MtcsStorLayout *self,int precision)
{
  tree type = make_node (INTEGER_TYPE);
  TYPE_PRECISION (type) = precision;
  mtcs_stor_layout_fixup_unsigned_type/*!fixup_unsigned_type*/(self,type);
  return type;
}

/* Set the extreme values of TYPE based on its precision in bits,
   then lay it out.  This is used both in `make_unsigned_type'
   and for enumeral types.  */
//原型 fixup_unsigned_type stor-layout.h stor-layout.cc
void mtcs_stor_layout_fixup_unsigned_type (MtcsStorLayout *self,tree type)
{
  int precision = TYPE_PRECISION (type);
  TYPE_UNSIGNED (type) = 1;
  set_min_and_max_values_for_integral_type (type, precision, UNSIGNED);
  /* Lay out the type: set its alignment, size, etc.  */
  mtcs_stor_layout_layout_type/*!layout_type*/(self,type);
}

/* Set the extreme values of TYPE based on its precision in bits,
   then lay it out.  Used when make_signed_type won't do
   because the tree code is not INTEGER_TYPE.  */
//原型 fixup_signed_type stor-layout.h stor-layout.cc
void mtcs_stor_layout_fixup_signed_type (MtcsStorLayout *self,tree type)
{
  int precision = TYPE_PRECISION (type);
  set_min_and_max_values_for_integral_type (type, precision, SIGNED);
  /* Lay out the type: set its alignment, size, etc.  */
  mtcs_stor_layout_layout_type/*!layout_type*/(self,type);
}



/* Assuming that all the fields have been laid out, this function uses
   RLI to compute the final TYPE_SIZE, TYPE_ALIGN, etc. for the type
   indicated by RLI.  */
//原型 finalize_record_size stor-layout.cc
static void finalize_record_size (MtcsStorLayout *self,record_layout_info rli)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree unpadded_size, unpadded_size_unit;

   /* Now we want just byte and bit offsets, so set the offset alignment
   to be a byte and then normalize.  */
   rli->offset_align = BITS_PER_UNIT;
   normalize_rli (rli);

   /* Determine the desired alignment.  */
#ifdef ROUND_TYPE_ALIGN //host=0 nvptx=0
   SET_TYPE_ALIGN (rli->t, ROUND_TYPE_ALIGN (rli->t, TYPE_ALIGN (rli->t),
   rli->record_align));
#else
   SET_TYPE_ALIGN (rli->t, MAX (TYPE_ALIGN (rli->t), rli->record_align));
#endif

   /* Compute the size so far.  Be sure to allow for extra bits in the
   size in bytes.  We have guaranteed above that it will be no more
   than a single byte.  */
   unpadded_size = rli_size_so_far (rli);
   unpadded_size_unit = rli_size_unit_so_far (rli);
   if (! integer_zerop (rli->bitpos))
      unpadded_size_unit   = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, unpadded_size_unit, size_one_node);

   /* Round the size up to be a multiple of the required alignment.  */
   TYPE_SIZE (rli->t) = round_up (unpadded_size, TYPE_ALIGN (rli->t));
   TYPE_SIZE_UNIT (rli->t) = round_up (unpadded_size_unit, TYPE_ALIGN_UNIT (rli->t));

   if (TREE_CONSTANT (unpadded_size)
   && simple_cst_equal (unpadded_size, TYPE_SIZE (rli->t)) == 0
   && input_location != BUILTINS_LOCATION
   && !TYPE_ARTIFICIAL (rli->t)){
      tree pad_size  = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, TYPE_SIZE_UNIT (rli->t), unpadded_size_unit);
      warning (OPT_Wpadded, "padding struct size to alignment boundary with %E bytes", pad_size);
   }

   if (warn_packed && TREE_CODE (rli->t) == RECORD_TYPE
   && TYPE_PACKED (rli->t) && ! rli->packed_maybe_necessary
   && TREE_CONSTANT (unpadded_size)) {
      tree unpacked_size;

   #ifdef ROUND_TYPE_ALIGN //host=0 nvptx=0
      rli->unpacked_align = ROUND_TYPE_ALIGN (rli->t, TYPE_ALIGN (rli->t), rli->unpacked_align);
   #else
      rli->unpacked_align = MAX (TYPE_ALIGN (rli->t), rli->unpacked_align);
   #endif

      unpacked_size = round_up (TYPE_SIZE (rli->t), rli->unpacked_align);
      if (simple_cst_equal (unpacked_size, TYPE_SIZE (rli->t))){
         if (TYPE_NAME (rli->t)){
            tree name;

            if (TREE_CODE (TYPE_NAME (rli->t)) == IDENTIFIER_NODE)
               name = TYPE_NAME (rli->t);
            else
               name = DECL_NAME (TYPE_NAME (rli->t));

            if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign))
               warning (OPT_Wpacked, "packed attribute causes inefficient alignment for %qE", name);
            else
               warning (OPT_Wpacked,"packed attribute is unnecessary for %qE", name);
         }else{
            if (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign))
               warning (OPT_Wpacked,  "packed attribute causes inefficient alignment");
            else
               warning (OPT_Wpacked, "packed attribute is unnecessary");
         }
      }
   }
}

/* Compute the TYPE_MODE for the TYPE (which is a RECORD_TYPE).  */
//原型 compute_record_mode stor-layout.h stor-layout.cc
void mtcs_stor_layout_compute_record_mode (MtcsStorLayout *self,tree type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree field;
   machine_mode mode = VOIDmode;
   /* Most RECORD_TYPEs have BLKmode, so we start off assuming that.
   However, if possible, we use a mode that fits in a register
   instead, in order to allow for better optimization down the
   line.  */
   SET_TYPE_MODE (type, mtcsMode->modes.M_BLKmode);

   poly_uint64 type_size;
   if (!poly_int_tree_p (TYPE_SIZE (type), &type_size))
      return;

   /* A record which has any BLKmode members must itself be
   BLKmode; it can't go in a register.  Unless the member is
   BLKmode only because it isn't aligned.  */
   for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field)){
      if (TREE_CODE (field) != FIELD_DECL)
         continue;

      poly_uint64 field_size;
      if (TREE_CODE (TREE_TYPE (field)) == ERROR_MARK
      || (TYPE_MODE (TREE_TYPE (field)) == mtcsMode->modes.M_BLKmode
      && ! TYPE_NO_FORCE_BLK (TREE_TYPE (field))
      && !(TYPE_SIZE (TREE_TYPE (field)) != 0
      && integer_zerop (TYPE_SIZE (TREE_TYPE (field)))))
      || !tree_fits_poly_uint64_p (bit_position (field))
      || DECL_SIZE (field) == 0
      || !poly_int_tree_p (DECL_SIZE (field), &field_size))
         return;

      /* If this field is the whole struct, remember its mode so
      that, say, we can put a double in a class into a DF
      register instead of forcing it to live in the stack.  */
      if (known_eq (field_size, type_size)
      /* Partial int types (e.g. __int20) may have TYPE_SIZE equal to
      wider types (e.g. int32), despite precision being less.  Ensure
      that the TYPE_MODE of the struct does not get set to the partial
      int mode if there is a wider type also in the struct.  */
      && known_gt (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,DECL_MODE (field)),
      mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode)))
         mode = DECL_MODE (field);

      /* With some targets, it is sub-optimal to access an aligned
      BLKmode structure as a scalar.  */
      if (mtcsTarget/*!targetm.member_type_forces_blk*/->member_type_forces_blk(mtcsTarget,field, mode))
         return;
   }

   /* If we only have one real field; use its mode if that mode's size
   matches the type's size.  This generally only applies to RECORD_TYPE.
   For UNION_TYPE, if the widest field is MODE_INT then use that mode.
   If the widest field is MODE_PARTIAL_INT, and the union will be passed
   by reference, then use that mode.  */
   if ((TREE_CODE (type) == RECORD_TYPE
   || (TREE_CODE (type) == UNION_TYPE
   && (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_INT
   || (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_PARTIAL_INT
   && (target_calls_pass_by_reference/*!targetm.calls.pass_by_reference*/(mtcsMachine->calls,
   pack_cumulative_args (0),mtcs_function_arg_info (mtcsMode,type, mode, /*named=*/false)))))))
   && mode != VOIDmode
   && known_eq (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode), type_size))
      ;
   else
      mode = mtcs_stor_layout_mode_for_size_tree/*!mode_for_size_tree*/(self,TYPE_SIZE (type), MODE_INT, 1).else_blk ();

   /* If structure's known alignment is less than what the scalar
   mode would need, and it matters, then stick with BLKmode.  */
   if (mode != mtcsMode->modes.M_BLKmode
   && mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
   && ! (TYPE_ALIGN (type) >= mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)
   || TYPE_ALIGN (type) >= mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode))){
      /* If this is the only reason this type is BLKmode, then
      don't force containing types to be BLKmode.  */
      TYPE_NO_FORCE_BLK (type) = 1;
      mode = mtcsMode->modes.M_BLKmode;
   }

   SET_TYPE_MODE (type, mode);
}

/* Do all of the work required to layout the type indicated by RLI,
   once the fields have been laid out.  This function will call `free'
   for RLI, unless FREE_P is false.  Passing a value other than false
   for FREE_P is bad practice; this option only exists to support the
   G++ 3.2 ABI.  */
//原型 finish_record_layout stor-layout.h stor-layout.cc
void mtcs_stor_layout_finish_record_layout (MtcsStorLayout *self,record_layout_info rli, int free_p)
{
  tree variant;

  /* Compute the final size.  */
  finalize_record_size(self,rli);

  /* Compute the TYPE_MODE for the record.  */
  mtcs_stor_layout_compute_record_mode/*!compute_record_mode*/(self,rli->t);

  /* Perform any last tweaks to the TYPE_SIZE, etc.  */
  finalize_type_size(self,rli->t);

  /* Compute bitfield representatives.  */
  mtcs_stor_layout_finish_bitfield_layout/*!finish_bitfield_layout*/(self,rli->t);

  /* Propagate TYPE_PACKED and TYPE_REVERSE_STORAGE_ORDER to variants.
     With C++ templates, it is too early to do this when the attribute
     is being parsed.  */
  for (variant = TYPE_NEXT_VARIANT (rli->t); variant;  variant = TYPE_NEXT_VARIANT (variant)){
      TYPE_PACKED (variant) = TYPE_PACKED (rli->t);
      TYPE_REVERSE_STORAGE_ORDER (variant) = TYPE_REVERSE_STORAGE_ORDER (rli->t);
    }

  /* Lay out any static members.  This is done now because their type
     may use the record's type.  */
  while (!vec_safe_is_empty (rli->pending_statics))
     mtcs_stor_layout_layout_decl/*!layout_decl*/(self,rli->pending_statics->pop (), 0);

  /* Clean up.  */
  if (free_p){
      vec_free (rli->pending_statics);
      free (rli);
  }
}

/* Return a new underlying object for a bitfield started with FIELD.  */
//原型 start_bitfield_representative stor-layout.cc
static tree start_bitfield_representative (MtcsStorLayout *self,tree field)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree repr = make_node (FIELD_DECL);
   DECL_FIELD_OFFSET (repr) = DECL_FIELD_OFFSET (field);
   /* Force the representative to begin at a BITS_PER_UNIT aligned
   boundary - C++ may use tail-padding of a base object to
   continue packing bits so the bitfield region does not start
   at bit zero (see g++.dg/abi/bitfield5.C for example).
   Unallocated bits may happen for other reasons as well,
   for example Ada which allows explicit bit-granular structure layout.  */
   DECL_FIELD_BIT_OFFSET (repr)= mtcs_const_size_binop/*!size_binop*/(mtcsConst,BIT_AND_EXPR,
         DECL_FIELD_BIT_OFFSET (field),bitsize_int (~(BITS_PER_UNIT - 1)));
   SET_DECL_OFFSET_ALIGN (repr, DECL_OFFSET_ALIGN (field));
   DECL_SIZE (repr) = DECL_SIZE (field);
   DECL_SIZE_UNIT (repr) = DECL_SIZE_UNIT (field);
   DECL_PACKED (repr) = DECL_PACKED (field);
   DECL_CONTEXT (repr) = DECL_CONTEXT (field);
   /* There are no indirect accesses to this field.  If we introduce
   some then they have to use the record alias set.  This makes
   sure to properly conflict with [indirect] accesses to addressable
   fields of the bitfield group.  */
   DECL_NONADDRESSABLE_P (repr) = 1;
   return repr;
}

/* Finish up a bitfield group that was started by creating the underlying
   object REPR with the last field in the bitfield group FIELD.  */
//原型 finish_bitfield_representative stor-layout.cc
static void finish_bitfield_representative (MtcsStorLayout *self,tree repr, tree field)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine   *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   unsigned HOST_WIDE_INT bitsize, maxbitsize;
   tree nextf, size;

   size = size_diffop (DECL_FIELD_OFFSET (field),DECL_FIELD_OFFSET (repr));
   while (TREE_CODE (size) == COMPOUND_EXPR)
      size = TREE_OPERAND (size, 1);
   gcc_assert (tree_fits_uhwi_p (size));
   bitsize = (tree_to_uhwi (size) * BITS_PER_UNIT
               + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (field))
               - tree_to_uhwi (DECL_FIELD_BIT_OFFSET (repr))
               + tree_to_uhwi (DECL_SIZE (field)));

   /* Round up bitsize to multiples of BITS_PER_UNIT.  */
   bitsize = (bitsize + BITS_PER_UNIT - 1) & ~(BITS_PER_UNIT - 1);

   /* Now nothing tells us how to pad out bitsize ...  */
   if (TREE_CODE (DECL_CONTEXT (field)) == RECORD_TYPE){
      nextf = DECL_CHAIN (field);
      while (nextf && TREE_CODE (nextf) != FIELD_DECL)
         nextf = DECL_CHAIN (nextf);
   }else
      nextf = NULL_TREE;
   if (nextf){
      tree maxsize;
      /* If there was an error, the field may be not laid out
      correctly.  Don't bother to do anything.  */
      if (TREE_TYPE (nextf) == error_mark_node){
         TREE_TYPE (repr) = error_mark_node;
         return;
      }
      maxsize = size_diffop (DECL_FIELD_OFFSET (nextf),DECL_FIELD_OFFSET (repr));
      if (tree_fits_uhwi_p (maxsize)){
         maxbitsize = (tree_to_uhwi (maxsize) * BITS_PER_UNIT
                     + tree_to_uhwi (DECL_FIELD_BIT_OFFSET (nextf))
                     - tree_to_uhwi (DECL_FIELD_BIT_OFFSET (repr)));
         /* If the group ends within a bitfield nextf does not need to be
         aligned to BITS_PER_UNIT.  Thus round up.  */
         maxbitsize = (maxbitsize + BITS_PER_UNIT - 1) & ~(BITS_PER_UNIT - 1);
      }else
         maxbitsize = bitsize;
   }else{
      /* Note that if the C++ FE sets up tail-padding to be re-used it
      creates a as-base variant of the type with TYPE_SIZE adjusted
      accordingly.  So it is safe to include tail-padding here.  */
      tree aggsize =mtcs_tree_unit_size_without_reusable_padding/*!lang_hooks.types.unit_size_without_reusable_padding*/
                                                                 (mtcsTree,DECL_CONTEXT (field));
      tree maxsize = size_diffop (aggsize, DECL_FIELD_OFFSET (repr));
      /* We cannot generally rely on maxsize to fold to an integer constant,
      so use bitsize as fallback for this case.  */
      if (tree_fits_uhwi_p (maxsize))
         maxbitsize = (tree_to_uhwi (maxsize) * BITS_PER_UNIT - tree_to_uhwi (DECL_FIELD_BIT_OFFSET (repr)));
      else
         maxbitsize = bitsize;
   }

   /* Only if we don't artificially break up the representative in
   the middle of a large bitfield with different possibly
   overlapping representatives.  And all representatives start
   at byte offset.  */
   gcc_assert (maxbitsize % BITS_PER_UNIT == 0);

   /* Find the smallest nice mode to use.  */
   opt_scalar_int_mode mode_iter;
   MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode_iter, MODE_INT)
      if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode_iter.require ()) >= bitsize)
         break;

   scalar_int_mode mode;
   if (!mode_iter.exists (&mode)
     || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) > maxbitsize
     || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) >
            mtcs_mode_get_max_fixed_size/*!MAX_FIXED_MODE_SIZE*/(mtcsMode)){
      if (TREE_CODE (TREE_TYPE (field)) == BITINT_TYPE){
         struct bitint_info info;
         unsigned prec = TYPE_PRECISION (TREE_TYPE (field));
         bool ok = target_c_bitint_type_info/*!targetm.c.bitint_type_info*/(mtcsMachine->c,prec, &info);
         gcc_assert (ok);
         scalar_int_mode limb_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,info.abi_limb_mode);
         unsigned lprec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode);
         if (prec > lprec) {
            /* For middle/large/huge _BitInt prefer bitsize being a multiple
            of limb precision.  */
            unsigned HOST_WIDE_INT bsz = CEIL (bitsize, lprec) * lprec;
            if (bsz <= maxbitsize)
               bitsize = bsz;
         }
      }
      /* We really want a BLKmode representative only as a last resort,
      considering the member b in
      struct { int a : 7; int b : 17; int c; } __attribute__((packed));
      Otherwise we simply want to split the representative up
      allowing for overlaps within the bitfield region as required for
      struct { int a : 7; int b : 7;
      int c : 10; int d; } __attribute__((packed));
      [0, 15] HImode for a and b, [8, 23] HImode for c.  */
      DECL_SIZE (repr) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,bitsize);
      DECL_SIZE_UNIT (repr) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,bitsize / BITS_PER_UNIT);
      SET_DECL_MODE (repr, mtcsMode->modes.M_BLKmode);
      TREE_TYPE (repr) = build_array_type_nelts (unsigned_char_type_node, bitsize / BITS_PER_UNIT);
   }else {
      unsigned HOST_WIDE_INT modesize = mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode);
      DECL_SIZE (repr) = mtcs_tree_get_bitsize_int/*!bitsize_int*/(mtcsTree,modesize);
      DECL_SIZE_UNIT (repr) = mtcs_tree_get_size_int/*!size_int*/(mtcsTree,modesize / BITS_PER_UNIT);
      SET_DECL_MODE (repr, mode);
      TREE_TYPE (repr) = mtcs_tree_type_for_mode/*!lang_hooks.types.type_for_mode*/(mtcsTree,mode, 1);
   }

   /* Remember whether the bitfield group is at the end of the
   structure or not.  */
   DECL_CHAIN (repr) = nextf;
}



/* Compute and set FIELD_DECLs for the underlying objects we should
   use for bitfield access for the structure T.  */
//原型 finish_bitfield_layout stor-layout.h stor-layout.cc
void mtcs_stor_layout_finish_bitfield_layout (MtcsStorLayout *self,tree t)
{
   tree field, prev;
   tree repr = NULL_TREE;

   if (TREE_CODE (t) == QUAL_UNION_TYPE)
      return;

   for (prev = NULL_TREE, field = TYPE_FIELDS (t);  field; field = DECL_CHAIN (field)){
      if (TREE_CODE (field) != FIELD_DECL)
         continue;

      /* In the C++ memory model, consecutive bit fields in a structure are
      considered one memory location and updating a memory location
      may not store into adjacent memory locations.  */
      if (!repr  && DECL_BIT_FIELD_TYPE (field)){
         /* Start new representative.  */
         repr = start_bitfield_representative(self,field);
      } else if (repr  && ! DECL_BIT_FIELD_TYPE (field)){
         /* Finish off new representative.  */
         finish_bitfield_representative(self,repr, prev);
         repr = NULL_TREE;
      } else if (DECL_BIT_FIELD_TYPE (field)){
         gcc_assert (repr != NULL_TREE);

         /* Zero-size bitfields finish off a representative and
         do not have a representative themselves.  This is
         required by the C++ memory model.  */
         if (integer_zerop (DECL_SIZE (field))){
            finish_bitfield_representative(self,repr, prev);
            repr = NULL_TREE;
         }
         /* We assume that either DECL_FIELD_OFFSET of the representative
         and each bitfield member is a constant or they are equal.
         This is because we need to be able to compute the bit-offset
         of each field relative to the representative in get_bit_range
         during RTL expansion.
         If these constraints are not met, simply force a new
         representative to be generated.  That will at most
         generate worse code but still maintain correctness with
         respect to the C++ memory model.  */
         else if (!((tree_fits_uhwi_p (DECL_FIELD_OFFSET (repr))
         && tree_fits_uhwi_p (DECL_FIELD_OFFSET (field)))
         || operand_equal_p (DECL_FIELD_OFFSET (repr), DECL_FIELD_OFFSET (field), 0))){
            finish_bitfield_representative(self,repr, prev);
            repr = start_bitfield_representative(self,field);
         }
      }else
         continue;

      if (repr)
         DECL_BIT_FIELD_REPRESENTATIVE (field) = repr;

      if (TREE_CODE (t) == RECORD_TYPE)
         prev = field;
      else if (repr){
         finish_bitfield_representative(self,repr, field);
         repr = NULL_TREE;
      }
   }

   if (repr)
      finish_bitfield_representative(self,repr, prev);
}

/* Subroutine of layout_decl: Force alignment required for the data type.
   But if the decl itself wants greater alignment, don't override that.  */
//原型 do_type_align stor-layout.cc

static inline void do_type_align (tree type, tree decl)
{
   if (TYPE_ALIGN (type) > DECL_ALIGN (decl)){
      SET_DECL_ALIGN (decl, TYPE_ALIGN (type));
      if (TREE_CODE (decl) == FIELD_DECL)
         DECL_USER_ALIGN (decl) = TYPE_USER_ALIGN (type);
   }
   if (TYPE_WARN_IF_NOT_ALIGN (type) > DECL_WARN_IF_NOT_ALIGN (decl))
      SET_DECL_WARN_IF_NOT_ALIGN (decl, TYPE_WARN_IF_NOT_ALIGN (type));
}

/* Set the size, mode and alignment of a ..._DECL node.
   TYPE_DECL does need this for C++.
   Note that LABEL_DECL and CONST_DECL nodes do not need this,
   and FUNCTION_DECL nodes have them set up in a special (and simple) way.
   Don't call layout_decl for them.

   KNOWN_ALIGN is the amount of alignment we can assume this
   decl has with no special effort.  It is relevant only for FIELD_DECLs
   and depends on the previous fields.
   All that matters about KNOWN_ALIGN is which powers of 2 divide it.
   If KNOWN_ALIGN is 0, it means, "as much alignment as you like":
   the record will be aligned to suit.  */
//原型 layout_decl stor-layout.h stor-layout.cc
void mtcs_stor_layout_layout_decl (MtcsStorLayout *self,tree decl, unsigned int known_align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   tree type = TREE_TYPE (decl);
   enum tree_code code = TREE_CODE (decl);
   rtx rtl = NULL_RTX;
   location_t loc = DECL_SOURCE_LOCATION (decl);

   if (code == CONST_DECL)
      return;

   gcc_assert (code == VAR_DECL || code == PARM_DECL || code == RESULT_DECL
                     || code == TYPE_DECL || code == FIELD_DECL);

   rtl = DECL_RTL_IF_SET (decl);

   if (type == error_mark_node)
      type = void_type_node;

   /* Usually the size and mode come from the data type without change,
   however, the front-end may set the explicit width of the field, so its
   size may not be the same as the size of its type.  This happens with
   bitfields, of course (an `int' bitfield may be only 2 bits, say), but it
   also happens with other fields.  For example, the C++ front-end creates
   zero-sized fields corresponding to empty base classes, and depends on
   layout_type setting DECL_FIELD_BITPOS correctly for the field.  Set the
   size in bytes from the size in bits.  If we have already set the mode,
   don't set it again since we can be called twice for FIELD_DECLs.  */

   DECL_UNSIGNED (decl) = TYPE_UNSIGNED (type);
   if (DECL_MODE (decl) == VOIDmode){
      n_debug("mtcsstorlayout.c mtcs_stor_layout_layout_decl 00 decl:%p mode:%d\n",decl,TYPE_MODE(type));
      SET_DECL_MODE (decl, TYPE_MODE (type));
   }
   if (DECL_SIZE (decl) == 0){
      DECL_SIZE (decl) = TYPE_SIZE (type);
      DECL_SIZE_UNIT (decl) = TYPE_SIZE_UNIT (type);
   } else if (DECL_SIZE_UNIT (decl) == 0)
      DECL_SIZE_UNIT (decl)= mtcs_const_fold_convert_loc/*!fold_convert_loc*/(mtcsConst,loc, sizetype,
            mtcs_const_size_binop_loc/*!size_binop_loc*/(mtcsConst,loc, CEIL_DIV_EXPR, DECL_SIZE (decl),bitsize_unit_node));

   if (code != FIELD_DECL)
      /* For non-fields, update the alignment from the type.  */
      do_type_align (type, decl);
   else
   /* For fields, it's a bit more complicated...  */
   {
      bool old_user_align = DECL_USER_ALIGN (decl);
      bool zero_bitfield = false;
      bool packed_p = DECL_PACKED (decl);
      unsigned int mfa;

      if (DECL_BIT_FIELD (decl)){
         DECL_BIT_FIELD_TYPE (decl) = type;

         /* A zero-length bit-field affects the alignment of the next
         field.  In essence such bit-fields are not influenced by
         any packing due to #pragma pack or attribute packed.  */
         if (integer_zerop (DECL_SIZE (decl))
         && ! mtcsTarget/*!targetm.ms_bitfield_layout_p*/->ms_bitfield_layout_p(mtcsTarget,DECL_FIELD_CONTEXT (decl))){
            zero_bitfield = true;
            packed_p = false;
            if (PCC_BITFIELD_TYPE_MATTERS)
               do_type_align (type, decl);
            else{
            #ifdef EMPTY_FIELD_BOUNDARY //host=0 nvptx=0
               if (EMPTY_FIELD_BOUNDARY > DECL_ALIGN (decl)){
                  SET_DECL_ALIGN (decl, EMPTY_FIELD_BOUNDARY);
                  DECL_USER_ALIGN (decl) = 0;
               }
            #endif
            }
         }

         /* See if we can use an ordinary integer mode for a bit-field.
         Conditions are: a fixed size that is correct for another mode,
         occupying a complete byte or bytes on proper boundary.  */
         if (TYPE_SIZE (type) != 0
         && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST
         &&mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,TYPE_MODE (type)) == MODE_INT){
            machine_mode xmode;
            if (mode_for_size_tree (DECL_SIZE (decl),MODE_INT, 1).exists (&xmode)){
               unsigned int xalign = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,xmode);
               if (!(xalign > BITS_PER_UNIT && DECL_PACKED (decl))
               && (known_align == 0 || known_align >= xalign)){
                  n_debug("mtcsstorlayout.c mtcs_stor_layout_layout_decl 00 decl:%p mode:%d\n",decl,xmode);

                  SET_DECL_ALIGN (decl, MAX (xalign, DECL_ALIGN (decl)));
                  SET_DECL_MODE (decl, xmode);
                  DECL_BIT_FIELD (decl) = 0;
               }
            }
         }

         /* Turn off DECL_BIT_FIELD if we won't need it set.  */
         if (TYPE_MODE (type) == mtcsMode->modes.M_BLKmode && DECL_MODE (decl) ==mtcsMode->modes.M_BLKmode
           && known_align >= TYPE_ALIGN (type) && DECL_ALIGN (decl) >= TYPE_ALIGN (type))
            DECL_BIT_FIELD (decl) = 0;
      }else if (packed_p && DECL_USER_ALIGN (decl))
         /* Don't touch DECL_ALIGN.  For other packed fields, go ahead and
         round up; we'll reduce it again below.  We want packing to
         supersede USER_ALIGN inherited from the type, but defer to
         alignment explicitly specified on the field decl.  */
            ;
      else
            do_type_align (type, decl);

      /* If the field is packed and not explicitly aligned, give it the
      minimum alignment.  Note that do_type_align may set
      DECL_USER_ALIGN, so we need to check old_user_align instead.  */
      if (packed_p  && !old_user_align)
         SET_DECL_ALIGN (decl, MIN (DECL_ALIGN (decl), BITS_PER_UNIT));

      if (! packed_p && ! DECL_USER_ALIGN (decl)){
         /* Some targets (i.e. i386, VMS) limit struct field alignment
         to a lower boundary than alignment of variables unless
         it was overridden by attribute aligned.  */
         #ifdef BIGGEST_FIELD_ALIGNMENT
            SET_DECL_ALIGN (decl, MIN (DECL_ALIGN (decl),(unsigned) BIGGEST_FIELD_ALIGNMENT));
         #endif
         #ifdef ADJUST_FIELD_ALIGN
            SET_DECL_ALIGN (decl, ADJUST_FIELD_ALIGN (decl, TREE_TYPE (decl),DECL_ALIGN (decl)));
         #endif
      }

      if (zero_bitfield)
         mfa = initial_max_fld_align * BITS_PER_UNIT;
      else
         mfa = maximum_field_alignment;
      /* Should this be controlled by DECL_USER_ALIGN, too?  */
      if (mfa != 0)
         SET_DECL_ALIGN (decl, MIN (DECL_ALIGN (decl), mfa));
   }

   /* Evaluate nonconstant size only once, either now or as soon as safe.  */
   if (DECL_SIZE (decl) != 0 && TREE_CODE (DECL_SIZE (decl)) != INTEGER_CST)
      DECL_SIZE (decl) = mtcs_stor_layout_variable_size/*!variable_size*/(self,DECL_SIZE (decl));
   if (DECL_SIZE_UNIT (decl) != 0  && TREE_CODE (DECL_SIZE_UNIT (decl)) != INTEGER_CST)
      DECL_SIZE_UNIT (decl) = mtcs_stor_layout_variable_size/*!variable_size*/(self,DECL_SIZE_UNIT (decl));

   /* If requested, warn about definitions of large data objects.  */
   if ((code == PARM_DECL || (code == VAR_DECL && !DECL_NONLOCAL_FRAME (decl))) && !DECL_EXTERNAL (decl)) {
      tree size = DECL_SIZE_UNIT (decl);

      if (size != 0 && TREE_CODE (size) == INTEGER_CST){
         /* -Wlarger-than= argument of HOST_WIDE_INT_MAX is treated
         as if PTRDIFF_MAX had been specified, with the value
         being that on the target rather than the host.  */
         unsigned HOST_WIDE_INT max_size = mtcsOptionsItem->x_warn_larger_than_size;
         if (max_size == HOST_WIDE_INT_MAX)
            max_size = tree_to_shwi (TYPE_MAX_VALUE (ptrdiff_type_node));

         if (compare_tree_int (size, max_size) > 0)
            warning (OPT_Wlarger_than_, "size of %q+D %E bytes exceeds maximum object size %wu",decl, size, max_size);
      }
   }

   /* If the RTL was already set, update its mode and mem attributes.  */
   if (rtl){
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,rtl, DECL_MODE (decl));
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, 0);
      if (MEM_P (rtl))
         mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,rtl, decl, 1);
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, rtl);
   }
}

/* Create and return a type for fract of PRECISION bits, UNSIGNEDP,
   and SATP.  */
//原型 make_fract_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_fract_type (MtcsStorLayout *self,int precision, int unsignedp, int satp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   tree type = make_node (FIXED_POINT_TYPE);
   TYPE_PRECISION (type) = precision;
   if (satp)
      TYPE_SATURATING (type) = 1;
   /* Lay out the type: set its alignment, size, etc.  */
   TYPE_UNSIGNED (type) = unsignedp;
   enum mode_class mclass = unsignedp ? MODE_UFRACT : MODE_FRACT;
   SET_TYPE_MODE (type, mtcs_mode_mode_for_size/*!mode_for_size*/(mtcsMode,precision, mclass, 0).require ());
   mtcs_stor_layout_layout_type/*!layout_type*/(self,type);
   return type;
}

/* Create and return a type for accum of PRECISION bits, UNSIGNEDP,
   and SATP.  */
//原型 make_accum_type stor-layout.h stor-layout.cc
tree mtcs_stor_layout_make_accum_type (MtcsStorLayout *self,int precision, int unsignedp, int satp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   tree type = make_node (FIXED_POINT_TYPE);
   TYPE_PRECISION (type) = precision;
   if (satp)
      TYPE_SATURATING (type) = 1;
   /* Lay out the type: set its alignment, size, etc.  */
   TYPE_UNSIGNED (type) = unsignedp;
   enum mode_class mclass = unsignedp ? MODE_UACCUM : MODE_ACCUM;
   SET_TYPE_MODE (type, mtcs_mode_mode_for_size/*!mode_for_size*/(mtcsMode,precision, mclass, 0).require ());
   mtcs_stor_layout_layout_type/*!layout_type*/(self,type);

   return type;
}


/* Given a VAR_DECL, PARM_DECL, RESULT_DECL, or FIELD_DECL, clears the
   results of a previous call to layout_decl and calls it again.  */
//原型 relayout_decl stor-layout.h stor-layout.cc
void mtcs_stor_layout_relayout_decl (MtcsStorLayout *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   DECL_SIZE (decl) = DECL_SIZE_UNIT (decl) = 0;
   SET_DECL_MODE (decl, VOIDmode);
   if (!DECL_USER_ALIGN (decl))
      SET_DECL_ALIGN (decl, 0);
   if (DECL_RTL_SET_P (decl))
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, 0);

   mtcs_stor_layout_layout_decl/*!layout_decl*/(self,decl, 0);
}

MtcsStorLayout    *mtcs_stor_layout_new(MtcsMode *mtcsMode)
{
    MtcsStorLayout *self = n_slice_alloc0 (sizeof(MtcsStorLayout));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsStorLayoutInit(self);
    return self;
}

