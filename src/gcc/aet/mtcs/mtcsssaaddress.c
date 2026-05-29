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
 * base on tree-ssa-address.cc
 */

/* Utility functions for manipulation with TARGET_MEM_REFs -- tree expressions
   that directly map to addressing modes of the target.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "memmodel.h"
#include "stringpool.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "expmed.h"
#include "insn-config.h"
#include "emit-rtl.h"
#include "recog.h"
#include "tree-pretty-print.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "tree-ssa-loop-ivopts.h"
#include "expr.h"
#include "tree-dfa.h"
#include "dumpfile.h"
#include "tree-affine.h"
#include "gimplify.h"
#include "builtins.h"

/* FIXME: We compute address costs using RTL.  */
#include "tree-ssa-address.h"

#include "mtcsssaaddress.h"
#include "mtcstarget.h"
#include "aet/aetprinttree.h"
#include "aet/aetprintgimple.h"

/* TODO -- handling of symbols (according to Richard Hendersons
   comments, http://gcc.gnu.org/ml/gcc-patches/2005-04/msg00949.html):

   There are at least 5 different kinds of symbols that we can run up against:

     (1) binds_local_p, small data area.
     (2) binds_local_p, eg local statics
     (3) !binds_local_p, eg global variables
     (4) thread local, local_exec
     (5) thread local, !local_exec

   Now, (1) won't appear often in an array context, but it certainly can.
   All you have to do is set -GN high enough, or explicitly mark any
   random object __attribute__((section (".sdata"))).

   All of these affect whether or not a symbol is in fact a valid address.
   The only one tested here is (3).  And that result may very well
   be incorrect for (4) or (5).

   An incorrect result here does not cause incorrect results out the
   back end, because the expander in expr.cc validizes the address.  However
   it would be nice to improve the handling here in order to produce more
   precise results.  */

/* A "template" for memory address, used to determine whether the address is
   valid for mode.  */

struct GTY (()) mem_addr_template {
  rtx ref;        /* The template.  */
  rtx * GTY ((skip)) step_p;  /* The point in template where the step should be
               filled in.  */
  rtx * GTY ((skip)) off_p;   /* The point in template where the offset should
               be filled in.  */
};


/* The templates.  Each of the low five bits of the index corresponds to one
   component of TARGET_MEM_REF being present, while the high bits identify
   the address space.  See TEMPL_IDX.  */

static GTY(()) vec<mem_addr_template, va_gc> *mem_addr_template_list;

#define TEMPL_IDX(AS, SYMBOL, BASE, INDEX, STEP, OFFSET) \
  (((int) (AS) << 5) \
   | ((SYMBOL != 0) << 4) \
   | ((BASE != 0) << 3) \
   | ((INDEX != 0) << 2) \
   | ((STEP != 0) << 1) \
   | (OFFSET != 0))

/* Stores address for memory reference with parameters SYMBOL, BASE, INDEX,
   STEP and OFFSET to *ADDR using address mode ADDRESS_MODE.  Stores pointers
   to where step is placed to *STEP_P and offset to *OFFSET_P.  */

static void gen_addr_rtx (MtcsSsaAddress *self,machine_mode address_mode,
         rtx symbol, rtx base, rtx index, rtx step, rtx offset,
         rtx *addr, rtx **step_p, rtx **offset_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);

   rtx act_elem;

   *addr = NULL_RTX;
   if (step_p)
      *step_p = NULL;
   if (offset_p)
      *offset_p = NULL;

   if (index && index != const0_rtx){
      act_elem = index;
      if (step){
         act_elem = gen_rtx_MULT (address_mode, act_elem, step);

         if (step_p)
            *step_p = &XEXP (act_elem, 1);
      }

      *addr = act_elem;
   }

   if (base && base != const0_rtx){
      if (*addr)
         *addr =  mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, address_mode, base, *addr);
      else
         *addr = base;
   }

   if (symbol){
      act_elem = symbol;
      if (offset){
         act_elem = gen_rtx_PLUS (address_mode, act_elem, offset);

         if (offset_p)
            *offset_p = &XEXP (act_elem, 1);

         if (GET_CODE (symbol) == SYMBOL_REF
         || GET_CODE (symbol) == LABEL_REF
         || GET_CODE (symbol) == CONST)
            act_elem = gen_rtx_CONST (address_mode, act_elem);
      }

      if (*addr)
         *addr = gen_rtx_PLUS (address_mode, *addr, act_elem);
      else
         *addr = act_elem;
   }else if (offset){
      if (*addr){
         *addr = gen_rtx_PLUS (address_mode, *addr, offset);
         if (offset_p)
            *offset_p = &XEXP (*addr, 1);
      }else{
         *addr = offset;
         if (offset_p)
            *offset_p = addr;
      }
   }

   if (!*addr)
      *addr = const0_rtx;
}

/* Returns address for TARGET_MEM_REF with parameters given by ADDR
   in address space AS.
   If REALLY_EXPAND is false, just make fake registers instead
   of really expanding the operands, and perform the expansion in-place
   by using one of the "templates".  */
