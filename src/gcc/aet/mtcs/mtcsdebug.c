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
 * base on debug.cc
 */

#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"

#include "aet/aetprinttree.h"
#include "mtcsdebug.h"

void mtcs_debug_init(MtcsDebug *self,char *fileName)
{
   self->init(self,fileName);
}

void mtcs_debug_finish (MtcsDebug *self,const char *main_filename )
{
   self->finish(self,main_filename);
}

void mtcs_debug_early_finish (MtcsDebug *self,const char *main_filename )
{
   self->early_finish(self,main_filename);
}

void mtcs_debug_assembly_start (MtcsDebug *self)
{
   self->assembly_start(self);
}

void mtcs_debug_define (MtcsDebug *self,unsigned int line , const char *text )
{
   self->define(self,line,text);
}

void mtcs_debug_undef (MtcsDebug *self,unsigned int line , const char *text )
{
   self->undef(self,line,text);
}

void mtcs_debug_start_source_file (MtcsDebug *self,unsigned int line , const char *text )
{
   self->start_source_file(self,line,text);
}

void mtcs_debug_end_source_file (MtcsDebug *self,unsigned int line )
{
   self->end_source_file (self,line);

}
void mtcs_debug_begin_block (MtcsDebug *self,unsigned int line , unsigned int n , tree block )
{
   self->begin_block (self,line,n,block);
}

void mtcs_debug_end_block (MtcsDebug *self,unsigned int line , unsigned int n)
{
   self->end_block (self,line,n);
}

bool mtcs_debug_ignore_block (MtcsDebug *self,const_tree block )
{
   return self->ignore_block (self,block);
}

void mtcs_debug_source_line (MtcsDebug *self,unsigned int line, unsigned int column, const char *file, int discriminator, bool is_stmt)
{
   self->source_line (self,line,column,file,discriminator,is_stmt);
}

void mtcs_debug_set_ignored_loc (MtcsDebug *self, unsigned int line , unsigned int column , const char *text )
{
   self->set_ignored_loc (self,line,column,text);
}

void mtcs_debug_begin_prologue (MtcsDebug *self, unsigned int line , unsigned int column , const char *text )
{
   self->begin_prologue (self,line,column,text);
}

void mtcs_debug_end_prologue (MtcsDebug *self,unsigned int line , const char *text )
{
   self->end_prologue (self,line,text);

}
void mtcs_debug_begin_epilogue(MtcsDebug *self,unsigned int line , const char *text )
{
   self->begin_epilogue (self,line,text);
}

void mtcs_debug_end_epilogue (MtcsDebug *self,unsigned int line , const char *text )
{
   self->end_epilogue (self,line,text);

}
void mtcs_debug_begin_function (MtcsDebug *self,tree decl )
{
   self->begin_function (self,decl);

}

void mtcs_debug_end_function (MtcsDebug *self,unsigned int line )
{
   self->end_function (self,line);
}

void mtcs_debug_register_main_translation_unit (MtcsDebug *self,tree decl )
{
   self->register_main_translation_unit (self,decl);
}

void mtcs_debug_function_decl (MtcsDebug *self,tree decl )
{
   self->function_decl (self,decl);
}

void mtcs_debug_early_global_decl (MtcsDebug *self,tree decl )
{
   self->early_global_decl (self,decl);
}

void mtcs_debug_late_global_decl (MtcsDebug *self,tree decl )
{
   self->late_global_decl (self,decl);
}

void mtcs_debug_type_decl (MtcsDebug *self,tree decl ,int local )
{
   self->type_decl (self,decl,local);
}

void mtcs_debug_imported_module_or_decl (MtcsDebug *self,tree t1 ,tree t2 ,tree t3 ,  bool b1 ,  bool b2 )
{
   self->imported_module_or_decl (self,t1,t2,t3,b1,b2);
}

bool mtcs_debug_die_ref_for_decl (MtcsDebug *self,tree t1, const char **sym, unsigned HOST_WIDE_INT *value)
{
   self->die_ref_for_decl (self,t1,sym,value);
}

void mtcs_debug_register_external_die (MtcsDebug *self, tree t1, const char *sym,unsigned HOST_WIDE_INT value)
{
   self->register_external_die (self,t1,sym,value);
}

void mtcs_debug_deferred_inline_function (MtcsDebug *self,tree decl )
{
   self->deferred_inline_function (self,decl);
}

void mtcs_debug_outlining_inline_function (MtcsDebug *self,tree decl )
{
   self->outlining_inline_function (self,decl);
}

void mtcs_debug_label (MtcsDebug *self,rtx_code_label *label )
{
   self->label (self,label);
}

void mtcs_debug_handle_pch (MtcsDebug *self,unsigned int line )
{
   self->handle_pch (self,line);
}

void mtcs_debug_var_location (MtcsDebug *self,rtx_insn *insn )
{
   self->var_location (self,insn);
}

void mtcs_debug_inline_entry (MtcsDebug *self, tree decl )
{
   self->inline_entry  (self,decl);

}

void mtcs_debug_size_function (MtcsDebug *self, tree decl )
{
   self->size_function  (self,decl);
}

void mtcs_debug_switch_text_section (MtcsDebug *self)
{
   self->switch_text_section  (self);
}

void mtcs_debug_set_name (MtcsDebug *self,tree t1 ,  tree t2 )
{
   self->set_name  (self,t1,t2);
}



