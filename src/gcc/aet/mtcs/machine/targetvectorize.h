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

#ifndef __GCC_TARGET_VECTORIZE__
#define __GCC_TARGET_VECTORIZE__

#include "../../nlib.h"
#include "machinetarget.h"

typedef struct _TargetVectorize TargetVectorize;
struct _TargetVectorize
{
   MachineTarget parent;
   //原型 targetm.vectorize.related_mode (vector_mode, element_mode, nunits); #define TARGET_VECTORIZE_RELATED_MODE default_vectorize_related_mode
   opt_machine_mode (* related_mode)(TargetVectorize *self,machine_mode vector_mode,scalar_mode element_mode,poly_uint64 nunits);
   //原型 targetm.vectorize.vec_perm_const != NULL #define TARGET_VECTORIZE_VEC_PERM_CONST NULL
   bool (*vec_perm_const)(TargetVectorize *self,machine_mode vmode, machine_mode op_mode,
                     rtx dst, rtx src0, rtx src1,  const vec_perm_indices & sel);
   //原型 targetm.vectorize.get_mask_mode (mode) #define TARGET_VECTORIZE_GET_MASK_MODE default_get_mask_mode
   opt_machine_mode (*get_mask_mode) (TargetVectorize *self,machine_mode mode);
   //原型 targetm.vectorize.preferred_simd_mode (smode); #define TARGET_VECTORIZE_PREFERRED_SIMD_MODE default_preferred_simd_mode
   machine_mode (*preferred_simd_mode) (TargetVectorize *self,scalar_mode mode);
   //原型 targetm.vectorize.autovectorize_vector_modes (&vector_modes, true); #define TARGET_VECTORIZE_AUTOVECTORIZE_VECTOR_MODES default_autovectorize_vector_modes
   unsigned int (*autovectorize_vector_modes) (TargetVectorize *self, vector_modes *modes, bool all);
};

void             target_vectorize_init(TargetVectorize *self);
//原型 targetm.vectorize.related_mode (vector_mode, element_mode, nunits); #define TARGET_VECTORIZE_RELATED_MODE default_vectorize_related_mode
opt_machine_mode target_vectorize_related_mode(TargetVectorize *self,machine_mode vector_mode,scalar_mode element_mode,poly_uint64 nunits);
nboolean         target_vectorize_have_vec_perm_const (TargetVectorize *self);
//原型 targetm.vectorize.vec_perm_const != NULL #define TARGET_VECTORIZE_VEC_PERM_CONST NULL
bool             target_vectorize_vec_perm_const (TargetVectorize *self,machine_mode vmode, machine_mode op_mode,
                        rtx dst, rtx src0, rtx src1,  const vec_perm_indices & sel);
//原型 targetm.vectorize.get_mask_mode (mode) #define TARGET_VECTORIZE_GET_MASK_MODE default_get_mask_mode
opt_machine_mode target_vectorize_get_mask_mode (TargetVectorize *self,machine_mode mode);
//原型 targetm.vectorize.preferred_simd_mode (smode); #define TARGET_VECTORIZE_PREFERRED_SIMD_MODE default_preferred_simd_mode
machine_mode     target_vectorize_preferred_simd_mode (TargetVectorize *self,scalar_mode mode);
//原型 targetm.vectorize.autovectorize_vector_modes (&vector_modes, true); #define TARGET_VECTORIZE_AUTOVECTORIZE_VECTOR_MODES default_autovectorize_vector_modes
unsigned int     target_vectorize_autovectorize_vector_modes (TargetVectorize *self, vector_modes *modes, bool all);



#endif