//原型 addr_for_mem_ref tree-ssa-address.h tree-ssa-address.cc
//重载函数
rtx mtcs_ssa_address_addr_for_mem_ref (MtcsSsaAddress *self,struct mem_address *addr, addr_space_t as,
        bool really_expand)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   scalar_int_mode address_mode = target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
   scalar_int_mode pointer_mode = target_addr_space_pointer_mode/*!targetm.addr_space.pointer_mode*/(mtcsMachine->addrSpace,as);
   rtx address, sym, bse, idx, st, off;
   struct mem_addr_template *templ;

   if (addr->step && !integer_onep (addr->step))
      st = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/ (mtcsRTL,wi::to_wide (addr->step), pointer_mode);
   else
      st = NULL_RTX;

   if (addr->offset && !integer_zerop (addr->offset)){
      poly_offset_int dc = poly_offset_int::from (wi::to_poly_wide (addr->offset), SIGNED);
      off = mtcs_rtl_immed_wide_int_const/*!immed_wide_int_const*/ (mtcsRTL,dc, pointer_mode);
   }else
      off = NULL_RTX;

   if (!really_expand){
      unsigned int templ_index = TEMPL_IDX (as, addr->symbol, addr->base, addr->index, st, off);

      if (templ_index >= vec_safe_length (mem_addr_template_list))
         vec_safe_grow_cleared (mem_addr_template_list, templ_index + 1, true);

      /* Reuse the templates for addresses, so that we do not waste memory.  */
      templ = &(*mem_addr_template_list)[templ_index];
      if (!templ->ref){
         sym = (addr->symbol ? gen_rtx_SYMBOL_REF (pointer_mode, ggc_strdup ("test_symbol")) : NULL_RTX);
         bse = (addr->base ? mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,
               pointer_mode, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg)  + 1): NULL_RTX);
         idx = (addr->index ? mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,
               pointer_mode, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg)  + 2) : NULL_RTX);

         gen_addr_rtx(self,pointer_mode, sym, bse, idx,
                           st? const0_rtx : NULL_RTX,
                           off? const0_rtx : NULL_RTX,
                           &templ->ref,
                           &templ->step_p,
                           &templ->off_p);
      }

      if (st)
         *templ->step_p = st;
      if (off)
         *templ->off_p = off;

      return templ->ref;
   }

   /* Otherwise really expand the expressions.  */
   sym = (addr->symbol ? mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,
         addr->symbol, NULL_RTX, pointer_mode, EXPAND_NORMAL) : NULL_RTX);
   bse = (addr->base ? mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,
         addr->base, NULL_RTX, pointer_mode, EXPAND_NORMAL) : NULL_RTX);
   idx = (addr->index ? mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,
         addr->index, NULL_RTX, pointer_mode, EXPAND_NORMAL) : NULL_RTX);

   /* addr->base could be an SSA_NAME that was set to a constant value.  The
   call to expand_expr may expose that constant.  If so, fold the value
   into OFF and clear BSE.  Otherwise we may later try to pull a mode from
   BSE to generate a REG, which won't work with constants because they
   are modeless.  */
   if (bse && GET_CODE (bse) == CONST_INT){
      if (off)
         off =  mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, pointer_mode, bse, off);
      else
         off = bse;
      gcc_assert (GET_CODE (off) == CONST_INT);
      bse = NULL_RTX;
   }
   gen_addr_rtx(self,pointer_mode, sym, bse, idx, st, off, &address, NULL, NULL);
   if (pointer_mode != address_mode)
      address = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,address_mode, address);
   return address;
}

/* implement addr_for_mem_ref() directly from a tree, which avoids exporting
   the mem_address structure.  */
//原型 addr_for_mem_ref tree-ssa-address.h tree-ssa-address.cc
//重载函数
rtx mtcs_ssa_address_addr_for_mem_ref (MtcsSsaAddress *self,tree exp, addr_space_t as, bool really_expand)
{
   struct mem_address addr;
   mtcs_ssa_address_get_address_description/*!get_address_description*/(self,exp, &addr);
   return mtcs_ssa_address_addr_for_mem_ref/*!addr_for_mem_ref*/(self,&addr, as, really_expand);
}

/* Returns address of MEM_REF in TYPE.  */
//原型 tree_mem_ref_addr tree-ssa-address.h tree-ssa-address.cc

tree mtcs_ssa_address_tree_mem_ref_addr (MtcsSsaAddress *self,tree type, tree mem_ref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree addr;
   tree act_elem;
   tree step = TMR_STEP (mem_ref), offset = TMR_OFFSET (mem_ref);
   tree addr_base = NULL_TREE, addr_off = NULL_TREE;

   addr_base = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,type, TMR_BASE (mem_ref));

   act_elem = TMR_INDEX (mem_ref);
   if (act_elem){
      if (step)
         act_elem = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MULT_EXPR, TREE_TYPE (act_elem),
      act_elem, step);
      addr_off = act_elem;
   }

   act_elem = TMR_INDEX2 (mem_ref);
   if (act_elem){
      if (addr_off)
         addr_off = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, TREE_TYPE (addr_off),addr_off, act_elem);
      else
         addr_off = act_elem;
   }

   if (offset && !integer_zerop (offset)){
      if (addr_off)
         addr_off = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,
                  PLUS_EXPR, TREE_TYPE (addr_off), addr_off,mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,
                        TREE_TYPE (addr_off), offset));
      else
         addr_off = offset;
   }

   if (addr_off)
      addr = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,addr_base, addr_off);
   else
      addr = addr_base;

   return addr;
}

