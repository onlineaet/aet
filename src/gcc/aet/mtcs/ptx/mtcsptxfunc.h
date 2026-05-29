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

#ifndef __GCC_MTCS_PTX_FUNC__
#define __GCC_MTCS_PTX_FUNC__

#include "aet/nlib.h"
#include "../mtcsrtldata.h"
#include "../mtcsfunc.h"

//struct tree_hasher : ggc_cache_ptr_hash<tree_node>
//{
//  static hashval_t hash (tree t) { return htab_hash_pointer (t); }
//  static bool equal (tree a, tree b) { return a == b; }
//};



struct  ptx_machine_function
{
  rtx_expr_list *call_args;  /* Arg list for the current call.  */
  bool doing_call; /* Within a CALL_ARGS ... CALL_ARGS_END sequence.  */
  bool is_variadic;  /* This call is variadic  */
  bool has_variadic;  /* Current function has a variadic call.  */
  bool has_chain; /* Current function has outgoing static chain.  */
  bool has_softstack; /* Current function has a soft stack frame.  */
  bool has_simtreg; /* Current function has an OpenMP SIMD region.  */
  int num_args; /* Number of args of current call.  */
  int return_mode; /* Return mode of current fn.
              (machine_mode not defined yet.) */
  rtx axis_predicate[2]; /* Neutering predicates.  */
  int axis_dim[2]; /* Maximum number of threads on each axis, dim[0] is
              vector_length, dim[1] is num_workers.  */
  bool axis_dim_init_p;
  rtx bcast_partition; /* Register containing the size of each
              vector's partition of share-memory used to
              broadcast state.  */
  rtx red_partition; /* Similar to bcast_partition, except for vector
            reductions.  */
  rtx sync_bar; /* Synchronization barrier ID for vectors.  */
  rtx unisimt_master; /* 'Master lane index' for -muniform-simt.  */
  rtx unisimt_predicate; /* Predicate for -muniform-simt.  */
  rtx unisimt_outside_simt_predicate; /* Predicate for -muniform-simt.  */
  rtx unisimt_location; /* Mask location for -muniform-simt.  */
  /* The following two fields hold the maximum size resp. alignment required
     for per-lane storage in OpenMP SIMD regions.  */
  unsigned HOST_WIDE_INT simt_stack_size;
  unsigned HOST_WIDE_INT simt_stack_align;
};

typedef struct _MtcsPtxFunc MtcsPtxFunc;
struct _MtcsPtxFunc
{
    MtcsFunc parent;
    //原型 GTY((cache)) hash_table<tree_hasher> *needed_fndecls_htab;  nvptx.cc
    //原型 GTY((cache)) hash_table<tree_hasher> *declared_fndecls_htab; nvptx.cc
    NHashTable  *needed_fndecls_htab;
    NHashTable  *declared_fndecls_htab;
    //原型 static GTY((cache)) hash_table<declared_libfunc_hasher> *declared_libfuncs_htab; nvptx.cc
    NHashTable  *declared_libfuncs_htab;

    int reserver;
};



MtcsPtxFunc *mtcs_ptx_func_new(MtcsMode *mtcsMode);
void  mtcs_ptx_func_write_return_mode (MtcsPtxFunc *self,NString *str, bool for_proto, mtcs_mode mode);
void  mtcs_ptx_func_write_fn_proto_from_insn (MtcsPtxFunc *self,NString *strs, const char *name,rtx result, rtx pat);
void  mtcs_ptx_func_write_fn_marker (MtcsPtxFunc *self,NString *str, bool is_defn, bool globalize, const char *name);
int   mtcs_ptx_func_write_arg_mode (MtcsPtxFunc *self,NString *str, int for_reg, int argno,mtcs_mode mode);
//原型 static void nvptx_record_libfunc (rtx callee, rtx retval, rtx pat) nvptx.cc
void  mtcs_ptx_func_write_record_libfunc (MtcsPtxFunc *self,NString *strs,rtx callee, rtx retval, rtx pat);
//原型 static nvptx_record_fndecl (tree decl) nvptx.cc
void  mtcs_ptx_func_record_fndecl (MtcsPtxFunc *self,tree decl);
//原型 static voiod write_fn_proto
//void mtcs_ptx_func_write_fn_proto (MtcsPtxFunc *self,NString *strs, bool is_defn,const char *name, const_tree decl, bool force_public=false);
char *mtcs_ptx_func_write_fn_proto(MtcsPtxFunc *self, const char *name, const_tree decl, bool force_public=false);

//原型 static bool write_return_type nvptx.cc
bool mtcs_ptx_func_write_return_type (MtcsPtxFunc *self,NString *strs, bool for_proto, tree type);
//原型 static int write_arg_type nvptx.cc
int mtcs_ptx_func_write_arg_type (MtcsPtxFunc *self,NString *str, int for_reg, int argno,tree type, bool prototyped);
//原型 static void write_fn_proto_1 nvptx.cc
char *mtcs_ptx_func_write_fn_proto_1 (MtcsPtxFunc *self,const char *name, const_tree decl, bool force_public);
//原型static void nvptx_maybe_record_fnsym
void mtcs_ptx_func_maybe_record_fnsym  (MtcsPtxFunc *self,rtx sym);
/**
 * 当完成汇编文件时
 * 原型 static void nvptx_file_end (void) 开始部分
 * hash_table<tree_hasher>::iterator iter;
    tree decl;
    FOR_EACH_HASH_TABLE_ELEMENT (*needed_fndecls_htab, decl, tree, iter)
       nvptx_record_fndecl (decl);
 */
void mtcs_ptx_func_file_end(MtcsPtxFunc *self);
//原型 nvptx_record_needed_fndecl nvptx.cc
void mtcs_ptx_func_record_needed_fndecl (MtcsPtxFunc *self,tree decl);

#endif
