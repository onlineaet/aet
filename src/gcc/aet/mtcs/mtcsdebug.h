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

#ifndef __GCC_MTCS_DEBUG__
#define __GCC_MTCS_DEBUG__

#include "../nlib.h"
#include "mtcsmicro.h"
#include "mtcscomponent.h"

typedef struct _MtcsDebug MtcsDebug;
struct _MtcsDebug
{
   MtcsComponent parent;
   /* Initialize debug output.  MAIN_FILENAME is the name of the main
   input file.  */
   void (* init) (MtcsDebug *self,const char *main_filename);

   /* Output debug symbols.  */
   void (* finish) (MtcsDebug *self,const char *main_filename);

   /* Run cleanups necessary after early debug generation.  */
   void (* early_finish) (MtcsDebug *self,const char *main_filename);

   /* Called from cgraph_optimize before starting to assemble
   functions/variables/toplevel asms.  */
   void (* assembly_start) (MtcsDebug *self);

   /* Macro defined on line LINE with name and expansion TEXT.  */
   void (* define) (MtcsDebug *self,unsigned int line, const char *text);

   /* MACRO undefined on line LINE.  */
   void (* undef) (MtcsDebug *self,unsigned int line, const char *macro);

   /* Record the beginning of a new source file FILE from LINE number
   in the previous one.  */
   void (* start_source_file) (MtcsDebug *self,unsigned int line, const char *file);

   /* Record the resumption of a source file.  LINE is the line number
   in the source file we are returning to.  */
   void (* end_source_file) (MtcsDebug *self,unsigned int line);

   /* Record the beginning of block N, counting from 1 and not
   including the function-scope block, at LINE.  */
   void (* begin_block) (MtcsDebug *self,unsigned int line, unsigned int n, tree block);

   /* Record the end of a block.  Arguments as for begin_block.  */
   void (* end_block) (MtcsDebug *self,unsigned int line, unsigned int n);

   /* Returns nonzero if it is appropriate not to emit any debugging
   information for BLOCK, because it doesn't contain any
   instructions.  This may not be the case for blocks containing
   nested functions, since we may actually call such a function even
   though the BLOCK information is messed up.  Defaults to true.  */
   bool (* ignore_block) (MtcsDebug *self,const_tree);

   /* Record a source file location at (FILE, LINE, COLUMN, DISCRIMINATOR).  */
   void (* source_line) (MtcsDebug *self,unsigned int line, unsigned int column,
         const char *file, int discriminator, bool is_stmt);

   /* Record a source file location for a DECL_IGNORED_P function.  */
   void (* set_ignored_loc) (MtcsDebug *self,unsigned int line, unsigned int column,const char *file);

   /* Called at start of prologue code.  LINE is the first line in the
   function.  */
   void (* begin_prologue) (MtcsDebug *self,unsigned int line, unsigned int column,const char *file);

   /* Called at end of prologue code.  LINE is the first line in the
   function.  */
   void (* end_prologue) (MtcsDebug *self,unsigned int line, const char *file);

   /* Called at beginning of epilogue code.  */
   void (* begin_epilogue) (MtcsDebug *self,unsigned int line, const char *file);

   /* Record end of epilogue code.  */
   void (* end_epilogue) (MtcsDebug *self,unsigned int line, const char *file);

   /* Called at start of function DECL, before it is declared.  */
   void (* begin_function) (MtcsDebug *self,tree decl);

   /* Record end of function.  LINE is highest line number in function.  */
   void (* end_function) (MtcsDebug *self,unsigned int line);

   /* Register UNIT as the main translation unit.  Called from front-ends when
   they create their main translation unit.  */
   void (* register_main_translation_unit) (MtcsDebug *self,tree);

   /* Debug information for a function DECL.  This might include the
   function name (a symbol), its parameters, and the block that
   makes up the function's body, and the local variables of the
   function.

   This is only called for FUNCTION_DECLs.  It is part of the late
   debug pass and is called from rest_of_handle_final.

   Location information is available at this point.

   See the documentation for early_global_decl and late_global_decl
   for other entry points into the debugging back-ends for DECLs.  */
   void (* function_decl) (MtcsDebug *self,tree decl);

   /* Debug information for a global DECL.  Called from the parser
   after the parsing process has finished.

   This gets called for both variables and functions.

   Location information is not available at this point, but it is a
   good probe point to get access to symbols before they get
   optimized away.

   This hook may be called on VAR_DECLs or FUNCTION_DECLs.  It is up
   to the hook to use what it needs.  */
   void (* early_global_decl) (MtcsDebug *self,tree decl);

   /* Augment debug information generated by early_global_decl with
   more complete debug info (if applicable).  Called from toplev.cc
   after the compilation proper has finished and cgraph information
   is available.

   This gets called for both variables and functions.

   Location information is usually available at this point, unless
   the hook is being called for a decl that has been optimized away.

   This hook may be called on VAR_DECLs or FUNCTION_DECLs.  It is up
   to the hook to use what it needs.  */
   void (* late_global_decl) (MtcsDebug *self,tree decl);

   /* Debug information for a type DECL.  Called from toplev.cc after
   compilation proper, also from various language front ends to
   record built-in types.  The second argument is properly a
   boolean, which indicates whether or not the type is a "local"
   type as determined by the language.  (It's not a boolean for
   legacy reasons.)  */
   void (* type_decl) (MtcsDebug *self,tree decl, int local);

   /* Debug information for imported modules and declarations.  */
   void (* imported_module_or_decl) (MtcsDebug *self,tree decl, tree name,tree context, bool child,bool implicit);