/* Returns true if a memory reference in MODE and with parameters given by
   ADDR is valid on the current target.  */
//原型 valid_mem_ref_p tree-ssa-address.h tree-ssa-address.cc
bool mtcs_ssa_address_valid_mem_ref_p (MtcsSsaAddress *self,machine_mode mode, addr_space_t as,
       struct mem_address *addr, code_helper ch)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   rtx address;
   address = mtcs_ssa_address_addr_for_mem_ref/*!addr_for_mem_ref*/(self,addr, as, false);
   if (!address)
      return false;

   return mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, address, as, ch);
}

/* Checks whether a TARGET_MEM_REF with type TYPE and parameters given by ADDR
   is valid on the current target and if so, creates and returns the
   TARGET_MEM_REF.  If VERIFY is false omit the verification step.  */
static tree create_mem_ref_raw (MtcsSsaAddress *self,tree type, tree alias_ptr_type, struct mem_address *addr,
          bool verify)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree base, index2;

   if (verify
   && !mtcs_ssa_address_valid_mem_ref_p/*!valid_mem_ref_p*/(self,TYPE_MODE (type), TYPE_ADDR_SPACE (type), addr))
      return NULL_TREE;

   if (addr->offset)
      addr->offset = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,alias_ptr_type, addr->offset);
   else
      addr->offset = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,alias_ptr_type, 0);

   if (addr->symbol){
      base = addr->symbol;
      index2 = addr->base;
   }else if (addr->base  && POINTER_TYPE_P (TREE_TYPE (addr->base))){
      base = addr->base;
      index2 = NULL_TREE;
   }else{
      base = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,type), 0);
      index2 = addr->base;
   }

   /* If possible use a plain MEM_REF instead of a TARGET_MEM_REF.
   ???  As IVOPTs does not follow restrictions to where the base
   pointer may point to create a MEM_REF only if we know that
   base is valid.  */
   if ((TREE_CODE (base) == ADDR_EXPR || TREE_CODE (base) == INTEGER_CST)
   && (!index2 || integer_zerop (index2))
   && (!addr->index || integer_zerop (addr->index)))
      return mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MEM_REF, type, base, addr->offset);

   return build5 (TARGET_MEM_REF, type,base, addr->offset, addr->index, addr->step, index2);
}

/* Returns true if OBJ is an object whose address is a link time constant.  */
static bool fixed_address_object_p (tree obj)
{
   return (VAR_P (obj)
      && (TREE_STATIC (obj) || DECL_EXTERNAL (obj))
      && ! DECL_DLLIMPORT_P (obj));
}

/* If ADDR contains an address of object that is a link time constant,
   move it to PARTS->symbol.  */
//原型 move_fixed_address_to_symbol tree-ssa-address.h tree-ssa-address.cc
void mtcs_ssa_address_move_fixed_address_to_symbol (MtcsSsaAddress *self,struct mem_address *parts, aff_tree *addr)
{
   unsigned i;
   tree val = NULL_TREE;

   for (i = 0; i < addr->n; i++){
      if (addr->elts[i].coef != 1)
         continue;

      val = addr->elts[i].val;
      if (TREE_CODE (val) == ADDR_EXPR  && fixed_address_object_p (TREE_OPERAND (val, 0)))
         break;
   }

   if (i == addr->n)
      return;

   parts->symbol = val;
   aff_combination_remove_elt (addr, i);
}

/* Return true if ADDR contains an instance of BASE_HINT and it's moved to
   PARTS->base.  */

static bool move_hint_to_base (MtcsSsaAddress *self,tree type, struct mem_address *parts, tree base_hint,
         aff_tree *addr)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   unsigned i;
   tree val = NULL_TREE;
   int qual;

   for (i = 0; i < addr->n; i++){
      if (addr->elts[i].coef != 1)
         continue;

      val = addr->elts[i].val;
      if (operand_equal_p (val, base_hint, 0))
         break;
   }

   if (i == addr->n)
      return false;

   /* Cast value to appropriate pointer type.  We cannot use a pointer
   to TYPE directly, as the back-end will assume registers of pointer
   type are aligned, and just the base itself may not actually be.
   We use void pointer to the type's address space instead.  */
   qual = ENCODE_QUAL_ADDR_SPACE (TYPE_ADDR_SPACE (type));
   type = mtcs_tree_build_qualified_type/*!build_qualified_type*/(mtcsTree,void_type_node, qual);
   parts->base = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,
         mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,type), val);
   aff_combination_remove_elt (addr, i);
   return true;
}

/* If ADDR contains an address of a dereferenced pointer, move it to
   PARTS->base.  */
static void move_pointer_to_base (MtcsSsaAddress *self, struct mem_address *parts, aff_tree *addr)
{
   unsigned i;
   tree val = NULL_TREE;
   for (i = 0; i < addr->n; i++){
      if (addr->elts[i].coef != 1)
         continue;
      val = addr->elts[i].val;
      if (POINTER_TYPE_P (TREE_TYPE (val)))
         break;
   }
   if (i == addr->n)
      return;
   parts->base = val;
   aff_combination_remove_elt (addr, i);
}

