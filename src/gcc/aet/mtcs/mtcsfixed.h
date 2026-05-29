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


#ifndef __GCC_MTCS_FIXED__
#define __GCC_MTCS_FIXED__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsFixed MtcsFixed;
struct _MtcsFixed
{
    MtcsComponent parent;


};

MtcsFixed *mtcs_fixed_new(MtcsMode *mtcsMode);
//原型 fixed_convert_from_real fixed-value.h fixed-value.cc
bool mtcs_fixed_fixed_convert_from_real (MtcsFixed *self,FIXED_VALUE_TYPE *f, scalar_mode mode,
          const REAL_VALUE_TYPE *a, bool sat_p);
//原型 fixed_convert_from_int fixed-value.h fixed-value.cc
bool mtcs_fixed_fixed_convert_from_int (MtcsFixed *self,FIXED_VALUE_TYPE *f, scalar_mode mode,
         double_int a, bool unsigned_p, bool sat_p);
//原型 fixed_convert fixed-value.h fixed-value.cc
bool mtcs_fixed_fixed_convert (MtcsFixed *self,FIXED_VALUE_TYPE *f, scalar_mode mode,
               const FIXED_VALUE_TYPE *a, bool sat_p);
//原型 real_convert_from_fixed fixed-value.h fixed-value.cc
void mtcs_fixed_real_convert_from_fixed (MtcsFixed *self,REAL_VALUE_TYPE *r, scalar_mode mode,
          const FIXED_VALUE_TYPE *f);
//原型 fixed_convert fixed-value.h fixed-value.cc
FIXED_VALUE_TYPE mtcs_fixed_fixed_from_double_int (MtcsFixed *self,double_int payload, scalar_mode mode);

#endif