   /* Return true if a DIE for the tree is available and return a symbol
   and offset that can be used to refer to it externally.  */
   bool (* die_ref_for_decl) (MtcsDebug *self,tree, const char **, unsigned HOST_WIDE_INT *);

   /* Early debug information for the tree is available at symbol plus
   offset externally.  */
   void (* register_external_die) (MtcsDebug *self,tree, const char *, unsigned HOST_WIDE_INT);

   /* DECL is an inline function, whose body is present, but which is
   not being output at this point.  */
   void (* deferred_inline_function) (MtcsDebug *self,tree decl);

   /* DECL is an inline function which is about to be emitted out of
   line.  The hook is useful to, e.g., emit abstract debug info for
   the inline before it gets mangled by optimization.  */
   void (* outlining_inline_function) (MtcsDebug *self,tree decl);

   /* Called from final_scan_insn for any CODE_LABEL insn whose
   LABEL_NAME is non-null.  */
   void (* label) (MtcsDebug *self,rtx_code_label *);

   /* Called after the start and before the end of writing a PCH file.
   The parameter is 0 if after the start, 1 if before the end.  */
   void (* handle_pch) (MtcsDebug *self,unsigned int);

   /* Called from final_scan_insn for any NOTE_INSN_VAR_LOCATION note.  */
   void (* var_location) (MtcsDebug *self,rtx_insn *);

   /* Called from final_scan_insn for any NOTE_INSN_INLINE_ENTRY note.  */
   void (* inline_entry) (MtcsDebug *self,tree block);

   /* Called from finalize_size_functions for size functions so that their body
   can be encoded in the debug info to describe the layout of variable-length
   structures.  */
   void (* size_function) (MtcsDebug *self,tree decl);

   /* Called from final_scan_insn if there is a switch between hot and cold
   text sections.  */
   void (* switch_text_section) (MtcsDebug *self);

   /* Called from grokdeclarator.  Replaces the anonymous name with the
   type name.  */
   void (* set_name) (MtcsDebug *self,tree, tree);

   /* This is 1 if the debug writer wants to see start and end commands for the
   main source files, and 0 otherwise.  */
   int start_end_main_source_file;

   /* The type of symtab field used by these debug hooks.  This is one
   of the TYPE_SYMTAB_IS_xxx values defined in tree.h.  */
   int tree_type_symtab_field;
};

void mtcs_debug_init(MtcsDebug *self,char *fileName);
void mtcs_debug_finish (MtcsDebug *self,const char *main_filename );
void mtcs_debug_early_finish (MtcsDebug *self,const char *main_filename );
void mtcs_debug_assembly_start (MtcsDebug *self);
void mtcs_debug_define (MtcsDebug *self,unsigned int line , const char *text );
void mtcs_debug_undef (MtcsDebug *self,unsigned int line , const char *text );
void mtcs_debug_start_source_file (MtcsDebug *self,unsigned int line , const char *text );
void mtcs_debug_end_source_file (MtcsDebug *self,unsigned int line );
void mtcs_debug_begin_block (MtcsDebug *self,unsigned int line , unsigned int n , tree block );
void mtcs_debug_end_block (MtcsDebug *self,unsigned int line , unsigned int n);
bool mtcs_debug_ignore_block (MtcsDebug *self,const_tree block );
void mtcs_debug_source_line (MtcsDebug *self,unsigned int line, unsigned int column, const char *file, int discriminator, bool is_stmt);
void mtcs_debug_set_ignored_loc (MtcsDebug *self, unsigned int line , unsigned int column , const char *text );
void mtcs_debug_begin_prologue (MtcsDebug *self, unsigned int line , unsigned int column , const char *text );
void mtcs_debug_end_prologue (MtcsDebug *self,unsigned int line , const char *text );
void mtcs_debug_begin_epilogue(MtcsDebug *self,unsigned int line , const char *text );
void mtcs_debug_end_epilogue (MtcsDebug *self,unsigned int line , const char *text );
void mtcs_debug_begin_function (MtcsDebug *self,tree decl );
void mtcs_debug_end_function (MtcsDebug *self,unsigned int line );
void mtcs_debug_register_main_translation_unit (MtcsDebug *self,tree decl );
void mtcs_debug_function_decl (MtcsDebug *self,tree decl );
void mtcs_debug_early_global_decl (MtcsDebug *self,tree decl );
void mtcs_debug_late_global_decl (MtcsDebug *self,tree decl );
void mtcs_debug_type_decl (MtcsDebug *self,tree decl ,int local );
void mtcs_debug_imported_module_or_decl (MtcsDebug *self,tree t1 ,tree t2 ,tree t3 ,  bool b1 ,  bool b2 );
bool mtcs_debug_die_ref_for_decl (MtcsDebug *self,tree, const char **, unsigned HOST_WIDE_INT *);
void rmtcs_debug_register_external_die (MtcsDebug *self, tree, const char *,unsigned HOST_WIDE_INT);
void mtcs_debug_deferred_inline_function (MtcsDebug *self,tree decl );
void mtcs_debug_outlining_inline_function (MtcsDebug *self,tree decl );
void mtcs_debug_label (MtcsDebug *self,rtx_code_label *label );
void mtcs_debug_handle_pch (MtcsDebug *self,unsigned int line );
void mtcs_debug_var_location (MtcsDebug *self,rtx_insn *insn );
void mtcs_debug_inline_entry (MtcsDebug *self, tree decl );
void mtcs_debug_size_function (MtcsDebug *self, tree decl );
void mtcs_debug_switch_text_section (MtcsDebug *self);
void mtcs_debug_set_name (MtcsDebug *self,tree t1 ,  tree t2 );

#endif