/* Moves the loop variant part V in linear address ADDR to be the index
   of PARTS.  */
static void move_variant_to_index (MtcsSsaAddress *self,struct mem_address *parts, aff_tree *addr, tree v)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   unsigned i;
   tree val = NULL_TREE;
   gcc_assert (!parts->index);
   for (i = 0; i < addr->n; i++){
      val = addr->elts[i].val;
      if (operand_equal_p (val, v, 0))
         break;
   }

   if (i == addr->n)
      return;

   parts->index = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,sizetype, val);
   parts->step = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,sizetype, addr->elts[i].coef);
   aff_combination_remove_elt (addr, i);
}

/* Adds ELT to PARTS.  */
static void add_to_parts (MtcsSsaAddress *self,struct mem_address *parts, tree elt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree type;

   if (!parts->index){
      parts->index = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,sizetype, elt);
      return;
   }

   if (!parts->base){
      parts->base = elt;
      return;
   }

   /* Add ELT to base.  */
   type = TREE_TYPE (parts->base);
   if (POINTER_TYPE_P (type))
      parts->base = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,parts->base, elt);
   else
      parts->base = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,PLUS_EXPR, type, parts->base, elt);
}

/* Returns true if multiplying by RATIO is allowed in an address.  Test the
   validity for a memory reference accessing memory of mode MODE in address
   space AS.  */
static bool multiplier_allowed_in_address_p (MtcsSsaAddress *self,HOST_WIDE_INT ratio, machine_mode mode,
             addr_space_t as)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   #define MAX_RATIO 128
   unsigned int data_index = (int) as * mtcs_mode_get_max_number/*!MAX_MACHINE_MODE*/(mtcsMode) + (int) mode;
   static vec<sbitmap> valid_mult_list;
   sbitmap valid_mult;

   if (data_index >= valid_mult_list.length ())
      valid_mult_list.safe_grow_cleared (data_index + 1, true);

   valid_mult = valid_mult_list[data_index];
   if (!valid_mult){
      machine_mode address_mode = targetm.addr_space.address_mode (as);
      rtx reg1 = mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,address_mode,
            mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg)  + 1);
      rtx reg2 = mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,address_mode,
            mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg)  + 2);
      rtx addr, scaled;
      HOST_WIDE_INT i;

      valid_mult = sbitmap_alloc (2 * MAX_RATIO + 1);
      bitmap_clear (valid_mult);
      scaled = gen_rtx_fmt_ee (MULT, address_mode, reg1, NULL_RTX);
      addr = gen_rtx_fmt_ee (PLUS, address_mode, scaled, reg2);
      for (i = -MAX_RATIO; i <= MAX_RATIO; i++){
         XEXP (scaled, 1) = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,i, address_mode);
         if (mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, addr, as)
         || mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsRecog,mode, scaled, as))
            bitmap_set_bit (valid_mult, i + MAX_RATIO);
      }

      if (dump_file && (dump_flags & TDF_DETAILS)){
         fprintf (dump_file, "  allowed multipliers:");
         for (i = -MAX_RATIO; i <= MAX_RATIO; i++)
            if (bitmap_bit_p (valid_mult, i + MAX_RATIO))
               fprintf (dump_file, " %d", (int) i);
         fprintf (dump_file, "\n");
         fprintf (dump_file, "\n");
      }

      valid_mult_list[data_index] = valid_mult;
   }

   if (ratio > MAX_RATIO || ratio < -MAX_RATIO)
      return false;

   return bitmap_bit_p (valid_mult, ratio + MAX_RATIO);
}

/* Finds the most expensive multiplication in ADDR that can be
   expressed in an addressing mode and move the corresponding
   element(s) to PARTS.  */
static void most_expensive_mult_to_index (MtcsSsaAddress *self,tree type, struct mem_address *parts,
               aff_tree *addr, bool speed)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   addr_space_t as = TYPE_ADDR_SPACE (type);
   machine_mode address_mode =  target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
   HOST_WIDE_INT coef;
   unsigned best_mult_cost = 0, acost;
   tree mult_elt = NULL_TREE, elt;
   unsigned i, j;
   enum tree_code op_code;

   offset_int best_mult = 0;
   for (i = 0; i < addr->n; i++){
      if (!wi::fits_shwi_p (addr->elts[i].coef))
         continue;

      coef = addr->elts[i].coef.to_shwi ();
      if (coef == 1 || !multiplier_allowed_in_address_p(self,coef, TYPE_MODE (type), as))
         continue;

      acost = mult_by_coeff_cost (coef, address_mode, speed);

      if (acost > best_mult_cost){
         best_mult_cost = acost;
         best_mult = offset_int::from (addr->elts[i].coef, SIGNED);
      }
   }

   if (!best_mult_cost)
      return;

   /* Collect elements multiplied by best_mult.  */
   for (i = j = 0; i < addr->n; i++){
      offset_int amult = offset_int::from (addr->elts[i].coef, SIGNED);
      offset_int amult_neg = -wi::sext (amult, TYPE_PRECISION (addr->type));

      if (amult == best_mult)
         op_code = PLUS_EXPR;
      else if (amult_neg == best_mult)
         op_code = MINUS_EXPR;
      else{
         addr->elts[j] = addr->elts[i];
         j++;
         continue;
      }

      elt = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,sizetype, addr->elts[i].val);
      if (mult_elt)
         mult_elt =mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,op_code, sizetype, mult_elt, elt);
      else if (op_code == PLUS_EXPR)
         mult_elt = elt;
      else
         mult_elt = fold_build1 (NEGATE_EXPR, sizetype, elt);
   }
   addr->n = j;

   parts->index = mult_elt;
   parts->step = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,sizetype, best_mult);
}

