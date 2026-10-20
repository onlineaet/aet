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
 * AET was originally developed  by the zclei@sina.com
 */

#ifndef __A_TYPE_H__
#define __A_TYPE_H__

// __CUDA_RUNTIME_H__ __VECTOR_TYPES_H__与cuda有关的头文件
#if !defined(__CUDA_RUNTIME_H__) && !defined(__VECTOR_TYPES_H__) && !defined(__DRIVER_TYPES_H__) && !defined(__CUDACC_RTC__)
   typedef struct dim3{ unsigned int x,y,z;}dim3;
   typedef struct int1{ int x;}int1;
   typedef struct int2{ int x,y;}int2;
   typedef struct int3{ int x,y,z;}int3;
   typedef struct int4{ int x,y,z,w;}int4;
#endif

#endif

