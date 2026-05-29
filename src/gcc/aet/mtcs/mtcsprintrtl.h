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

#ifndef __GCC_MTCS_PRINT_RTL__
#define __GCC_MTCS_PRINT_RTL__

#include "aet/nlib.h"

#include "bitmap.h"

class mtcs_rtx_reuse_manager;

/* A class for writing rtx to a FILE *.  */

class mtcs_rtx_writer
{
 public:
   mtcs_rtx_writer (FILE *outfile, int ind, bool simple, bool compact,
         mtcs_rtx_reuse_manager *reuse_manager,void *mtcsTarget);

  void print_rtx (const_rtx in_rtx);
  void print_rtl (const_rtx rtx_first);
  void print_rtl_single_with_indent (const_rtx x, int ind);

  void finish_directive ();

  void *target;//创建mtcs_rtx_writer时传入

 private:
  void print_rtx_operand_code_0 (const_rtx in_rtx, int idx);
  void print_rtx_operand_code_e (const_rtx in_rtx, int idx);
  void print_rtx_operand_codes_E_and_V (const_rtx in_rtx, int idx);
  void print_rtx_operand_code_L (const_rtx in_rtx, int idx);
  void print_rtx_operand_code_i (const_rtx in_rtx, int idx);
  void print_rtx_operand_code_r (const_rtx in_rtx);
  void print_rtx_operand_code_u (const_rtx in_rtx, int idx);
  void print_rtx_operand (const_rtx in_rtx, int idx);
  bool operand_has_default_value_p (const_rtx in_rtx, int idx);

 private:
  FILE *m_outfile;
  int m_indent;
  bool m_sawclose;
  bool m_in_call_function_usage;

  /* True means use simplified format without flags, modes, etc.  */
  bool m_simple;

  /* If true, use compact dump format:
     - PREV/NEXT_INSN UIDs are omitted
     - INSN_CODEs are omitted,
     - register numbers are omitted for hard and virtual regs, and
       non-virtual pseudos are offset relative to the first such reg, and
       printed with a '%' sigil e.g. "%0" for (LAST_VIRTUAL_REGISTER + 1),
     - insn names are prefixed with "c" (e.g. "cinsn", "cnote", etc).  */
  bool m_compact;

  /* An optional instance of rtx_reuse_manager.  */
  mtcs_rtx_reuse_manager *m_rtx_reuse_manager;
};

#ifdef BUFSIZ
extern void print_rtl (FILE *, const_rtx);
#endif
extern void print_rtx_insn_vec (FILE *file, const vec<rtx_insn *> &vec);

extern void mtcs_dump_value_slim (FILE *, const_rtx, int);
extern void mtcs_dump_insn_slim (FILE *, const rtx_insn *);
extern void mtcs_dump_rtl_slim (FILE *, const rtx_insn *, const rtx_insn *,
            int, int);
extern void mtcs_print_value (pretty_printer *, const_rtx, int);
extern void mtcs_print_pattern (pretty_printer *, const_rtx, int);
extern void mtcs_print_insn (pretty_printer *pp, const rtx_insn *x, int verbose);
extern void mtcs_print_insn_with_notes (pretty_printer *, const rtx_insn *);

extern void mtcs_rtl_dump_bb_for_graph (pretty_printer *, basic_block);
extern const char *mtcs_str_pattern_slim (const_rtx);

extern void mtcs_print_rtx_function (FILE *file, function *fn, bool compact);


/* For some rtx codes (such as SCRATCH), instances are defined to only be
   equal for pointer equality: two distinct SCRATCH instances are non-equal.
   copy_rtx preserves this equality by reusing the SCRATCH instance.

   For example, in this x86 instruction:

      (cinsn (set (mem/v:BLK (scratch:DI) [0  A8])
                    (unspec:BLK [
                            (mem/v:BLK (scratch:DI) [0  A8])
                        ] UNSPEC_MEMORY_BLOCKAGE)) "test.c":2
                 (nil))

   the two instances of "(scratch:DI)" are actually the same underlying
   rtx pointer (and thus "equal"), and the insn will only be recognized
   (as "*memory_blockage") if this pointer-equality is preserved.

   To be able to preserve this pointer-equality when round-tripping
   through dumping/loading the rtl, we need some syntax.  The first
   time a reused rtx is encountered in the dump, we prefix it with
   a reuse ID:

      (0|scratch:DI)

   Subsequent references to the rtx in the dump can be expressed using
   "reuse_rtx" e.g.:

      (reuse_rtx 0)

   This class is responsible for tracking a set of reuse IDs during a dump.

   Dumping with reuse-support is done in two passes:

   (a) a first pass in which "preprocess" is called on each top-level rtx
       to be seen in the dump.  This traverses the rtx and its descendents,
       identifying rtx that will be seen more than once in the actual dump,
       and assigning them reuse IDs.

   (b) the actual dump, via print_rtx etc.  print_rtx detect the presence
       of a live rtx_reuse_manager and uses it if there is one.  Any rtx
       that were assigned reuse IDs will be printed with it the first time
       that they are seen, and then printed as "(reuse_rtx ID)" subsequently.

   The first phase is needed since otherwise there would be no way to tell
   if an rtx will be reused when first encountering it.  */

class mtcs_rtx_reuse_manager
{
 public:
   mtcs_rtx_reuse_manager ();

  /* The first pass.  */
  void preprocess (const_rtx x);

  /* The second pass (within print_rtx).  */
  bool has_reuse_id (const_rtx x, int *out);
  bool seen_def_p (int reuse_id);
  void set_seen_def (int reuse_id);

 private:
  hash_map<const_rtx, int> m_rtx_occurrence_count;
  hash_map<const_rtx, int> m_rtx_reuse_ids;
  auto_bitmap m_defs_seen;
  int m_next_id;
};

//原型 print_mem_expr rtl.h print-rtl.cc
void mtcs_print_mem_expr (FILE *outfile, const_tree expr);
//原型 print_inline_rtx  rtl.h print-rtl.cc
void mtcs_print_inline_rtx (FILE *outf, const_rtx x, int ind);
//原型 debug_rtx rtl.h print-rtl.cc
void mtcs_debug_rtx (const_rtx x);
//原型 debug rtl.h print-rtl.cc
void mtcs_debug (const rtx_def &ref);
//原型 debug rtl.h print-rtl.cc
void mtcs_debug (const rtx_def *ptr);
//原型 debug_rtx_list rtl.h print-rtl.cc
void mtcs_debug_rtx_list (const rtx_insn *x, int n);
//原型 debug_rtx_find rtl.h print-rtl.cc
const rtx_insn * mtcs_debug_rtx_find (const rtx_insn *x, int uid);
//原型 debug_rtx_range rtl.h print-rtl.cc
void mtcs_debug_rtx_range (const rtx_insn *start, const rtx_insn *end);
//原型 print_rtl rtl.h print-rtl.cc
void mtcs_print_rtl (FILE *outf, const_rtx rtx_first);
//原型 print_rtl_single rtl.h print-rtl.cc
void mtcs_print_rtl_single (FILE *outf, const_rtx x);
//原型 print_simple_rtl rtl.h print-rtl.cc
void mtcs_print_simple_rtl (FILE *outf, const_rtx x);
//原型 print_rtx_insn_vec print-rtl.h print-rtl.cc
void mtcs_print_rtx_insn_vec (FILE *file, const vec<rtx_insn *> &vec);
void mtcs_print_func_rtl(struct function *fn);
//重载函数
void mtcs_print_func_rtl(struct function *fn,const char *explain);

#endif