/* Splits address ADDR for a memory access of type TYPE into PARTS.
   If BASE_HINT is non-NULL, it specifies an SSA name to be used
   preferentially as base of the reference, and IV_CAND is the selected
   iv candidate used in ADDR.  Store true to VAR_IN_BASE if variant
   part of address is split to PARTS.base.

   TODO -- be more clever about the distribution of the elements of ADDR
   to PARTS.  Some architectures do not support anything but single
   register in address, possibly with a small integer offset; while
   create_mem_ref will simplify the address to an acceptable shape
   later, it would be more efficient to know that asking for complicated
   addressing modes is useless.  */
static void addr_to_parts (MtcsSsaAddress *self,tree type, aff_tree *addr, tree iv_cand, tree base_hint,
          struct mem_address *parts, bool *var_in_base, bool speed)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree part;
   unsigned i;

   parts->symbol = NULL_TREE;
   parts->base = NULL_TREE;
   parts->index = NULL_TREE;
   parts->step = NULL_TREE;

   if (maybe_ne (addr->offset, 0))
      parts->offset = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,sizetype, addr->offset);
   else
      parts->offset = NULL_TREE;

   /* Try to find a symbol.  */
   mtcs_ssa_address_move_fixed_address_to_symbol/*!move_fixed_address_to_symbol*/(self,parts, addr);

   /* Since at the moment there is no reliable way to know how to
   distinguish between pointer and its offset, we decide if var
   part is the pointer based on guess.  */
   *var_in_base = (base_hint != NULL && parts->symbol == NULL);
   if (*var_in_base)
      *var_in_base = move_hint_to_base(self,type, parts, base_hint, addr);
   else
      move_variant_to_index(self,parts, addr, iv_cand);

   /* First move the most expensive feasible multiplication to index.  */
   if (!parts->index)
      most_expensive_mult_to_index(self,type, parts, addr, speed);

   /* Move pointer into base.  */
   if (!parts->symbol && !parts->base)
      move_pointer_to_base(self,parts, addr);

   /* Then try to process the remaining elements.  */
   for (i = 0; i < addr->n; i++){
      part = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,sizetype, addr->elts[i].val);
      if (addr->elts[i].coef != 1)
         part = mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MULT_EXPR, sizetype, part,
               mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,sizetype, addr->elts[i].coef));
      add_to_parts(self,parts, part);
   }
   if (addr->rest)
      add_to_parts(self,parts, mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,sizetype, addr->rest));
}

/* Force the PARTS to register.  */
static void gimplify_mem_ref_parts (MtcsSsaAddress *self,gimple_stmt_iterator *gsi, struct mem_address *parts)
{
   if (parts->base)
      parts->base = force_gimple_operand_gsi_1 (gsi, parts->base,
                           is_gimple_mem_ref_addr, NULL_TREE,
                           true, GSI_SAME_STMT);
   if (parts->index)
      parts->index = force_gimple_operand_gsi (gsi, parts->index,
                              true, NULL_TREE,
                              true, GSI_SAME_STMT);
}

/* Return true if the OFFSET in PARTS is the only thing that is making
   it an invalid address for type TYPE.  */
static bool mem_ref_valid_without_offset_p (MtcsSsaAddress *self,tree type, mem_address parts)
{
   if (!parts.base)
      parts.base = parts.offset;
   parts.offset = NULL_TREE;
   return mtcs_ssa_address_valid_mem_ref_p/*!valid_mem_ref_p*/(self,TYPE_MODE (type), TYPE_ADDR_SPACE (type), &parts);
}

/* Fold PARTS->offset into PARTS->base, so that there is no longer
   a separate offset.  Emit any new instructions before GSI.  */
static void add_offset_to_base (MtcsSsaAddress *self,gimple_stmt_iterator *gsi, mem_address *parts)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree tmp = parts->offset;
   if (parts->base){
      tmp = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,parts->base, tmp);
      tmp = force_gimple_operand_gsi_1 (gsi, tmp, is_gimple_mem_ref_addr,NULL_TREE, true, GSI_SAME_STMT);
   }
   parts->base = tmp;
   parts->offset = NULL_TREE;
}

/* Creates and returns a TARGET_MEM_REF for address ADDR.  If necessary
   computations are emitted in front of GSI.  TYPE is the mode
   of created memory reference. IV_CAND is the selected iv candidate in ADDR,
   and BASE_HINT is non NULL if IV_CAND comes from a base address
   object.  */
