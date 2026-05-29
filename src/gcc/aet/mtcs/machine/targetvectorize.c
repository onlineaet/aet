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

#include "targetvectorize.h"
#include "aet/aetprinttree.h"
#include "../mtcstarget.h"


//原型 targetm.vectorize.related_mode (vector_mode, element_mode, nunits); #define TARGET_VECTORIZE_RELATED_MODE default_vectorize_related_mode
//opt_machine_mode default_vectorize_related_mode (machine_mode vector_mode,
//                scalar_mode element_mode, poly_uint64 nunits)
static  opt_machine_mode relatedMode_cb(TargetVectorize *self,machine_mode vector_mode,scalar_mode element_mode,poly_uint64 nunits)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   machine_mode result_mode;
   if ((maybe_ne (nunits, 0U)
   || multiple_p (mtcs_mode_get_size(mtcsMode,vector_mode),
   mtcs_mode_get_size(mtcsMode,element_mode), &nunits))
   && mode_for_vector (element_mode, nunits).exists (&result_mode)
   && mtcs_mode_is_vector_p(mtcsMode,result_mode)
   && mtcsTarget/*!targetm.vector_mode_supported_p*/->vector_mode_supported_p(mtcsTarget,result_mode))
      return result_mode;
   return opt_machine_mode ();
}

//原型 targetm.vectorize.get_mask_mode (mode) #define TARGET_VECTORIZE_GET_MASK_MODE default_get_mask_mode
static opt_machine_mode getMaskMode_cb (TargetVectorize *self,machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   return mtcs_mode_related_int_vector_mode/*!related_int_vector_mode*/(mtcsMode,mode);
}

//原型 targetm.vectorize.autovectorize_vector_modes (&vector_modes, true); #define TARGET_VECTORIZE_AUTOVECTORIZE_VECTOR_MODES default_autovectorize_vector_modes
static unsigned int autovectorizeVectorModes_cb (TargetVectorize *self, vector_modes *modes, bool all)
{
   return 0;
}

void target_vectorize_init(TargetVectorize *self)
{
   //原型 targetm.vectorize.related_mode (vector_mode, element_mode, nunits); #define TARGET_VECTORIZE_RELATED_MODE default_vectorize_related_mode
   self->related_mode =relatedMode_cb;
   //原型 targetm.vectorize.vec_perm_const != NULL #define TARGET_VECTORIZE_VEC_PERM_CONST NULL
   self->vec_perm_const=NULL;
   //原型 targetm.vectorize.get_mask_mode (mode) #define TARGET_VECTORIZE_GET_MASK_MODE default_get_mask_mode
   self->get_mask_mode = getMaskMode_cb ;
   //原型 targetm.vectorize.preferred_simd_mode (smode); #define TARGET_VECTORIZE_PREFERRED_SIMD_MODE default_preferred_simd_mode
   //子类实现
   self->preferred_simd_mode = NULL;
   //原型 targetm.vectorize.autovectorize_vector_modes (&vector_modes, true); #define TARGET_VECTORIZE_AUTOVECTORIZE_VECTOR_MODES default_autovectorize_vector_modes
   self->autovectorize_vector_modes = autovectorizeVectorModes_cb;
}

//原型 targetm.vectorize.related_mode (vector_mode, element_mode, nunits); #define TARGET_VECTORIZE_RELATED_MODE default_vectorize_related_mode
opt_machine_mode  target_vectorize_related_mode(TargetVectorize *self,machine_mode vector_mode,
                  scalar_mode element_mode,poly_uint64 nunits)
{
   return self->related_mode(self,vector_mode,element_mode,nunits);
}

nboolean  target_vectorize_have_vec_perm_const (TargetVectorize *self)
{
   return self->vec_perm_const!=NULL;
}

//原型 targetm.vectorize.vec_perm_const != NULL #define TARGET_VECTORIZE_VEC_PERM_CONST NULL
bool   target_vectorize_vec_perm_const (TargetVectorize *self,machine_mode vmode, machine_mode op_mode,
                        rtx dst, rtx src0, rtx src1,  const vec_perm_indices & sel)
{
   return self->vec_perm_const(self,vmode,op_mode,dst,src0,src1,sel);
}

//原型 targetm.vectorize.get_mask_mode (mode) #define TARGET_VECTORIZE_GET_MASK_MODE default_get_mask_mode
opt_machine_mode target_vectorize_get_mask_mode (TargetVectorize *self,machine_mode mode)
{
   return self->get_mask_mode(self,mode);
}

//原型 targetm.vectorize.preferred_simd_mode (smode); #define TARGET_VECTORIZE_PREFERRED_SIMD_MODE default_preferred_simd_mode
machine_mode     target_vectorize_preferred_simd_mode (TargetVectorize *self,scalar_mode mode)
{
   return self->preferred_simd_mode(self,mode);
}

//原型 targetm.vectorize.autovectorize_vector_modes (&vector_modes, true); #define TARGET_VECTORIZE_AUTOVECTORIZE_VECTOR_MODES default_autovectorize_vector_modes
unsigned int     target_vectorize_autovectorize_vector_modes (TargetVectorize *self, vector_modes *modes, bool all)
{
   return self->autovectorize_vector_modes(self,modes,all);
}



