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

#ifndef __GCC_MTCS_ALIGN__
#define __GCC_MTCS_ALIGN__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "flags.h"


typedef struct _MtcsAlign MtcsAlign;
struct _MtcsAlign
{
    MtcsComponent parent;
    //原型LOCAL_DECL_ALIGNMENT default.h
    nuint (*get_local_decl_alignment)(MtcsAlign *self,tree decl);
    //原型 LOCAL_ALIGNMENT default.h
    nuint (*get_local_alignment)(MtcsAlign *self,tree type,nuint alignment);
    //原型 parse_alignment_opts toplev.h toplev.cc
    void (*parse_alignment_opts)(MtcsAlign *self);
    //原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
    HOST_WIDE_INT (*get_vector_alignment) (MtcsAlign *self,const_tree type);
    //原型 #define ADDR_VEC_ALIGN(VEC) (JUMP_TABLES_IN_TEXT_SECTION ? 5 : 2)
    int (*get_addr_vec_align)(MtcsAlign *self,rtx_jump_table_data *table);
    //原型 #define LABEL_ALIGN(LABEL) align_labels
    align_flags (*get_label_align)(MtcsAlign *self,rtx_insn *label);
    //原型 #define LABEL_ALIGN_AFTER_BARRIER(LABEL) 0
    int (*get_label_align_after_barrier)(MtcsAlign *self,rtx_insn *table);
    //原型 #define INSN_LENGTH_ALIGNMENT(INSN) length_unit_log
    int (*get_insn_length_alignment)(MtcsAlign *self,rtx_insn *table);
    //原型 #define DWARF_CIE_DATA_ALIGNMENT (-((int) UNITS_PER_WORD))
    int (*get_dwarf_cie_data_alignment)(MtcsAlign *self);


    //原型  #define STACK_SLOT_ALIGNMENT(TYPE,MODE,ALIGN) default.h
    nuint (*get_stack_slot_alignment)(MtcsAlign *self,tree type,machine_mode mode,nuint alignment);
    nuint biggestAlignment;//原型 BIGGEST_ALIGNMENT
    nuint strictAlignmemt;//原型 STRICT_ALIGNMENT
    nuint trampolineAlignment;//原型 #define TRAMPOLINE_ALIGNMENT
    nuint trampolineSize;//原型 #define TRAMPOLINE_SIZE

    //原型 this_target_flag_state flag.h
     target_flag_state thisTargetFlagState;
};



void         mtcs_align_init(MtcsAlign *self);
//原型 LOCAL_ALIGNMENT default.h
nuint        mtcs_align_get_local_alignment(MtcsAlign *self,tree type,nuint alignment);
//原型 LOCAL_DECL_ALIGNMENT default.h
nuint        mtcs_align_get_local_decl_alignment(MtcsAlign *self,tree decl);

//原型  #define STACK_SLOT_ALIGNMENT(TYPE,MODE,ALIGN) default.h
nuint        mtcs_align_get_stack_slot_alignment(MtcsAlign *self,tree type,machine_mode mode,nuint alignment);
void        mtcs_align_set_biggest_alignment(MtcsAlign *self,nuint biggestAlignment);
//原型 BIGGEST_ALIGNMENT
nuint        mtcs_align_get_biggest_alignment(MtcsAlign *self);
//原型 STRICT_ALIGNMENT
void        mtcs_align_set_strict_alignment(MtcsAlign *self,nuint strictAlignment);
nuint       mtcs_align_get_strict_alignment(MtcsAlign *self);
//原型 parse_alignment_opts toplev.h toplev.cc
void       mtcs_align_parse_alignment_opts (MtcsAlign *self);
//原型 #define TRAMPOLINE_ALIGNMENT FUNCTION_ALIGNMENT (FUNCTION_BOUNDARY) nvptx 256 平台定义 defaults.h
nuint       mtcs_align_get_trampoline_alignment (MtcsAlign *self);
void       mtcs_align_set_trampoline_alignment(MtcsAlign *self,nuint value);
//原型 #define TRAMPOLINE_SIZE
nuint       mtcs_align_get_trampoline_size (MtcsAlign *self);
void       mtcs_align_set_trampoline_size(MtcsAlign *self,nuint value);
//原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
HOST_WIDE_INT mtcs_align_get_vector_alignment (MtcsAlign *self,const_tree type);
//原型
//#define FUNCTION_ALIGNMENT(ALIGN)               \
//  (lang_hooks.custom_function_descriptors          \
//   && targetm.calls.custom_function_descriptors > 0         \
//   ? MAX ((ALIGN),                  \
//     2 * targetm.calls.custom_function_descriptors * BITS_PER_UNIT)\
//   : (ALIGN))
//default.h
int mtcs_align_get_function_alignment(MtcsAlign *self,int align);

//原型 #define stack_realign_drap (crtl->stack_realign_needed && crtl->need_drap)'
bool mtcs_align_stack_realign_drap(MtcsAlign *self);
//原型 #define ADDR_VEC_ALIGN(VEC) (JUMP_TABLES_IN_TEXT_SECTION ? 5 : 2)
int mtcs_align_get_addr_vec_align (MtcsAlign *self,rtx_jump_table_data *table);
//原型 #define LABEL_ALIGN(LABEL) align_labels
align_flags mtcs_align_get_label_align(MtcsAlign *self,rtx_insn *label);
//原型 #define LABEL_ALIGN_AFTER_BARRIER(LABEL) 0
int mtcs_align_get_label_align_after_barrier(MtcsAlign *self,rtx_insn *table);
//原型 #define INSN_LENGTH_ALIGNMENT(INSN) length_unit_log
int mtcs_align_get_insn_length_alignment(MtcsAlign *self,rtx_insn *table);
//原型 #define DWARF_CIE_DATA_ALIGNMENT (-((int) UNITS_PER_WORD))
int mtcs_align_get_dwarf_cie_data_alignment(MtcsAlign *self);

#endif