//原型 create_mem_ref tree-ssa-address.h tree-ssa-address.cc
tree mtcs_ssa_address_create_mem_ref (MtcsSsaAddress *self, gimple_stmt_iterator *gsi, tree type, aff_tree *addr,
      tree alias_ptr_type, tree iv_cand, tree base_hint, bool speed)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   bool var_in_base;
   tree mem_ref, tmp;
   struct mem_address parts;

   addr_to_parts(self,type, addr, iv_cand, base_hint, &parts, &var_in_base, speed);
   gimplify_mem_ref_parts(self,gsi, &parts);
   mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
   if (mem_ref)
      return mem_ref;

   /* The expression is too complicated.  Try making it simpler.  */

   /* Merge symbol into other parts.  */
   if (parts.symbol){
      tmp = parts.symbol;
      parts.symbol = NULL_TREE;
      gcc_assert (is_gimple_val (tmp));

      if (parts.base){
         gcc_assert (useless_type_conversion_p (sizetype, TREE_TYPE (parts.base)));

         if (parts.index){
            /* Add the symbol to base, eventually forcing it to register.  */
            tmp = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,tmp, parts.base);
            tmp = force_gimple_operand_gsi_1 (gsi, tmp, is_gimple_mem_ref_addr,NULL_TREE, true, GSI_SAME_STMT);
         }else{
            /* Move base to index, then move the symbol to base.  */
            parts.index = parts.base;
         }
         parts.base = tmp;
      }else
         parts.base = tmp;

      mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
      if (mem_ref)
         return mem_ref;
   }

   /* Move multiplication to index by transforming address expression:
   [... + index << step + ...]
   into:
   index' = index << step;
   [... + index' + ,,,].  */
   if (parts.step && !integer_onep (parts.step)){
      gcc_assert (parts.index);
      if (parts.offset && mem_ref_valid_without_offset_p(self,type, parts)){
         add_offset_to_base(self,gsi, &parts);
         mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
         gcc_assert (mem_ref);
         return mem_ref;
      }

      parts.index = force_gimple_operand_gsi (gsi,
      mtcs_const_fold_build2/*!fold_build2*/(mtcsConst,MULT_EXPR, sizetype,parts.index, parts.step),
      true, NULL_TREE, true, GSI_SAME_STMT);
      parts.step = NULL_TREE;

      mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
      if (mem_ref)
         return mem_ref;
   }

   /* Add offset to invariant part by transforming address expression:
   [base + index + offset]
   into:
   base' = base + offset;
   [base' + index]
   or:
   index' = index + offset;
   [base + index']
   depending on which one is invariant.  */
   if (parts.offset && !integer_zerop (parts.offset)){
      tree old_base = unshare_expr (parts.base);
      tree old_index = unshare_expr (parts.index);
      tree old_offset = unshare_expr (parts.offset);

      tmp = parts.offset;
      parts.offset = NULL_TREE;
      /* Add offset to invariant part.  */
      if (!var_in_base){
         if (parts.base){
            tmp = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,parts.base, tmp);
            tmp = force_gimple_operand_gsi_1 (gsi, tmp,is_gimple_mem_ref_addr,NULL_TREE, true,GSI_SAME_STMT);
         }
         parts.base = tmp;
      }else{
         if (parts.index){
            tmp = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,parts.index, tmp);
            tmp = force_gimple_operand_gsi_1 (gsi, tmp,is_gimple_mem_ref_addr,NULL_TREE, true,GSI_SAME_STMT);
         }
         parts.index = tmp;
      }

      mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
      if (mem_ref)
         return mem_ref;

      /* Restore parts.base, index and offset so that we can check if
      [base + offset] addressing mode is supported in next step.
      This is necessary for targets only support [base + offset],
      but not [base + index] addressing mode.  */
      parts.base = old_base;
      parts.index = old_index;
      parts.offset = old_offset;
   }

   /* Transform [base + index + ...] into:
   base' = base + index;
   [base' + ...].  */
   if (parts.index){
      tmp = parts.index;
      parts.index = NULL_TREE;
      /* Add index to base.  */
      if (parts.base){
         tmp = mtcs_const_build_pointer_plus/*!fold_build_pointer_plus*/(mtcsConst,parts.base, tmp);
         tmp = force_gimple_operand_gsi_1 (gsi, tmp,is_gimple_mem_ref_addr,NULL_TREE, true, GSI_SAME_STMT);
      }
      parts.base = tmp;

      mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
      if (mem_ref)
         return mem_ref;
   }

   /* Transform [base + offset] into:
   base' = base + offset;
   [base'].  */
   if (parts.offset && !integer_zerop (parts.offset)){
      add_offset_to_base(self,gsi, &parts);
      mem_ref = create_mem_ref_raw(self,type, alias_ptr_type, &parts, true);
      if (mem_ref)
         return mem_ref;
   }

   /* Verify that the address is in the simplest possible shape
   (only a register).  If we cannot create such a memory reference,
   something is really wrong.  */
   gcc_assert (parts.symbol == NULL_TREE);
   gcc_assert (parts.index == NULL_TREE);
   gcc_assert (!parts.step || integer_onep (parts.step));
   gcc_assert (!parts.offset || integer_zerop (parts.offset));
   gcc_unreachable ();
}

