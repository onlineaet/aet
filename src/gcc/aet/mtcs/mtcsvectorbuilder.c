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
 * base on stmt.cc
 */


/* This file handles generation of all the assembler code
   *except* the instructions of a buildertion.
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
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"

#include "mtcsvectorbuilder.h"
#include "mtcstarget.h"
//
//static void mtcsVectorBuilderInit(MtcsVectorBuilder *self)
//{
//
//}
//
//static rtx find_cached_value (MtcsVectorBuilder *self)
//{
//  if (encoded_nelts () != 1)
//    return NULL_RTX;
//
//  rtx elt = (*this)[0];
//
//  if (GET_MODE_CLASS (m_mode) == MODE_VECTOR_BOOL)
//    {
//      if (elt == const1_rtx)
//    return CONST1_RTX (m_mode);
//      else if (elt == constm1_rtx)
//    return CONSTM1_RTX (m_mode);
//      else if (elt == const0_rtx)
//    return CONST0_RTX (m_mode);
//      else
//    gcc_unreachable ();
//    }
//
//  /* We can be called before the global vector constants are set up,
//     but in that case we'll just return null.  */
//  scalar_mode inner_mode = GET_MODE_INNER (m_mode);
//  if (elt == CONST0_RTX (inner_mode))
//    return CONST0_RTX (m_mode);
//  else if (elt == CONST1_RTX (inner_mode))
//    return CONST1_RTX (m_mode);
//  else if (elt == CONSTM1_RTX (inner_mode))
//    return CONSTM1_RTX (m_mode);
//
//  return NULL_RTX;
//}
//
//rtx mtcs_vector_builder_build(rtvec v)
//{
//  finalize ();
//  rtx x = find_cached_value ();
//  if (x)
//    return x;
//
//  x = gen_rtx_raw_CONST_VECTOR (m_mode, v);
//  CONST_VECTOR_NPATTERNS (x) = npatterns ();
//  CONST_VECTOR_NELTS_PER_PATTERN (x) = nelts_per_pattern ();
//  return x;
//}
//
//MtcsVectorBuilder *mtcs_vector_builder_get()
//{
//    static MtcsVectorBuilder *singleton = NULL;
//    if (!singleton){
//         singleton =n_slice_alloc0 (sizeof(MtcsVectorBuilder));
//         mtcsVectorBuilderInit(singleton);
//    }
//    return singleton;
//}


/* Return a CONST_VECTOR for the current constant.  V is an existing
   rtvec that contains all the elements.  */

rtx MtcsVectorBuilder::build (rtvec v)
{
  finalize ();

  rtx x = find_cached_value ();
  if (x)
    return x;

  x = gen_rtx_raw_CONST_VECTOR (m_mode, v);
  CONST_VECTOR_NPATTERNS (x) = npatterns ();
  CONST_VECTOR_NELTS_PER_PATTERN (x) = nelts_per_pattern ();
  return x;
}

/* Return a vector element with the value BASE + FACTOR * STEP.  */

rtx MtcsVectorBuilder::apply_step (rtx base, unsigned int factor,const poly_wide_int &step) const
{
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  scalar_int_mode int_mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,
          (machine_mode)mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,m_mode));
  return mtcs_rtl_immed_wide_int_const (mtcsRTL,mtcs_rtl_to_poly_wide/*!wi::to_poly_wide*/(base, int_mode) + factor * step,int_mode);
}

/* Return a CONST_VECTOR for the current constant.  */

rtx MtcsVectorBuilder::build ()
{
  finalize ();

  rtx x = find_cached_value ();
  if (x)
    return x;

  unsigned int nelts;
  if (!mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/ (mtcsMode,m_mode).is_constant (&nelts))
    nelts = encoded_nelts ();
  rtvec v = rtvec_alloc (nelts);
  for (unsigned int i = 0; i < nelts; ++i)
    RTVEC_ELT (v, i) = elt (i);
  x = gen_rtx_raw_CONST_VECTOR (m_mode, v);
  CONST_VECTOR_NPATTERNS (x) = npatterns ();
  CONST_VECTOR_NELTS_PER_PATTERN (x) = nelts_per_pattern ();
  return x;
}

/* Check whether there is a global cached value for the vector.
   Return it if so, otherwise return null.  */

rtx MtcsVectorBuilder::find_cached_value ()
{
  if (encoded_nelts () != 1)
    return NULL_RTX;

  rtx elt = (*this)[0];

  if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,m_mode) == MODE_VECTOR_BOOL){
      if (elt == const1_rtx)
          return CONST1_RTX (m_mode);
      else if (elt == constm1_rtx)
          return CONSTM1_RTX (m_mode);
      else if (elt == const0_rtx)
          return CONST0_RTX (m_mode);
      else
    gcc_unreachable ();
  }

  /* We can be called before the global vector constants are set up,
     but in that case we'll just return null.  */
  scalar_mode inner_mode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,m_mode);
  if (elt == CONST0_RTX (inner_mode))
    return CONST0_RTX (m_mode);
  else if (elt == CONST1_RTX (inner_mode))
    return CONST1_RTX (m_mode);
  else if (elt == CONSTM1_RTX (inner_mode))
    return CONSTM1_RTX (m_mode);

  return NULL_RTX;
}

