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

#ifndef __GCC_MTCS_LOWER_SUBREG__
#define __GCC_MTCS_LOWER_SUBREG__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"
#include "mtcspass.h"


/* Information about whether, and where, lower-subreg should be applied.  */
struct mtcs_lower_subreg_choices {
  /* A boolean vector for move splitting that is indexed by mode and is
     true for each mode that is to have its copies split.  */
  bool move_modes_to_split[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

  /* True if zero-extensions from word_mode to twice_word_mode should
     be split.  */
  bool splitting_zext;

  /* Index X is true if twice_word_mode shifts by X + BITS_PER_WORD
     should be split.  */
  bool splitting_ashift[MAX_BITS_PER_WORD];
  bool splitting_lshiftrt[MAX_BITS_PER_WORD];
  bool splitting_ashiftrt[MAX_BITS_PER_WORD];

  /* True if there is at least one mode that is worth splitting.  */
  bool something_to_do;
};

typedef struct _MtcsLowerSubreg MtcsLowerSubreg;
struct _MtcsLowerSubreg
{
   MtcsComponent parent;
   /* An integer mode that is twice as wide as word_mode.  */
   scalar_int_mode_pod x_twice_word_mode;

   /* What we have decided to do when optimizing for size (index 0)
      and speed (index 1).  */
   struct mtcs_lower_subreg_choices x_choices[2];
   /* Bit N in this bitmap is set if regno N is used in a context in
      which we can decompose it.  */
    bitmap decomposable_context;

   /* Bit N in this bitmap is set if regno N is used in a context in
      which it cannot be decomposed.  */
    bitmap non_decomposable_context;

   /* Bit N in this bitmap is set if regno N is used in a subreg
      which changes the mode but not the size.  This typically happens
      when the register accessed as a floating-point value; we want to
      avoid generating accesses to its subwords in integer modes.  */
    bitmap subreg_context;

   /* Bit N in the bitmap in element M of this array is set if there is a
      copy from reg M to reg N.  */
    vec<bitmap> reg_copy_graph;
};



MtcsLowerSubreg *mtcs_lower_subreg_new(MtcsMode *mtcsMode);
//原型 init_lower_subreg rtl.h lower-subreg.cc
void mtcs_lower_subreg_init_lower_subreg (MtcsLowerSubreg *self);
//rtl pass subreg1 subreg2 subreg3 需要调用的函数
void mtcs_lower_subreg_decompose_multiword_subregs(MtcsLowerSubreg *self,bool decompose_copies);


//原型 NEXT_PASS (pass_lower_subreg, 1);  RTL_PASS lower-subreg.cc subreg1 subreg2 subreg3 n 有条件执行 flag_split_wide_types != 0 decompose_multiword_subregs
typedef struct _MtcsPassLowerSubreg MtcsPassLowerSubreg;
struct _MtcsPassLowerSubreg
{
   MtcsPass parent;
};
MtcsPassLowerSubreg *mtcs_pass_lower_subreg_new(MtcsMode *mtcsMode,int num);

#endif