/* Copies components of the address from OP to ADDR.  */
//原型 get_address_description tree-ssa-address.h tree-ssa-address.cc
void mtcs_ssa_address_get_address_description (MtcsSsaAddress *self,tree op, struct mem_address *addr)
{
   if (TREE_CODE (TMR_BASE (op)) == ADDR_EXPR){
      addr->symbol = TMR_BASE (op);
      addr->base = TMR_INDEX2 (op);
   }else{
      addr->symbol = NULL_TREE;
      if (TMR_INDEX2 (op)){
         gcc_assert (integer_zerop (TMR_BASE (op)));
         addr->base = TMR_INDEX2 (op);
      }else
         addr->base = TMR_BASE (op);
   }
   addr->index = TMR_INDEX (op);
   addr->step = TMR_STEP (op);
   addr->offset = TMR_OFFSET (op);
}

/* Copies the reference information from OLD_REF to NEW_REF, where
   NEW_REF should be either a MEM_REF or a TARGET_MEM_REF.  */
//原型 copy_ref_info tree-ssa-address.h tree-ssa-address.cc
void mtcs_ssa_address_copy_ref_info (MtcsSsaAddress *self, tree new_ref, tree old_ref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsBuiltins *mtcsBuiltins=mtcs_target_get_builtins(mtcsTarget);

   tree new_ptr_base = NULL_TREE;

   gcc_assert (TREE_CODE (new_ref) == MEM_REF || TREE_CODE (new_ref) == TARGET_MEM_REF);

   TREE_SIDE_EFFECTS (new_ref) = TREE_SIDE_EFFECTS (old_ref);
   TREE_THIS_VOLATILE (new_ref) = TREE_THIS_VOLATILE (old_ref);

   new_ptr_base = TREE_OPERAND (new_ref, 0);

   tree base = get_base_address (old_ref);
   if (!base)
      return;

   /* We can transfer points-to information from an old pointer
   or decl base to the new one.  */
   if (new_ptr_base
   && TREE_CODE (new_ptr_base) == SSA_NAME
   && !SSA_NAME_PTR_INFO (new_ptr_base)){
      if ((TREE_CODE (base) == MEM_REF
      || TREE_CODE (base) == TARGET_MEM_REF)
      && TREE_CODE (TREE_OPERAND (base, 0)) == SSA_NAME
      && SSA_NAME_PTR_INFO (TREE_OPERAND (base, 0))){
      duplicate_ssa_name_ptr_info (new_ptr_base, SSA_NAME_PTR_INFO (TREE_OPERAND (base, 0)));
         reset_flow_sensitive_info (new_ptr_base);
      }else if (VAR_P (base) || TREE_CODE (base) == PARM_DECL || TREE_CODE (base) == RESULT_DECL){
         struct ptr_info_def *pi = get_ptr_info (new_ptr_base);
         pt_solution_set_var (&pi->pt, base);
      }
   }

   /* We can transfer dependence info.  */
   if (!MR_DEPENDENCE_CLIQUE (new_ref) && (TREE_CODE (base) == MEM_REF
   || TREE_CODE (base) == TARGET_MEM_REF)  && MR_DEPENDENCE_CLIQUE (base)){
      MR_DEPENDENCE_CLIQUE (new_ref) = MR_DEPENDENCE_CLIQUE (base);
      MR_DEPENDENCE_BASE (new_ref) = MR_DEPENDENCE_BASE (base);
   }

   /* And alignment info.  Note we cannot transfer misalignment info
   since that sits on the SSA name but this is flow-sensitive info
   which we cannot transfer in this generic routine.  */
   unsigned old_align = mtcs_builtins_get_object_alignment/*!get_object_alignment*/(mtcsBuiltins,old_ref);
   unsigned new_align = mtcs_builtins_get_object_alignment/*!get_object_alignment*/(mtcsBuiltins,new_ref);
   if (new_align < old_align)
      TREE_TYPE (new_ref) = build_aligned_type (TREE_TYPE (new_ref), old_align);
}

/* Move constants in target_mem_ref REF to offset.  Returns the new target
   mem ref if anything changes, NULL_TREE otherwise.  */
