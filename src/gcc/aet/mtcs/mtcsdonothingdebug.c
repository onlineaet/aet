#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "rtl.h"
#include "tree.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "insn-config.h"
#include "ira.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "rtlhash.h"
#include "reload.h"
#include "output.h"
#include "expr.h"
#include "dwarf2out.h"
#include "dwarf2ctf.h"
#include "dwarf2codeview.h"
#include "dwarf2asm.h"
#include "toplev.h"
#include "md5.h"
#include "tree-pretty-print.h"
#include "print-rtl.h"
#include "debug.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "lra.h"
#include "dumpfile.h"
#include "opts.h"
#include "tree-dfa.h"
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

#include "gdb/gdb-index.h"
#include "rtl-iter.h"
#include "stringpool.h"
#include "attribs.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */

#include "mtcsdonothingdebug.h"
#include "mtcstarget.h"

static void init_cb (MtcsDebug *mtcsDebug,const char *main_filename ATTRIBUTE_UNUSED)
{
}

static void finish_cb (MtcsDebug *mtcsDebug,const char *main_filename ATTRIBUTE_UNUSED)
{
}

static void earlyFinish_cb (MtcsDebug *mtcsDebug,const char *main_filename ATTRIBUTE_UNUSED)
{
}

static void assemblyStart_cb (MtcsDebug *mtcsDebug)
{
}

static void define_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}

static void undef_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}


static void startSourceFile_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}


static void endSourceFile_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED)
{
}

static void beginBlock_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED,
             unsigned int n ATTRIBUTE_UNUSED, tree block ATTRIBUTE_UNUSED)
{
}

static void endBlock_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED,
             unsigned int n ATTRIBUTE_UNUSED,  tree block ATTRIBUTE_UNUSED)
{
}

static bool ignoreBlock_cb (MtcsDebug *mtcsDebug,const_tree block ATTRIBUTE_UNUSED)
{
   return true;
}

static void sourceLine_cb (MtcsDebug *mtcsDebug,unsigned int, unsigned int, const char *, int, bool)
{

}


static void setIgnoredLoc_cb (MtcsDebug *mtcsDebug, unsigned int line ATTRIBUTE_UNUSED,
            unsigned int column ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}

static void beginPrologue_cb (MtcsDebug *mtcsDebug, unsigned int line ATTRIBUTE_UNUSED,
            unsigned int column ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}

static void endPrologue_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}

static void beginEpilogue_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}

static void endEpilogue_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED, const char *text ATTRIBUTE_UNUSED)
{
}

static void beginFunction_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void endFunction_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED)
{
}

static void registerMainTranslationUnit_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void functionDecl_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void earlyGlobalDecl_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void lateGlobalDecl_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void typeDecl_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED,int local ATTRIBUTE_UNUSED)
{
}

static void importedModuleOrDecl_cb (MtcsDebug *mtcsDebug,tree t1 ATTRIBUTE_UNUSED,
               tree t2 ATTRIBUTE_UNUSED,
               tree t3 ATTRIBUTE_UNUSED,
               bool b1 ATTRIBUTE_UNUSED,
               bool b2 ATTRIBUTE_UNUSED)
{
}

static bool dieRefForDecl_cb (MtcsDebug *mtcsDebug,tree, const char **,
               unsigned HOST_WIDE_INT *)
{
  return false;
}

static void  registerExternalDie_cb (MtcsDebug *mtcsDebug, tree, const char *,unsigned HOST_WIDE_INT)
{
}

static void deferredInlineFunction_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void outliningInlineFunction_cb (MtcsDebug *mtcsDebug,tree decl ATTRIBUTE_UNUSED)
{
}

static void label_cb (MtcsDebug *mtcsDebug,rtx_code_label *label ATTRIBUTE_UNUSED)
{
}

static void handlePch_cb (MtcsDebug *mtcsDebug,unsigned int line ATTRIBUTE_UNUSED)
{
}

static void varLocation_cb (MtcsDebug *mtcsDebug,rtx_insn *insn ATTRIBUTE_UNUSED)
{
}

static void inlineEntry_cb (MtcsDebug *mtcsDebug, tree decl ATTRIBUTE_UNUSED)
{
}

static void sizeFunction_cb (MtcsDebug *mtcsDebug, tree decl ATTRIBUTE_UNUSED)
{
}

static void switchTextSection_cb (MtcsDebug *mtcsDebug)
{
}

static void setName_cb (MtcsDebug *mtcsDebug,tree t1 ATTRIBUTE_UNUSED,  tree t2 ATTRIBUTE_UNUSED)
{
}




static void mtcsDothingDebugInit(MtcsDoNothingDebug *self)
{
   MtcsDebug *mtcsDebug=(MtcsDebug*)self;
   mtcsDebug->init = init_cb;
   mtcsDebug->finish = finish_cb;
   mtcsDebug->early_finish = earlyFinish_cb;
   mtcsDebug->assembly_start = assemblyStart_cb;
   mtcsDebug->define = define_cb;
   mtcsDebug->undef = undef_cb;
   mtcsDebug->start_source_file = startSourceFile_cb;
   mtcsDebug->end_source_file=endSourceFile_cb;
   mtcsDebug->begin_block =beginBlock_cb;
   mtcsDebug->end_block =endBlock_cb;
   mtcsDebug->ignore_block =ignoreBlock_cb;
   mtcsDebug->source_line =sourceLine_cb;
   mtcsDebug->set_ignored_loc =setIgnoredLoc_cb;
   mtcsDebug->begin_prologue =beginPrologue_cb;
   mtcsDebug->end_prologue =endPrologue_cb;
   mtcsDebug->begin_epilogue =beginEpilogue_cb;
   mtcsDebug->end_epilogue =endEpilogue_cb;
   mtcsDebug->begin_function =beginFunction_cb;
   mtcsDebug->end_function =endFunction_cb;
   mtcsDebug->register_main_translation_unit =registerMainTranslationUnit_cb;
   mtcsDebug->function_decl =functionDecl_cb;
   mtcsDebug->early_global_decl =earlyGlobalDecl_cb;
   mtcsDebug->late_global_decl =lateGlobalDecl_cb;
   mtcsDebug->type_decl =typeDecl_cb;
   mtcsDebug->imported_module_or_decl =importedModuleOrDecl_cb;
   mtcsDebug->die_ref_for_decl =dieRefForDecl_cb;
   mtcsDebug->register_external_die =registerExternalDie_cb;
   mtcsDebug->deferred_inline_function =deferredInlineFunction_cb;
   mtcsDebug->outlining_inline_function =outliningInlineFunction_cb;
   mtcsDebug->label =label_cb;
   mtcsDebug->handle_pch =handlePch_cb;
   mtcsDebug->var_location =varLocation_cb;
   mtcsDebug->inline_entry =inlineEntry_cb;
   mtcsDebug->size_function =sizeFunction_cb;
   mtcsDebug->switch_text_section =switchTextSection_cb;
   mtcsDebug->set_name =setName_cb;
   mtcsDebug->start_end_main_source_file =0;
   mtcsDebug->tree_type_symtab_field =TYPE_SYMTAB_IS_ADDRESS;
}

MtcsDoNothingDebug *mtcs_do_nothing_debug_new()
{
   MtcsDoNothingDebug *self = n_slice_alloc0 (sizeof(MtcsDoNothingDebug));
   mtcsDothingDebugInit(self);
   return self;
}
