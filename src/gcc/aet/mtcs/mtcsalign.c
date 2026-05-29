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

#include "mtcsalign.h"
#include "mtcstarget.h"

//原型 LOCAL_DECL_ALIGNMENT defaults.h
static nuint   getLocalDeclAlignment_cb(MtcsAlign *self,tree decl)
{
   return mtcs_align_get_local_alignment(self, TREE_TYPE (decl),DECL_ALIGN (decl));
}

static nuint  getStackSlotAlignment_cb(MtcsAlign *self,tree type,machine_mode mode,nuint alignment)
{
   return  type ? mtcs_align_get_local_alignment (self,type,alignment) :alignment;
}

//原型 LOCAL_ALIGNMENT default.h #define LOCAL_ALIGNMENT(TYPE, ALIGNMENT) ALIGNMENT

static nuint   getLocalAlignment_cb(MtcsAlign *self,tree type,nuint alignment)
{
   return  alignment;
}

void         mtcs_align_init(MtcsAlign *self)
{
    self->get_local_decl_alignment=getLocalDeclAlignment_cb;
    self->get_local_alignment=getLocalAlignment_cb;
    self->get_stack_slot_alignment=getStackSlotAlignment_cb;
}

//原型 LOCAL_ALIGNMENT defaults.h
nuint  mtcs_align_get_local_alignment(MtcsAlign *self,tree type,nuint alignment)
{
    return self->get_local_alignment(self,type,alignment);
}

//原型 LOCAL_DECL_ALIGNMENT defaults.h
nuint  mtcs_align_get_local_decl_alignment(MtcsAlign *self,tree decl)
{
    return self->get_local_decl_alignment(self,decl);
}

nuint  mtcs_align_get_stack_slot_alignment(MtcsAlign *self,tree type,machine_mode mode,nuint alignment)
{
    return self->get_stack_slot_alignment(self,type,mode,alignment);
}

//原型 parse_alignment_opts toplev.h toplev.cc
void       mtcs_align_parse_alignment_opts (MtcsAlign *self)
{
    self->parse_alignment_opts(self);
}

//原型 BIGGEST_ALIGNMENT default.h
nuint mtcs_align_get_biggest_alignment(MtcsAlign *self)
{
    return self->biggestAlignment;
}

void        mtcs_align_set_biggest_alignment(MtcsAlign *self,nuint biggestAlignment)
{
    self->biggestAlignment=biggestAlignment;
}


//原型 STRICT_ALIGNMENT
void        mtcs_align_set_strict_alignment(MtcsAlign *self,nuint strictAlignment)
{
    self->strictAlignmemt=strictAlignment;
}

nuint       mtcs_align_get_strict_alignment(MtcsAlign *self)
{
    return self->strictAlignmemt;
}

//原型 #define TRAMPOLINE_ALIGNMENT FUNCTION_ALIGNMENT (FUNCTION_BOUNDARY) nvptx 256 第个平台定义 defaults.h
nuint   mtcs_align_get_trampoline_alignment (MtcsAlign *self)
{
   return self->trampolineAlignment;
}
void       mtcs_align_set_trampoline_alignment(MtcsAlign *self,nuint value)
{
   self->trampolineAlignment=value;
}

//原型 #define TRAMPOLINE_SIZE
nuint       mtcs_align_get_trampoline_size (MtcsAlign *self)
{
   return self->trampolineSize;

}
void       mtcs_align_set_trampoline_size(MtcsAlign *self,nuint value)
{
   self->trampolineSize=value;
}

//原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
HOST_WIDE_INT mtcs_align_get_vector_alignment (MtcsAlign *self,const_tree type)
{
   return self->get_vector_alignment(self,type);
}

/* Force minimum alignment to be able to use the least significant bits
   for distinguishing descriptor addresses from code addresses.  */
//原型
//#define FUNCTION_ALIGNMENT(ALIGN)               \
//  (lang_hooks.custom_function_descriptors          \
//   && targetm.calls.custom_function_descriptors > 0         \
//   ? MAX ((ALIGN),                  \
//     2 * targetm.calls.custom_function_descriptors * BITS_PER_UNIT)\
//   : (ALIGN))
//default.h
int mtcs_align_get_function_alignment(MtcsAlign *self,int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   if(mtcsLang->custom_function_descriptors
         && mtcsMachine->calls->custom_function_descriptors/*!targetm.calls.custom_function_descriptors*/>0){
      return MAX(align,2*mtcsMachine->calls->custom_function_descriptors/*!targetm.calls.custom_function_descriptors*/*BITS_PER_UNIT);
   }
   return align;

}

//原型 #define stack_realign_drap (crtl->stack_realign_needed && crtl->need_drap)'
bool mtcs_align_stack_realign_drap(MtcsAlign *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   return (mtcsRtlData/*!crtl*/->stack_realign_needed && mtcsRtlData/*!crtl*/->need_drap);
}

//原型 #define ADDR_VEC_ALIGN(VEC) (JUMP_TABLES_IN_TEXT_SECTION ? 5 : 2)
int mtcs_align_get_addr_vec_align (MtcsAlign *self,rtx_jump_table_data *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

   if(self->get_addr_vec_align)
      return self->get_addr_vec_align(self,table);
   //缺省实现 原型 final_addr_vec_align final.cc
   int align = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,table->get_data_mode ());

   if (align > mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(self) / BITS_PER_UNIT)
      align = mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(self) / BITS_PER_UNIT;
   return exact_log2 (align);
}

//原型 #define LABEL_ALIGN(LABEL) align_labels
align_flags mtcs_align_get_label_align(MtcsAlign *self,rtx_insn *label)
{
   if(self->get_label_align)
      return self->get_label_align(self,label);
   //缺省实现 #define LABEL_ALIGN(LABEL) align_labels final.cc
   return self->thisTargetFlagState.x_align_labels;
}

//原型 #define LABEL_ALIGN_AFTER_BARRIER(LABEL) 0
int mtcs_align_get_label_align_after_barrier(MtcsAlign *self,rtx_insn *table)
{
   if(self->get_label_align_after_barrier)
      return self->get_label_align_after_barrier(self,table);
   //缺省实现 #define LABEL_ALIGN_AFTER_BARRIER(LABEL) 0 final.cc
   return 0;
}

//原型 #define INSN_LENGTH_ALIGNMENT(INSN) length_unit_log
int mtcs_align_get_insn_length_alignment(MtcsAlign *self,rtx_insn *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);

   if(self->get_insn_length_alignment)
      return self->get_insn_length_alignment(self,table);
   //缺省实现 #define INSN_LENGTH_ALIGNMENT(INSN) length_unit_log  final.cc
   return mtcs_insn_attr_get_length_unit_log(mtcsInsnAttr);
}

//原型 #define DWARF_CIE_DATA_ALIGNMENT (-((int) UNITS_PER_WORD)) defaults.h
int mtcs_align_get_dwarf_cie_data_alignment(MtcsAlign *self)
{
   if(self->get_dwarf_cie_data_alignment)
      return self->get_dwarf_cie_data_alignment(self);
   return (-((int) UNITS_PER_WORD)); //STACK_GROWS_DOWNWARD 肯定定义的 参见 defualts.h

}
