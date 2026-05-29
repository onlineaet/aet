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

#ifndef __GCC_MTCS_PTX__
#define __GCC_MTCS_PTX__


#include "../mtcstarget.h"
#include "aet/nlib.h"
#include "mtcsptxmath.h"

typedef struct _MtcsPtx MtcsPtx;
struct _MtcsPtx
{
   MtcsTarget parent;
   struct
   {
      unsigned HOST_WIDE_INT mask; /* Mask for storing fragment.  */
      unsigned HOST_WIDE_INT val; /* Current fragment value.  */
      unsigned HOST_WIDE_INT remaining; /*  Remaining bytes to be written
      out.  */
      unsigned size;  /* Fragment size to accumulate.  */
      unsigned offset;  /* Offset within current fragment.  */
      bool started;   /* Whether we've output any initializer.  */
   } init_frag;

   /*记录调用的函数，比如函数中调用 printf 需要生成一条外部引用printf函数的声明，像这样
    BEGIN GLOBAL FUNCTION DECL: printf
   .extern .func (.param.u32 %value_out) printf (.param.u64 %in_ar0, .param.u64 %in_ar1);
    mtcs_ptx_output_call_insn mtcs_ptx_func_record_fndecl 调用
    当汇率完文件后，写入到汇率第一个函数前。nvptx需在函数声明在前。
   */
   NString *func_decls;//记录在函数内调用的其它函数

   //-misa=sm_75
   char *isa;//缺省misa=sm_75 由编译参数传入
   int nvptxOptimize;
   /* Buffer needed for worker reductions.  This has to be distinct from
   the worker broadcast array, as both may be live concurrently.  */
   unsigned worker_red_size;
   unsigned worker_red_align;
   GTY(()) rtx worker_red_sym;

   /* Buffer needed for vector reductions, when vector_length >
   PTX_WARP_SIZE.  This has to be distinct from the worker broadcast
   array, as both may be live concurrently.  */
   unsigned vector_red_size;
   unsigned vector_red_align;
   unsigned vector_red_partition;
   GTY(()) rtx vector_red_sym;

   /* Shared memory block for gang-private variables.  */
   unsigned gang_private_shared_size;
   unsigned gang_private_shared_align;
   GTY(()) rtx gang_private_shared_sym;
   hash_map<tree_decl_hash, unsigned int> gang_private_shared_hmap;

   GTY(()) tree nvptx_previous_fndecl;

   MtcsPtxMath *mtcsPtxMath;
};

MtcsPtx    *mtcs_ptx_new();
MtcsPtx    *mtcs_ptx_new_full(int isa,int ptxVersion);
bool        mtcs_ptx_pass_in_memory (MtcsPtx *self,mtcs_mode  mode, const_tree type, bool for_return);
nuint       mtcs_ptx_maybe_split_mode ( MtcsPtx *self,mtcs_mode mode);
bool        mtcs_ptx_split_mode_p (MtcsPtx *self,machine_mode mode);
//原型 nvptx_mach_max_workers nvptx.cc
int         mtcs_ptx_get_mach_max_workers(MtcsPtx *self);
//获取需要替换的函数名
//原型 static const char * nvptx_name_replacement (const char *name) nvptx.cc
const char *mtcs_ptx_get_replace_function_name(MtcsPtx *self,const char *origName);
#endif

