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


#ifndef __GCC_MTCS_INTERNAL_FN__
#define __GCC_MTCS_INTERNAL_FN__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"

//内置函数，用户不能直接调用，由编译器生成的函数
typedef struct _MtcsInternalFn MtcsInternalFn;
struct _MtcsInternalFn
{
     MtcsComponent parent;
     void (*expandDIVMOD)(MtcsInternalFn *self,rtx q,rtx a,rtx b,rtx r,rtx tmp,machine_mode mode,int unsignedp);
};

//原型 expand_addsub_overflow internal-fn.h internal-fn.cc
void mtcs_internal_fn_expand_addsub_overflow (MtcsInternalFn *self,location_t loc, tree_code code, tree lhs,
         tree arg0, tree arg1, bool unsr_p, bool uns0_p,
         bool uns1_p, bool is_ubsan, tree *datap);

//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,internal_fn, tree_pair,
                   optimization_type);
//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,internal_fn, tree,
                   optimization_type);
//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,gcall *, optimization_type);

/* Return true if FN is supported for types TYPE0 and TYPE1 when the
   optimization type is OPT_TYPE.  The types are those associated with
   the "type0" and "type1" fields of FN's direct_internal_fn_info
   structure.  */
//原型 direct_internal_fn_supported_p internal-fn.h internal-fn.cc
inline bool mtcs_internal_fn_direct_internal_fn_supported_p (MtcsInternalFn *self,internal_fn fn, tree type0, tree type1,
            optimization_type opt_type)
{
  return mtcs_internal_fn_direct_internal_fn_supported_p/*!direct_internal_fn_supported_p*/(self,
        fn, tree_pair (type0, type1),opt_type);
}

//原型 get_supported_else_vals internal-fn.h internal-fn.cc
void mtcs_internal_fn_get_supported_else_vals (MtcsInternalFn *self,enum insn_code icode, unsigned else_index,vec<int> &else_vals);
//原型 supported_else_val_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_supported_else_val_p (MtcsInternalFn *self,enum insn_code icode, unsigned else_index, int else_val);
//原型 internal_gather_scatter_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_internal_gather_scatter_fn_supported_p (MtcsInternalFn *self,internal_fn ifn, tree vector_type,
               tree memory_element_type, tree offset_vector_type, int scale, vec<int> *elsvals);
//原型 internal_check_ptrs_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_internal_check_ptrs_fn_supported_p (MtcsInternalFn *self,internal_fn ifn, tree type,
                poly_uint64 length, unsigned int align);
//原型 internal_len_load_store_bias internal-fn.h internal-fn.cc
signed char mtcs_internal_fn_internal_len_load_store_bias (MtcsInternalFn *self,internal_fn ifn, machine_mode mode);
//原型 expand_internal_call internal-fn.h internal-fn.cc
void mtcs_internal_fn_expand_internal_call (MtcsInternalFn *self,internal_fn fn, gcall *stmt);
//原型 expand_internal_call internal-fn.h internal-fn.cc
void mtcs_internal_fn_expand_internal_call (MtcsInternalFn *self,gcall *stmt);
//原型 vectorized_internal_fn_supported_p internal-fn.h internal-fn.cc
bool mtcs_internal_fn_vectorized_internal_fn_supported_p (MtcsInternalFn *self,internal_fn ifn, tree type);
//原型 target_supports_divmod_p tree-ssa-math-opts.cc
bool mtcs_internal_fn_supports_divmod_p(MtcsInternalFn *self,bool unsign, machine_mode mode);

void mtcs_internal_fn_divmod(MtcsInternalFn *self,rtx q,rtx a,rtx b,rtx r,rtx tmp,machine_mode mode,int unsignedp);

#endif

