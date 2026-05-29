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
 * base on dfa.cc
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

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "aet/aetprinttree.h"
#include "mtcsdfa.h"
#include "mtcstarget.h"

static void mtcsDfaInit(MtcsDfa *self)
{

}

/* Returns the base object and a constant BITS_PER_UNIT offset in *POFFSET that
   denotes the starting address of the memory access EXP.
   Returns NULL_TREE if the offset is not constant or any component
   is not BITS_PER_UNIT-aligned.
   VALUEIZE if non-NULL is used to valueize SSA names.  It should return
   its argument or a constant if the argument is known to be constant.  */
//原型 get_addr_base_and_unit_offset_1 tree-dfa.h tree-dfa.cc
tree mtcs_dfa_get_addr_base_and_unit_offset_1 (MtcsDfa *self,tree exp, poly_int64 *poffset,
             tree (*valueize) (tree))
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree   *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   poly_int64 byte_offset = 0;

   /* Compute cumulative byte-offset for nested component-refs and array-refs,
   and find the ultimate containing object.  */
   while (1){
      switch (TREE_CODE (exp)){
         case BIT_FIELD_REF:
         {
            poly_int64 this_byte_offset;
            poly_uint64 this_bit_offset;
            if (!poly_int_tree_p (TREE_OPERAND (exp, 2), &this_bit_offset)
            || !multiple_p (this_bit_offset, BITS_PER_UNIT, &this_byte_offset))
               return NULL_TREE;
            byte_offset += this_byte_offset;
         }
            break;

         case COMPONENT_REF:
         {
            tree field = TREE_OPERAND (exp, 1);
            tree this_offset = mtcs_tree_component_ref_field_offset/*!component_ref_field_offset*/(mtcsTree,exp);
            poly_int64 hthis_offset;

            if (!this_offset || !poly_int_tree_p (this_offset, &hthis_offset)
            || (TREE_INT_CST_LOW (DECL_FIELD_BIT_OFFSET (field)) % BITS_PER_UNIT))
               return NULL_TREE;

            hthis_offset += (TREE_INT_CST_LOW (DECL_FIELD_BIT_OFFSET (field))/ BITS_PER_UNIT);
            byte_offset += hthis_offset;
         }
            break;

         case ARRAY_REF:
         case ARRAY_RANGE_REF:
         {
            tree index = TREE_OPERAND (exp, 1);
            tree low_bound, unit_size;

            if (valueize  && TREE_CODE (index) == SSA_NAME)
               index = (*valueize) (index);
            if (!poly_int_tree_p (index))
               return NULL_TREE;
            low_bound = mtcs_tree_array_ref_low_bound/*!array_ref_low_bound*/(mtcsTree,exp);
            if (valueize   && TREE_CODE (low_bound) == SSA_NAME)
               low_bound = (*valueize) (low_bound);
            if (!poly_int_tree_p (low_bound))
               return NULL_TREE;
            unit_size = mtcs_tree_array_ref_element_size/*!array_ref_element_size*/(mtcsTree,exp);
            if (TREE_CODE (unit_size) != INTEGER_CST)
               return NULL_TREE;

            /* If the resulting bit-offset is constant, track it.  */
            poly_offset_int woffset   = wi::sext (wi::to_poly_offset (index) - wi::to_poly_offset (low_bound),
                  TYPE_PRECISION (sizetype));
            woffset *= wi::to_offset (unit_size);
            byte_offset += woffset.force_shwi ();
         }
            break;

         case REALPART_EXPR:
            break;

         case IMAGPART_EXPR:
            byte_offset += TREE_INT_CST_LOW (TYPE_SIZE_UNIT (TREE_TYPE (exp)));
            break;

         case VIEW_CONVERT_EXPR:
            break;

         case MEM_REF:
         {
            tree base = TREE_OPERAND (exp, 0);
            if (valueize   && TREE_CODE (base) == SSA_NAME)
               base = (*valueize) (base);

            /* Hand back the decl for MEM[&decl, off].  */
            if (TREE_CODE (base) == ADDR_EXPR){
               if (!integer_zerop (TREE_OPERAND (exp, 1))){
                  poly_offset_int off = mem_ref_offset (exp);
                  byte_offset += off.force_shwi ();
               }
               exp = TREE_OPERAND (base, 0);
            }
            goto done;
         }

         case TARGET_MEM_REF:
         {
            tree base = TREE_OPERAND (exp, 0);
            if (valueize && TREE_CODE (base) == SSA_NAME)
               base = (*valueize) (base);

            /* Hand back the decl for MEM[&decl, off].  */
            if (TREE_CODE (base) == ADDR_EXPR){
               if (TMR_INDEX (exp) || TMR_INDEX2 (exp))
                  return NULL_TREE;
               if (!integer_zerop (TMR_OFFSET (exp))){
                  poly_offset_int off = mem_ref_offset (exp);
                  byte_offset += off.force_shwi ();
               }
               exp = TREE_OPERAND (base, 0);
            }
            goto done;
         }

         default:
            goto done;
      }
      exp = TREE_OPERAND (exp, 0);
   }//end while(1)

done:
   *poffset = byte_offset;
   return exp;
}

/* Returns the base object and a constant BITS_PER_UNIT offset in *POFFSET that
   denotes the starting address of the memory access EXP.
   Returns NULL_TREE if the offset is not constant or any component
   is not BITS_PER_UNIT-aligned.  */
//原型 get_addr_base_and_unit_offset tree-dfa.h tree-dfa.cc
tree mtcs_dfa_get_addr_base_and_unit_offset (MtcsDfa *self,tree exp, poly_int64 *poffset)
{
  return mtcs_dfa_get_addr_base_and_unit_offset_1/*!get_addr_base_and_unit_offset_1*/(self,exp, poffset, NULL);
}

MtcsDfa *mtcs_dfa_new(MtcsMode *mtcsMode)
{
   MtcsDfa *self = n_slice_alloc0 (sizeof(MtcsDfa));
   mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
   mtcsDfaInit(self);
   return self;
}


