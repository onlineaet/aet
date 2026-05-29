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
#define IN_TARGET_CODE 1 //不加这句在machmode.h中的GET_MODE_SIZE编译到poly_uint16(poly_int) 因为poly_int没有重载>号，所以编译报错
//insn-modes.h由nvptx生成，但i386生成的类型全覆盖nvptx的insn-modes.h,不需要平台的？？？
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

#include "aet/aetprinttree.h"
#include "../mtcstool.h"
#include "mtcsptxalign.h"
#include "ptx-common.h"
#include "../mtcstarget.h"

//原型 parse_alignment_opts toplev.h toplev.cc
static void parseAlignmentOpts_cb (MtcsAlign *mtcsAlign);
static HOST_WIDE_INT getVectorAlignment_cb (MtcsAlign *mtcsAlign,const_tree type);
//原型 #define ADDR_VEC_ALIGN(VEC) (JUMP_TABLES_IN_TEXT_SECTION ? 5 : 2)
static int getAddrVecAlign_cb (MtcsAlign *mtcsAlign,rtx_jump_table_data *table);

static void mtcsPtxAlignInit(MtcsPtxAlign *self)
{
    MtcsAlign *mtcsAlign=(MtcsAlign *)self;
    mtcs_align_set_strict_alignment(mtcsAlign,PTX_STRICT_ALIGNMENT);
    mtcs_align_set_biggest_alignment(mtcsAlign,PTX_BIGGEST_ALIGNMENT);
    mtcs_align_set_trampoline_alignment(mtcsAlign,PTX_TRAMPOLINE_ALIGNMENT);
    mtcs_align_set_trampoline_size(mtcsAlign,PTX_TRAMPOLINE_SIZE);
    //原型 parse_alignment_opts toplev.h toplev.cc
    mtcsAlign->parse_alignment_opts=parseAlignmentOpts_cb;
    mtcsAlign->get_vector_alignment=getVectorAlignment_cb;
    //原型 #define ADDR_VEC_ALIGN(VEC) (JUMP_TABLES_IN_TEXT_SECTION ? 5 : 2)
    mtcsAlign->get_addr_vec_align=getAddrVecAlign_cb;

}

//原型 #define ADDR_VEC_ALIGN(VEC) (JUMP_TABLES_IN_TEXT_SECTION ? 5 : 2) nvptx.h
static int getAddrVecAlign_cb (MtcsAlign *mtcsAlign,rtx_jump_table_data *table)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsAlign);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   return mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)?5:2;
}

/* Parse "N[:M][:...]" into struct align_flags A.
   VALUES contains parsed values (in reverse order), all processed
   values are popped.  */
//原型 read_log_maxskip toplev.cc
static void read_log_maxskip (auto_vec<unsigned> &values, align_flags_tuple *a)
{
  unsigned n = values.pop ();
  if (n != 0)
    a->log = floor_log2 (n * 2 - 1);
  if (values.is_empty ())
    a->maxskip = n ? n - 1 : 0;
  else{
      unsigned m = values.pop ();
      /* -falign-foo=N:M means M-1 max bytes of padding, not M.  */
      if (m > 0)
          m--;
      a->maxskip = m;
  }
  /* Normalize the tuple.  */
  a->normalize ();
}

/* Parse "N[:M[:N2[:M2]]]" string FLAG into a pair of struct align_flags.  */
//原型 parse_N_M toplev.cc SUBALIGN_LOG宏在nvptx没定义。
static void parse_N_M (const char *flag, align_flags &a)
{
  if (flag){
      static hash_map <nofree_string_hash, align_flags> cache;
      align_flags *entry = cache.get (flag);
      if (entry){
          a = *entry;
          return;
      }
      auto_vec<unsigned> result_values;
      bool r = parse_and_check_align_values (flag, NULL, result_values, false,UNKNOWN_LOCATION);
      if (!r)
          return;
      /* Reverse values for easier manipulation.  */
      result_values.reverse ();
      read_log_maxskip (result_values, &a.levels[0]);
      if (!result_values.is_empty ())
          read_log_maxskip (result_values, &a.levels[1]);
//#ifdef SUBALIGN_LOG
//      else
//    {
//      /* N2[:M2] is not specified.  This arch has a default for N2.
//         Before -falign-foo=N:M:N2:M2 was introduced, x86 had a tweak.
//         -falign-functions=N with N > 8 was adding secondary alignment.
//         -falign-functions=10 was emitting this before every function:
//            .p2align 4,,9
//            .p2align 3
//         Now this behavior (and more) can be explicitly requested:
//         -falign-functions=16:10:8
//         Retain old behavior if N2 is missing: */
//
//      int align = 1 << a.levels[0].log;
//      int subalign = 1 << SUBALIGN_LOG;
//
//      if (a.levels[0].log > SUBALIGN_LOG
//          && a.levels[0].maxskip >= subalign - 1)
//        {
//          /* Set N2 unless subalign can never have any effect.  */
//          if (align > a.levels[0].maxskip + 1)
//        {
//          a.levels[1].log = SUBALIGN_LOG;
//          a.levels[1].normalize ();
//        }
//        }
//    }
//#endif

      /* Cache seen value.  */
      cache.put (flag, a);
  }
}

/* Process -falign-foo=N[:M[:N2[:M2]]] options.  */
//原型 parse_alignment_opts toplev.h toplev.cc
static void parseAlignmentOpts_cb (MtcsAlign *mtcsAlign)
{
  MtcsPtxAlign *self=(MtcsPtxAlign *)mtcsAlign;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  parse_N_M (mtcsOptionsItem->x_str_align_loops,mtcsAlign->thisTargetFlagState.x_align_loops/*!align_loops*/);
  parse_N_M (mtcsOptionsItem->x_str_align_jumps, mtcsAlign->thisTargetFlagState.x_align_jumps);
  parse_N_M (mtcsOptionsItem->x_str_align_labels, mtcsAlign->thisTargetFlagState.x_align_labels);
  parse_N_M (mtcsOptionsItem->x_str_align_functions, mtcsAlign->thisTargetFlagState.x_align_functions);
}

/* Limit vector alignments to BIGGEST_ALIGNMENT.  */
//原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
static HOST_WIDE_INT getVectorAlignment_cb (MtcsAlign *mtcsAlign,const_tree type)
{
  fprintf(stderr,"-----nvptx.cc -----59-- TARGET_VECTOR_ALIGNMENT HOST_WIDE_INT nvptx_vector_alignment (const_tree type)\n");
  MtcsPtxAlign *self=(MtcsPtxAlign *)mtcsAlign;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  unsigned HOST_WIDE_INT align;
  tree size = TYPE_SIZE (type);
  /* Ensure align is not bigger than BIGGEST_ALIGNMENT.  */
  if (tree_fits_uhwi_p (size)){
      align = tree_to_uhwi (size);
      align = MIN (align, mtcsAlign->biggestAlignment/*!BIGGEST_ALIGNMENT*/);
  }else
    align =  mtcsAlign->biggestAlignment/*!BIGGEST_ALIGNMENT*/;

  /* Ensure align is not smaller than mode alignment.  */
  align = MAX (align, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,TYPE_MODE (type)));

  return align;
}

MtcsPtxAlign *mtcs_ptx_align_new(MtcsMode *mtcsMode)
{
     MtcsPtxAlign *self = n_slice_alloc0 (sizeof(MtcsPtxAlign));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_align_init((MtcsAlign *)self);
     mtcsPtxAlignInit(self);
     return self;
}

