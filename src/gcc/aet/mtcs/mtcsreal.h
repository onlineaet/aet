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

#ifndef __GCC_MTCS_REAL__
#define __GCC_MTCS_REAL__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsReal MtcsReal;


struct _MtcsReal
{
    MtcsComponent parent;
    int store_flag_value;//原型 #define STORE_FLAG_VALUE 1
    REAL_VALUE_TYPE (*float_store_flag_value)(MtcsReal *self,mtcs_mode mode);//原型 FLOAT_STORE_FLAG_VALUE (mode);
    //原型 real.h 原是全局变量，现在改为 MtcsReal成员，通过备份恢复机制，赋值给全局变量
     REAL_VALUE_TYPE dconst0;
     REAL_VALUE_TYPE dconst1;
     REAL_VALUE_TYPE dconst2;
     REAL_VALUE_TYPE dconstm0;
     REAL_VALUE_TYPE dconstm1;
     REAL_VALUE_TYPE dconsthalf;
     REAL_VALUE_TYPE dconstinf;
     REAL_VALUE_TYPE dconstninf;
};



void mtcs_real_init(MtcsReal *self);
//原型 real_from_integer real.h real.cc
void mtcs_real_real_from_integer (MtcsReal *self,REAL_VALUE_TYPE *r,mtcs_mode mode, const wide_int_ref val_in, signop sgn);
//原型 real_convert real.h real.cc
void mtcs_real_real_convert (MtcsReal *self,REAL_VALUE_TYPE *r, mtcs_mode mode, const REAL_VALUE_TYPE *a);
//原型 real_value_truncate real.h real.cc
REAL_VALUE_TYPE mtcs_real_real_value_truncate (MtcsReal *self,mtcs_mode mode , REAL_VALUE_TYPE a);
//原型 exact_real_truncate real.h real.cc
bool mtcs_real_exact_real_truncate (MtcsReal *self,mtcs_mode mode, const REAL_VALUE_TYPE *a);
//原型 real_to_target real.h real.cc
long mtcs_real_real_to_target (MtcsReal *self,long *buf, const REAL_VALUE_TYPE *r_orig,mtcs_mode mode);
//原型 real_from_target real.h real.cc
void mtcs_real_real_from_target (MtcsReal *self,REAL_VALUE_TYPE *r, const long *buf, mtcs_mode mode);
//原型 real_from_string2 real.h real.cc
REAL_VALUE_TYPE mtcs_real_real_from_string2 (MtcsReal *self,const char *s, mtcs_mode mode);
//原型 STORE_FLAG_VALUE 每个平台不一样 default.h STORE_FLAG_VALUE=1 gcn STORE_FLAG_VALUE=-1;
int   mtcs_real_get_store_flag_value(MtcsReal *self);
void  mtcs_real_set_store_flag_value(MtcsReal *self,int value);
//原型 FLOAT_STORE_FLAG_VALUE (mode);
REAL_VALUE_TYPE mtcs_real_float_store_flag_value(MtcsReal *self,mtcs_mode mode);
//原型 init_emit_once rtl.h emit-rtl.cc 的real部份
void mtcs_real_init_once(MtcsReal *self);
//原型 real_value_from_int_cst real.h tree.cc
REAL_VALUE_TYPE mtcs_real_real_value_from_int_cst (MtcsReal *self,const_tree type, const_tree i);
//原型 real_trunc real.h real.cc
void mtcs_real_real_trunc (MtcsReal *self,REAL_VALUE_TYPE *r, format_helper fmt,const REAL_VALUE_TYPE *x);
//原型 real_max_representable value-range.h
REAL_VALUE_TYPE mtcs_real_max_representable (MtcsReal *self,const_tree type);
//原型 real_min_representable value-range.h
REAL_VALUE_TYPE mtcs_real_min_representable (MtcsReal *self,const_tree type);
//原型 frange_val_max value-range.h
REAL_VALUE_TYPE mtcs_real_frange_val_max (MtcsReal *self,const_tree type);
//原型 frange_val_min value-range.h
REAL_VALUE_TYPE mtcs_real_frange_val_min (MtcsReal *self,const_tree type);
//原型 frange_val_is_min  value-range.h
bool mtcs_real_frange_val_is_min (MtcsReal *self,const REAL_VALUE_TYPE &r, const_tree type);
//原型 frange_val_is_max  value-range.h
bool mtcs_real_frange_val_is_max (MtcsReal *self,const REAL_VALUE_TYPE &r, const_tree type);
//原型 real_maxval real.h real.cc
void mtcs_real_real_maxval (MtcsReal *self,REAL_VALUE_TYPE *r, int sign, machine_mode mode);

///----------------实现 dfp.h--------------
//原型 decimal_real_maxval dfp.h dfp.cc
void mtcs_real_decimal_real_maxval (MtcsReal *self,REAL_VALUE_TYPE *r, int sign, machine_mode mode);

#endif