//原型 maybe_fold_tmr tree-ssa-address.h tree-ssa-address.cc
tree  mtcs_ssa_address_maybe_fold_tmr (MtcsSsaAddress *self,tree ref)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst  *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsDfa   *mtcsDfa=mtcs_target_get_dfa(mtcsTarget);

   struct mem_address addr;
   bool changed = false;
   tree new_ref, off;

   mtcs_ssa_address_get_address_description/*!get_address_description*/(self,ref, &addr);

   if (addr.base
   && TREE_CODE (addr.base) == INTEGER_CST
   && !integer_zerop (addr.base)){
      addr.offset = mtcs_const_fold_binary_to_constant/*!fold_binary_to_constant*/(mtcsConst,PLUS_EXPR,
               TREE_TYPE (addr.offset),addr.offset, addr.base);
      addr.base = NULL_TREE;
      changed = true;
   }

   if (addr.symbol && TREE_CODE (TREE_OPERAND (addr.symbol, 0)) == MEM_REF){
      addr.offset = mtcs_const_fold_binary_to_constant/*!fold_binary_to_constant*/(mtcsConst,
            PLUS_EXPR, TREE_TYPE (addr.offset),addr.offset,TREE_OPERAND (TREE_OPERAND (addr.symbol, 0), 1));
      addr.symbol = TREE_OPERAND (TREE_OPERAND (addr.symbol, 0), 0);
      changed = true;
   }else if (addr.symbol && handled_component_p (TREE_OPERAND (addr.symbol, 0))){
      poly_int64 offset;
      addr.symbol =mtcs_const_build_fold_addr_expr/*!build_fold_addr_expr*/(mtcsConst,
            mtcs_dfa_get_addr_base_and_unit_offset/*!get_addr_base_and_unit_offset*/(mtcsDfa,TREE_OPERAND (addr.symbol, 0), &offset));
      addr.offset = mtcs_const_int_const_binop/*!int_const_binop*/(mtcsConst,PLUS_EXPR,addr.offset, size_int (offset));
      changed = true;
   }

   if (addr.index && TREE_CODE (addr.index) == INTEGER_CST){
      off = addr.index;
      if (addr.step){
         off = mtcs_const_fold_binary_to_constant/*!fold_binary_to_constant*/(mtcsConst,MULT_EXPR, sizetype,off, addr.step);
         addr.step = NULL_TREE;
      }
      addr.offset = mtcs_const_fold_binary_to_constant/*!fold_binary_to_constant*/(mtcsConst,
            PLUS_EXPR,TREE_TYPE (addr.offset),addr.offset, off);
      addr.index = NULL_TREE;
      changed = true;
   }

   if (!changed)
      return NULL_TREE;

   /* If we have propagated something into this TARGET_MEM_REF and thus
   ended up folding it, always create a new TARGET_MEM_REF regardless
   if it is valid in this for on the target - the propagation result
   wouldn't be anyway.  */
   new_ref = create_mem_ref_raw(self,TREE_TYPE (ref),TREE_TYPE (addr.offset), &addr, false);
   TREE_SIDE_EFFECTS (new_ref) = TREE_SIDE_EFFECTS (ref);
   TREE_THIS_VOLATILE (new_ref) = TREE_THIS_VOLATILE (ref);
   return new_ref;
}

/* Return the preferred index scale factor for accessing memory of mode
   MEM_MODE in the address space of pointer BASE.  Assume that we're
   optimizing for speed if SPEED is true and for size otherwise.  */
//原型 preferred_mem_scale_factor tree-ssa-address.h tree-ssa-address.cc
unsigned int mtcs_ssa_address_preferred_mem_scale_factor (MtcsSsaAddress *self,tree base, machine_mode mem_mode,
             bool speed)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlanal  *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   /* For BLKmode, we can't do anything so return 1.  */
   if (mem_mode == mtcsMode->modes.M_BLKmode)
      return 1;

   struct mem_address parts = {};
   addr_space_t as = TYPE_ADDR_SPACE (TREE_TYPE (base));
   unsigned int fact = mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,mem_mode);

   /* Addressing mode "base + index".  */
   parts.index = integer_one_node;
   parts.base = integer_one_node;
   rtx addr = mtcs_ssa_address_addr_for_mem_ref/*!addr_for_mem_ref*/(self,&parts, as, false);
   unsigned cost = mtcs_rtlanal_address_cost/*!address_cost*/(mtcsRtlanal,addr, mem_mode, as, speed);

   /* Addressing mode "base + index << scale".  */
   parts.step = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,sizetype, fact);
   addr = mtcs_ssa_address_addr_for_mem_ref/*!addr_for_mem_ref*/(self,&parts, as, false);
   unsigned new_cost = mtcs_rtlanal_address_cost/*!address_cost*/(mtcsRtlanal,addr, mem_mode, as, speed);

   /* Compare the cost of an address with an unscaled index with
   a scaled index and return factor if useful. */
   if (new_cost < cost)
      return mtcs_mode_get_unit_size/*!GET_MODE_UNIT_SIZE*/(mtcsMode,mem_mode);
   return 1;
}

/* Dump PARTS to FILE.  */
void mtcs_ssa_address_dump_mem_address (FILE *file, struct mem_address *parts)
{
   if (parts->symbol){
      fprintf (file, "symbol: ");
      print_generic_expr (file, TREE_OPERAND (parts->symbol, 0), TDF_SLIM);
      fprintf (file, "\n");
   }
   if (parts->base){
      fprintf (file, "base: ");
      print_generic_expr (file, parts->base, TDF_SLIM);
      fprintf (file, "\n");
   }
   if (parts->index){
      fprintf (file, "index: ");
      print_generic_expr (file, parts->index, TDF_SLIM);
      fprintf (file, "\n");
   }
   if (parts->step){
      fprintf (file, "step: ");
      print_generic_expr (file, parts->step, TDF_SLIM);
      fprintf (file, "\n");
   }
   if (parts->offset){
      fprintf (file, "offset: ");
      print_generic_expr (file, parts->offset, TDF_SLIM);
      fprintf (file, "\n");
   }
}

static void mtcsMtcsSsaAddressInit(MtcsSsaAddress *self)
{

}

MtcsSsaAddress *mtcs_ssa_address_new(MtcsMode *mtcsMode)
{
   MtcsSsaAddress *self = n_slice_alloc0 (sizeof(MtcsSsaAddress));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsMtcsSsaAddressInit(self);
     return self;
}
