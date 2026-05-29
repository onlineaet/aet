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
 * base on final.cc
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
#include "stmt.h"

#include "dwarf2out.h"

#include "aet/aetprinttree.h"
#include "mtcsfinal.h"
#include "mtcstarget.h"
#include "mtcstool.h"
#include "mtcsasm.h"
#include "mtcsexcept.h"
#include "mtcsoutput.h"
#include "mtcscompile.h"
#include "mtcsprintrtl.h"

/* Most ports don't need to define CC_STATUS_INIT.
   So define a null default for it to save conditionalization later.  */
#ifndef CC_STATUS_INIT
#define CC_STATUS_INIT
#endif


/* Bitflags used by final_scan_insn.  */
#define SEEN_NOTE   1
#define SEEN_EMITTED    2
#define SEEN_NEXT_VIEW  4

#define LABEL_TO_ALIGNMENT(LABEL) \
  (self->label_align[CODE_LABEL_NUMBER (LABEL) - self->min_labelno])

#define INSN_SHUID(INSN) (self->uid_shuid[INSN_UID (INSN)])

static inline bool in_initial_view_p (rtx_insn *insn)
{
   return (!DECL_IGNORED_P (current_function_decl)
      && debug_variable_location_views  && insn && GET_CODE (insn) == NOTE  &&
      (NOTE_KIND (insn) == NOTE_INSN_VAR_LOCATION  || NOTE_KIND (insn) == NOTE_INSN_DELETED));
}

static bool dwarf2_debug_info_emitted_p (MtcsFinal *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   /* When DWARF2 debug info is not generated internally.  */
   n_debug("mtcsfinal.cc dwarf2_debug_info_emitted_p %d %d %d %d %d\n",
         mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(mtcsOpts,mtcsOptionsItem),dwarf_based_debuginfo_p (),
         mtcsOptionsItem->x_write_symbols & DWARF2_DEBUG,mtcsOptionsItem->x_write_symbols,DWARF2_DEBUG);
   if (!mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(mtcsOpts,mtcsOptionsItem) && !dwarf_based_debuginfo_p ()){
      n_debug("final.cc dwarf2_debug_info_emitted_p 00 %d %d\n",mtcsOptionsItem->x_write_symbols,DWARF2_DEBUG);
      return false;
   }

   if (DECL_IGNORED_P (decl))
      return false;

   return true;
}

/* Return the instance number assigned to DECL.  */
//decl_to_instance_map_t *decl_to_instance_map; final.cc中定义了
static inline int map_decl_to_instance (const_tree decl)
{
   int *inst;
   if (!decl_to_instance_map || !decl || !DECL_P (decl))
      return 0;
   inst = decl_to_instance_map->get (decl);
   if (!inst)
      return 0;
   return *inst;
}

static inline int compute_discriminator (location_t loc)
{
   int discriminator;

   if (!decl_to_instance_map)
      discriminator = get_discriminator_from_loc (loc);
   else{
      tree block = LOCATION_BLOCK (loc);
      while (block && TREE_CODE (block) == BLOCK  && !inlined_function_outer_scope_p (block))
      block = BLOCK_SUPERCONTEXT (block);

      tree decl;
      if (!block)
         decl = current_function_decl;
      else if (DECL_P (block))
         decl = block;
      else
         decl = block_ultimate_origin (block);

      discriminator = map_decl_to_instance (decl);
   }
   return discriminator;
}

/* Clear the flag in *SEEN indicating we need to emit the next view.
   This should be called next to the source_line debug hook.  */
static inline void clear_next_view_needed (int *seen)
{
   *seen &= ~SEEN_NEXT_VIEW;
}

static inline void set_next_view_needed (int *seen)
{
   if (debug_variable_location_views)
      *seen |= SEEN_NEXT_VIEW;
}

/* Test whether we have a pending request to emit the next view in
   *SEEN, and emit it if needed, clearing the request bit.  */
static inline void maybe_output_next_view (MtcsFinal *self,int *seen)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);

   if ((*seen & SEEN_NEXT_VIEW) != 0) {
      clear_next_view_needed (seen);
      mtcs_debug_source_line/*!(*debug_hooks->source_line)*/(mtcsDebug,self->last_linenum,
            self->last_columnnum,self->last_filename, self->last_discriminator, false);
   }
}

/* Given a CALL_INSN, find and return the nested CALL. */
static rtx call_from_call_insn (rtx_call_insn *insn)
{
   rtx x;
   gcc_assert (CALL_P (insn));
   x = PATTERN (insn);
   while (GET_CODE (x) != CALL){
      switch (GET_CODE (x)){
         default:
            gcc_unreachable ();
         case COND_EXEC:
            x = COND_EXEC_CODE (x);
            break;
         case PARALLEL:
            x = XVECEXP (x, 0, 0);
            break;
         case SET:
            x = XEXP (x, 1);
            break;
      }
   }
   return x;
}

//原型 app_enable output.h final.cc
void mtcs_final_app_enable(MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   if (! self->app_on){
      mtcs_asm_app_on(mtcsAsm);
      self->app_on = 1;
   }
}

//原型 app_disable output.h final.cc
void mtcs_final_app_disable(MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   if (self->app_on){
      mtcs_asm_app_off(mtcsAsm);
      self->app_on = 0;
   }
}

/* Judge if an absolute jump table is relocatable.  */
static bool jumptable_relocatable (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   bool relocatable = false;
   if (!CASE_VECTOR_PC_RELATIVE
         && !target_asm_out_generate_pic_addr_diff_vec/*!targetm.asm_out.generate_pic_addr_diff_vec*/(mtcsMachine->asmOut)
         && mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/)
      relocatable = target_asm_out_reloc_rw_mask/*!targetm.asm_out.reloc_rw_mask*/(mtcsMachine->asmOut);
   return relocatable;
}

/* Return true if INSN is a call to the current function.  */
static bool self_recursive_call_p (rtx_insn *insn)
{
   tree fndecl = get_call_fndecl (insn);
   return (fndecl == current_function_decl  && decl_binds_to_current_def_p (fndecl));
}

/* Emit lexical block notes needed to change scope from S1 to S2.  */
static void change_scope (MtcsFinal *self,rtx_insn *orig_insn, tree s1, tree s2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   rtx_insn *insn = orig_insn;
   tree com = NULL_TREE;
   tree ts1 = s1, ts2 = s2;
   tree s;
   while (ts1 != ts2){
      gcc_assert (ts1 && ts2);
      if (BLOCK_NUMBER (ts1) > BLOCK_NUMBER (ts2))
         ts1 = BLOCK_SUPERCONTEXT (ts1);
      else if (BLOCK_NUMBER (ts1) < BLOCK_NUMBER (ts2))
         ts2 = BLOCK_SUPERCONTEXT (ts2);
      else{
         ts1 = BLOCK_SUPERCONTEXT (ts1);
         ts2 = BLOCK_SUPERCONTEXT (ts2);
      }
   }
   com = ts1;

   /* Close scopes.  */
   s = s1;
   while (s != com){
      rtx_note *note = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_BLOCK_END, insn);
      NOTE_BLOCK (note) = s;
      s = BLOCK_SUPERCONTEXT (s);
   }
   /* Open scopes.  */
   s = s2;
   while (s != com){
      insn = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_BLOCK_BEG, insn);
      NOTE_BLOCK (insn) = s;
      s = BLOCK_SUPERCONTEXT (s);
   }
}

/* Return scope resulting from combination of S1 and S2.  */
static tree choose_inner_scope (tree s1, tree s2)
{
   if (!s1)
      return s2;
   if (!s2)
      return s1;
   if (BLOCK_NUMBER (s1) > BLOCK_NUMBER (s2))
      return s1;
   return s2;
}

/* Rebuild all the NOTE_INSN_BLOCK_BEG and NOTE_INSN_BLOCK_END notes based
   on the scope tree and the newly reordered instructions.  */
static void reemit_insn_block_notes (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   tree cur_block = DECL_INITIAL (cfun->decl);
   rtx_insn *insn;

   insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   for (; insn; insn = NEXT_INSN (insn)){
      tree this_block;

      /* Prevent lexical blocks from straddling section boundaries.  */
      if (NOTE_P (insn))
         switch (NOTE_KIND (insn)){
            case NOTE_INSN_SWITCH_TEXT_SECTIONS:
            {
               for (tree s = cur_block; s != DECL_INITIAL (cfun->decl); s = BLOCK_SUPERCONTEXT (s)){
                  rtx_note *note = mtcs_emit_emit_note_before/*!emit_note_before*/(mtcsEmit,NOTE_INSN_BLOCK_END, insn);
                  NOTE_BLOCK (note) = s;
                  note = mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_BLOCK_BEG, insn);
                  NOTE_BLOCK (note) = s;
               }
            }
            break;

            case NOTE_INSN_BEGIN_STMT:
            case NOTE_INSN_INLINE_ENTRY:
               this_block = LOCATION_BLOCK (NOTE_MARKER_LOCATION (insn));
               goto set_cur_block_to_this_block;
            default:
               continue;
         }

      if (!active_insn_p (insn))
         continue;

      /* Avoid putting scope notes between jump table and its label.  */
      if (JUMP_TABLE_DATA_P (insn))
         continue;

      this_block = insn_scope (insn);
      /* For sequences compute scope resulting from merging all scopes
      of instructions nested inside.  */
      if (rtx_sequence *body = dyn_cast <rtx_sequence *> (PATTERN (insn))){
         int i;
         this_block = NULL;
         for (i = 0; i < body->len (); i++)
            this_block = choose_inner_scope (this_block,insn_scope (body->insn (i)));
      }
set_cur_block_to_this_block:
      if (! this_block){
         if (INSN_LOCATION (insn) == UNKNOWN_LOCATION)
            continue;
         else
            this_block = DECL_INITIAL (cfun->decl);
      }

      if (this_block != cur_block){
         change_scope(self,insn, cur_block, this_block);
         cur_block = this_block;
      }
   }

   /* change_scope emits before the insn, not after.  */
   rtx_note *note = mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_DELETED);
   change_scope(self,note, cur_block, DECL_INITIAL (cfun->decl));
   mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,note);
   reorder_blocks ();
}

static void profile_function (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   int noProfileCounters =1;
   if(mtcs_config_ifndef(mtcsConfig,MTCS_NO_PROFILE_COUNTERS))
      noProfileCounters = 0;
   /*!
   #ifndef NO_PROFILE_COUNTERS
   # define NO_PROFILE_COUNTERS    0
   #endif
   */
   rtx sval = NULL, chain = NULL;
   if(mtcsAsm->output_reg_push/*!#ifdef ASM_OUTPUT_REG_PUSH host=1 nvptx=0*/){
      if (cfun->returns_struct)
         sval = target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,
               TREE_TYPE (current_function_decl),true);
      if (cfun->static_chain_decl)
         chain = target_calls_static_chain/*!targetm.calls.static_chain*/(mtcsMachine->calls,current_function_decl, true);
   }
   /*!
   #ifdef ASM_OUTPUT_REG_PUSH host=1 nvptx=0
   rtx sval = NULL, chain = NULL;

   if (cfun->returns_struct)
   sval = targetm.calls.struct_value_rtx (TREE_TYPE (current_function_decl),
     true);
   if (cfun->static_chain_decl)
   chain = targetm.calls.static_chain (current_function_decl, true);
   #endif
   */
   if (! noProfileCounters/*!NO_PROFILE_COUNTERS*/){
      int align = MIN (mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign), LONG_TYPE_SIZE);
      mtcs_asm_switch_to_section (mtcsAsm,data_section);
      mtcsAsm->output_align/*ASM_OUTPUT_ALIGN (file,*/(mtcsAsm, floor_log2 (align / BITS_PER_UNIT));
      target_asm_out_internal_label/*!targetm.asm_out.internal_label*/(mtcsMachine->asmOut, "LP", current_function_funcdef_no);
      mtcs_asm_assemble_integer (mtcsAsm,const0_rtx, LONG_TYPE_SIZE / BITS_PER_UNIT, align, 1);
   }

   mtcs_asm_switch_to_section (mtcsAsm,mtcs_asm_current_function_section/*!current_function_section*/(mtcsAsm));

   if(mtcsAsm->output_reg_push/*!#ifdef ASM_OUTPUT_REG_PUSH host=1 nvptx=0*/){
      if (sval && REG_P (sval))
         mtcsAsm->output_reg_push/*!ASM_OUTPUT_REG_PUSH (file */(mtcsAsm,REGNO(sval));
      if (chain && REG_P (chain))
         mtcsAsm->output_reg_push/*!ASM_OUTPUT_REG_PUSH (file */(mtcsAsm,REGNO(chain));
   }
   /*
   #ifdef ASM_OUTPUT_REG_PUSH host=1 nvptx=0
   if (sval && REG_P (sval))
   ASM_OUTPUT_REG_PUSH (file, REGNO (sval));
   if (chain && REG_P (chain))
   ASM_OUTPUT_REG_PUSH (file, REGNO (chain));
   #endif
   */

   mtcsTarget->function_profiler/*!FUNCTION_PROFILER*/(mtcsTarget, current_function_funcdef_no);

   if(mtcsAsm->output_reg_push/*!#ifdef ASM_OUTPUT_REG_PUSH host=1 nvptx=0*/){
      if (chain && REG_P (chain))
         mtcsAsm->output_reg_pop/*!ASM_OUTPUT_REG_POP (file */(mtcsAsm,REGNO(chain));
      if (sval && REG_P (sval))
         mtcsAsm->output_reg_pop/*!ASM_OUTPUT_REG_POP (file */(mtcsAsm,REGNO(sval));
   }
   /*!
   #ifdef ASM_OUTPUT_REG_PUSH host=1 nvptx=0
   if (chain && REG_P (chain))
   ASM_OUTPUT_REG_POP (file, REGNO (chain));
   if (sval && REG_P (sval))
   ASM_OUTPUT_REG_POP (file, REGNO (sval));
   #endif
   */
}

static void profile_after_prologue (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   if (!mtcsTarget->profile_before_prologue/*targetm.profile_before_prologue*/ (mtcsTarget) && mtcsRtlData/*!crtl*/->profile)
      profile_function (self);
}

/* Return whether a source line note needs to be emitted before INSN.
   Sets IS_STMT to TRUE if the line should be marked as a possible
   breakpoint location.  */
static bool notice_source_line (MtcsFinal *self,rtx_insn *insn, bool *is_stmt)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   const char *filename;
   int linenum, columnnum;
   int discriminator;

   if (NOTE_MARKER_P (insn)){
      location_t loc = NOTE_MARKER_LOCATION (insn);
      expanded_location xloc = expand_location (loc);
      if (xloc.line == 0 && (LOCATION_LOCUS (loc) == UNKNOWN_LOCATION || LOCATION_LOCUS (loc) == BUILTINS_LOCATION)){
         n_debug("位置是0---\n");
         return false;
      }
      filename = xloc.file;
      linenum = xloc.line;
      columnnum = xloc.column;
      discriminator = compute_discriminator (loc);
      self->force_source_line = true;
      n_debug("notice_source_line 00---%s %d %d %d\n",filename,linenum,columnnum,discriminator);
   }else if (self->override_filename){
      filename = self->override_filename;
      linenum = self->override_linenum;
      columnnum =self->override_columnnum;
      discriminator =self->override_discriminator;
      n_debug("notice_source_line 11---%s %d %d %d\n",filename,linenum,columnnum,discriminator);
   }else if (INSN_HAS_LOCATION (insn)){
      expanded_location xloc = insn_location (insn);
      filename = xloc.file;
      linenum = xloc.line;
      columnnum = xloc.column;
      discriminator = insn_discriminator (insn);
      n_debug("notice_source_line 22---%s %d %d %d\n",filename,linenum,columnnum,discriminator);
   }else{
      filename = NULL;
      linenum = 0;
      columnnum = 0;
      discriminator = 0;
      n_debug("notice_source_line 33---%s %d %d %d\n",filename,linenum,columnnum,discriminator);
   }

   if (filename == NULL){
      n_debug("notice_source_line 文件是空的---\n");
      return false;
   }
   n_debug("notice_source_line ---%s %d %d %d\n",filename,linenum,columnnum,discriminator);
   if (self->force_source_line  || filename != self->last_filename
   || self->last_linenum != linenum  || (mtcsOptionsItem->x_debug_column_info && self->last_columnnum != columnnum)){
      self->force_source_line = false;
      self->last_filename = filename;
      self->last_linenum = linenum;
      self->last_columnnum = columnnum;
      self->last_discriminator = discriminator;
      if (is_stmt)
         *is_stmt = true;
      self->high_block_linenum = MAX (self->last_linenum, self->high_block_linenum);
      self->high_function_linenum = MAX (self->last_linenum, self->high_function_linenum);
      return true;
   }
   int supportsDiscriminator = mtcs_config_ifdef(mtcsConfig,MTCS_HAVE_GAS_DISCRIMINATOR)?1:0;
   if (supportsDiscriminator/*!SUPPORTS_DISCRIMINATOR*/ && self->last_discriminator != discriminator){
      /* If the discriminator changed, but the line number did not,
      output the line table entry with is_stmt false so the
      debugger does not treat this as a breakpoint location.  */
      self->last_discriminator = discriminator;
      if (is_stmt)
         *is_stmt = false;
      return true;
   }

   return false;
}


/* Emit the appropriate declaration for an alternate-entry-point
   symbol represented by INSN, to FILE.  INSN is a CODE_LABEL with
   LABEL_KIND != LABEL_NORMAL.

   The case fall-through in this function is intentional.  */
static void output_alternate_entry_point (MtcsFinal *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   const char *name = LABEL_NAME (insn);
   switch (LABEL_KIND (insn)){
      case LABEL_WEAK_ENTRY:
         if(mtcsAsm->weaken_label){
         mtcsAsm->weaken_label(mtcsAsm,name);
         gcc_fallthrough ();
         }
         /*!
         #ifdef ASM_WEAKEN_LABEL host=1 nvptx=0
         ASM_WEAKEN_LABEL (file, name);
         gcc_fallthrough ();
         #endif
         */
      case LABEL_GLOBAL_ENTRY:
         target_asm_out_globalize_label/*targetm.asm_out.globalize_label*/(mtcsMachine->asmOut, name);
         gcc_fallthrough ();
      case LABEL_STATIC_ENTRY:
         if(mtcsAsm->output_type_directive)
            mtcsAsm->output_type_directive(mtcsAsm,name,"function");
         /*!
         #ifdef ASM_OUTPUT_TYPE_DIRECTIVE
         ASM_OUTPUT_TYPE_DIRECTIVE (file, name, "function");
         #endif
         */
         mtcsAsm->output_label/*ASM_OUTPUT_LABEL (file,*/(mtcsAsm, name);
         break;

      case LABEL_NORMAL:
      default:
         gcc_unreachable ();
   }
}

/* Print a comment into the asm showing FILENAME, LINENUM, and the
   corresponding source line, if available.  */
static void asm_show_source (MtcsFinal *self,const char *filename, int linenum)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   if (!filename)
      return;

   char_span line= global_dc->get_file_cache ().get_source_line (filename, linenum);
   if (!line)
      return;

   fprintf (mtcsAsm->asmFile, "%s %s:%i: ", mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm), filename, linenum);
   /* "line" is not 0-terminated, so we must use its length.  */
   fwrite (line.get_buffer (), 1, line.length (), mtcsAsm->asmFile);
   fputc ('\n', mtcsAsm->asmFile);
}

/* Dumper helper for basic block information. FILE is the assembly
   output file, and INSN is the instruction being emitted.  */
static void dump_basic_block_info (MtcsFinal *self,FILE *file, rtx_insn *insn, basic_block *start_to_bb,
                                       basic_block *end_to_bb, int bb_map_size, int *bb_seqn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   char *asmCommentStart = mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm);
   basic_block bb;
   if (!mtcsOptionsItem->x_flag_debug_asm)
      return;

   if (INSN_UID (insn) < bb_map_size && (bb = start_to_bb[INSN_UID (insn)]) != NULL){
      edge e;
      edge_iterator ei;
      fprintf (file, "%s BLOCK %d", asmCommentStart/*!ASM_COMMENT_START*/, bb->index);
      if (bb->count.initialized_p ()){
         fprintf (file, ", count:");
         bb->count.dump (file);
      }
      fprintf (file, " seq:%d", (*bb_seqn)++);
      fprintf (file, "\n%s PRED:", asmCommentStart/*!ASM_COMMENT_START*/);
      FOR_EACH_EDGE (e, ei, bb->preds){
         dump_edge_info (file, e, TDF_DETAILS, 0);
      }
      fprintf (file, "\n");
   }
   if (INSN_UID (insn) < bb_map_size && (bb = end_to_bb[INSN_UID (insn)]) != NULL){
      edge e;
      edge_iterator ei;

      fprintf (asm_out_file, "%s SUCC:", asmCommentStart/*!ASM_COMMENT_START*/);
      FOR_EACH_EDGE (e, ei, bb->succs){
         dump_edge_info (asm_out_file, e, TDF_DETAILS, 1);
      }
      fprintf (file, "\n");
   }
}

/* Output assembler code for some insns: all or part of a function.
   For description of args, see `final_start_function', above.  */

static void final_1 (MtcsFinal *self,rtx_insn *first, int seen, int optimize_p)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsAddr *mtcsAddr=mtcs_target_get_addr(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx_insn *insn, *next;

  /* Used for -dA dump.  */
  basic_block *start_to_bb = NULL;
  basic_block *end_to_bb = NULL;
  int bb_map_size = 0;
  int bb_seqn = 0;

  self->last_ignored_compare = 0;
  n_debug("mtcsfinal.c final_1 00\n");

  mtcs_recog_init_recog/*!init_recog*/(mtcsRecog);
  n_debug("mtcsfinal.c final_1 11\n");

  CC_STATUS_INIT;
  n_debug("mtcsfinal.c final_1 22\n");

  if (mtcsOptionsItem->x_flag_debug_asm){
      n_debug("mtcsfinal.c final_1 33\n");

      basic_block bb;
      bb_map_size = mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData) + 1;
      start_to_bb = XCNEWVEC (basic_block, bb_map_size);
      end_to_bb = XCNEWVEC (basic_block, bb_map_size);

      /* There is no cfg for a thunk.  */
      if (!cfun->is_thunk){
          n_debug("mtcsfinal.c final_1 44\n");

        FOR_EACH_BB_REVERSE_FN (bb, cfun){
            n_debug("mtcsfinal.c final_1 55\n");
            start_to_bb[INSN_UID (BB_HEAD (bb))] = bb;
            end_to_bb[INSN_UID (BB_END (bb))] = bb;
        }
      }
  }

  /* Output the insns.  */
  for (insn = first; insn;){
      n_debug("mtcsfinal.c final_1 66\n");
      if (mtcs_insn_attr_get_have_attr_length/*!HAVE_ATTR_length*/(mtcsInsnAttr)){
          n_debug("mtcsfinal.c final_1 77\n");

          if ((unsigned) INSN_UID (insn) >=mtcs_addr_insn_addresses_size/*!INSN_ADDRESSES_SIZE*/(mtcsAddr)){
              n_debug("mtcsfinal.c final_1 88\n");

              /* This can be triggered by bugs elsewhere in the compiler if
             new insns are created after init_insn_lengths is called.  */
              gcc_assert (NOTE_P (insn));
              mtcsAddr->insn_current_address/*!insn_current_address*/ = -1;
          }else{
              n_debug("mtcsfinal.c final_1 99\n");

              mtcsAddr->insn_current_address/*!insn_current_address*/ =
                    mtcs_addr_insn_addresses/*!INSN_ADDRESSES*/(mtcsAddr,INSN_UID (insn));
          }
          /* final can be seen as an iteration of shorten_branches that
             does nothing (since a fixed point has already been reached).  */
          self->insn_last_address = mtcsAddr->insn_current_address/*!insn_current_address*/;
      }
//      pretty_printer pp;
//      pp.buffer->stream = stderr;
//      print_insn(&pp,insn,false);
      mtcs_print_rtl_single(stderr,insn);
      n_debug("\n mtcsfinal.c final_1 66aa printrtl end\n");

      dump_basic_block_info(self,mtcsAsm->asmFile, insn, start_to_bb, end_to_bb,bb_map_size, &bb_seqn);
      insn = mtcs_final_final_scan_insn (self,insn, optimize_p, 0, &seen);
  }
  n_debug("mtcsfinal.c final_1 100\n");
  maybe_output_next_view (self,&seen);
  n_debug("mtcsfinal.c final_1 101\n");
  if (mtcsOptionsItem->x_flag_debug_asm){
      n_debug("mtcsfinal.c final_1 102\n");
      free (start_to_bb);
      free (end_to_bb);
  }
  /* Remove CFI notes, to avoid compare-debug failures.  */
  for (insn = first; insn; insn = next){
      next = NEXT_INSN (insn);
      if (NOTE_P (insn) && (NOTE_KIND (insn) == NOTE_INSN_CFI || NOTE_KIND (insn) == NOTE_INSN_CFI_LABEL)){
          n_debug("mtcsfinal.c final_1 103\n");

          mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
      }
  }
}

/* The final scan for one insn, INSN.
   Args are same as in `final', except that INSN
   is the insn being scanned.
   Value returned is the next insn to be scanned.

   NOPEEPHOLES is the flag to disallow peephole processing (currently
   used for within delayed branch sequence output).

   SEEN is used to track the end of the prologue, for emitting
   debug information.  We force the emission of a line note after
   both NOTE_INSN_PROLOGUE_END and NOTE_INSN_FUNCTION_BEG.  */
static rtx_insn * final_scan_insn_1(MtcsFinal *self,rtx_insn *insn,
      int optimize_p ATTRIBUTE_UNUSED,int nopeepholes ATTRIBUTE_UNUSED, int *seen)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsDwarf2Cfi *mtcsDwarf2Cfi=mtcs_target_get_dwarf2_cfi(mtcsTarget);
   MtcsDwarf2Out *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
   MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   char *asmCommentStart = mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm);
   rtx_insn *next;
   rtx_jump_table_data *table;

   mtcs_output_add_insn_count(mtcsOutput,1);
   /* Ignore deleted insns.  These can occur when we split insns (due to a
   template of "#") while not optimizing.  */
   if (insn->deleted ())
      return NEXT_INSN (insn);
   n_debug("mtcsfinal.cc final_scan_insn_1 每条rtx:%p\n",insn);
   mtcs_print_rtl_single(stderr,insn);
   switch (GET_CODE (insn)){
      case NOTE:
         switch (NOTE_KIND (insn)){
            case NOTE_INSN_DELETED:
            case NOTE_INSN_UPDATE_SJLJ_CONTEXT:
               n_debug("mtcsfinal.c final_scan_insn_1 00 NOTE_INSN_DELETED NOTE_INSN_UPDATE_SJLJ_CONTEXT\n");
               break;
            case NOTE_INSN_SWITCH_TEXT_SECTIONS:
               n_debug("mtcsfinal.c final_scan_insn_1 11 NOTE_INSN_SWITCH_TEXT_SECTIONS\n");
               maybe_output_next_view (self,seen);
               mtcs_except_output_function_exception_table (mtcsExcept,0);
               if (mtcsMachine->asmOut->unwind_emit/*!targetm.asm_out.unwind_emit*/)
                  target_asm_out_unwind_emit/*!targetm.asm_out.unwind_emit*/(mtcsMachine->asmOut, insn);

               mtcsAsm->in_cold_section_p = !mtcsAsm->in_cold_section_p;

               gcc_checking_assert (mtcsAsm->in_cold_section_p);
               if (mtcsAsm->in_cold_section_p)
                  mtcsAsm->cold_function_name = clone_function_name (current_function_decl, "cold");

               if (mtcs_dwarf2_cfi_dwarf2out_do_frame (mtcsDwarf2Cfi)){
                  mtcs_dwarf2_out_dwarf2out_switch_text_section/*dwarf2out_switch_text_section*/(mtcsDwarf2Out);
                  if (!dwarf2_debug_info_emitted_p(self,current_function_decl)  && !DECL_IGNORED_P (current_function_decl))
                     mtcs_debug_switch_text_section/*!debug_hooks->switch_text_section*/(mtcsDebug);
               }else if (!DECL_IGNORED_P (current_function_decl))
                  mtcs_debug_switch_text_section/*!debug_hooks->switch_text_section*/(mtcsDebug);
               if (DECL_IGNORED_P (current_function_decl) && self->last_linenum && self->last_filename)
                  mtcs_debug_set_ignored_loc/*!debug_hooks->set_ignored_loc*/(mtcsDebug,
                        self->last_linenum, self->last_columnnum,self->last_filename);

               mtcs_asm_switch_to_section (mtcsAsm,mtcs_asm_current_function_section/*!current_function_section*/(mtcsAsm));
               target_asm_out_function_switched_text_sections/*targetm.asm_out.function_switched_text_sections*/
               (mtcsMachine->asmOut,current_function_decl,mtcsAsm->in_cold_section_p);
               /* Emit a label for the split cold section.  Form label name by
               suffixing "cold" to the original function's name.  */
               if (mtcsAsm->in_cold_section_p){
                  if(mtcsAsm->declare_cold_function_name/*!#ifdef ASM_DECLARE_COLD_FUNCTION_NAME*/)
                     mtcsAsm->declare_cold_function_name(mtcsAsm,IDENTIFIER_POINTER(mtcsAsm->cold_function_name),current_function_decl);
                  else /*!#else*/
                     mtcsAsm->output_label(mtcsAsm,IDENTIFIER_POINTER (mtcsAsm->cold_function_name));
                  /*!#endif*/
                  if (mtcs_dwarf2_cfi_dwarf2out_do_frame (mtcsDwarf2Cfi) && cfun->fde->dw_fde_second_begin != NULL)
                     mtcsAsm->output_label(mtcsAsm, cfun->fde->dw_fde_second_begin);
               }
               break;

            case NOTE_INSN_BASIC_BLOCK:
               n_debug("mtcsfinal.c final_scan_insn_1 22 NOTE_INSN_BASIC_BLOCK\n");
               if (self->need_profile_function){
                  n_debug("mtcsfinal.c final_scan_insn_1 22-1-- if (need_profile_function)\n");
                  profile_function (self);
                  self->need_profile_function = false;
               }
               if (mtcsMachine->asmOut->unwind_emit/*!targetm.asm_out.unwind_emit*/){
                  n_debug("mtcsfinal.c final_scan_insn_1 22-2-- if (targetm.asm_out.unwind_emit)\n");
                  target_asm_out_unwind_emit/*!targetm.asm_out.unwind_emit*/(mtcsMachine->asmOut, insn);
               }
               break;

            case NOTE_INSN_EH_REGION_BEG:
               n_debug("mtcsfinal.c final_scan_insn_1 33 NOTE_INSN_EH_REGION_BEG\n");
               target_asm_out_output_debug_label/*ASM_OUTPUT_DEBUG_LABEL*/(mtcsMachine->asmOut,"LEHB",NOTE_EH_HANDLER (insn));
               break;

            case NOTE_INSN_EH_REGION_END:
               n_debug("mtcsfinal.c final_scan_insn_1 44 NOTE_INSN_EH_REGION_END\n");
               target_asm_out_output_debug_label/*ASM_OUTPUT_DEBUG_LABEL*/(mtcsMachine->asmOut, "LEHE",NOTE_EH_HANDLER (insn));
               break;

            case NOTE_INSN_PROLOGUE_END:
               n_debug("mtcsfinal.c final_scan_insn_1 55 NOTE_INSN_PROLOGUE_END\n");
               target_asm_out_function_end_prologue/*targetm.asm_out.function_end_prologue*/(mtcsMachine->asmOut);
               profile_after_prologue (self);
               if ((*seen & (SEEN_EMITTED | SEEN_NOTE)) == SEEN_NOTE){
                  n_debug("mtcsfinal.c final_scan_insn_1 55-1--  if ((*seen & (SEEN_EMITTED | SEEN_NOTE)) == SEEN_NOTE)\n");
                  *seen |= SEEN_EMITTED;
                  self->force_source_line = true;
               }else
                  *seen |= SEEN_NOTE;

               break;

            case NOTE_INSN_EPILOGUE_BEG:
               n_debug("mtcsfinal.c final_scan_insn_1 66 NOTE_INSN_EPILOGUE_BEG\n");
               if (!DECL_IGNORED_P (current_function_decl))
                  mtcs_debug_begin_epilogue/*!(*debug_hooks->begin_epilogue)*/(mtcsDebug,self->last_linenum, self->last_filename);
               target_asm_out_function_begin_epilogue/*targetm.asm_out.function_begin_epilogue*/(mtcsMachine->asmOut);
               break;

            case NOTE_INSN_CFI:
               n_debug("mtcsfinal.c final_scan_insn_1 77 NOTE_INSN_CFI\n");
               mtcs_dwarf2_cfi_out_emit_cfi (mtcsDwarf2Cfi,NOTE_CFI (insn));
               break;

            case NOTE_INSN_CFI_LABEL:
               n_debug("mtcsfinal.c final_scan_insn_1 88 NOTE_INSN_CFI_LABEL\n");
               target_asm_out_output_debug_label/*ASM_OUTPUT_DEBUG_LABEL*/(mtcsMachine->asmOut, "LCFI",NOTE_LABEL_NUMBER (insn));
               break;

            case NOTE_INSN_FUNCTION_BEG:
               n_debug("mtcsfinal.c final_scan_insn_1 99 NOTE_INSN_FUNCTION_BEG need_profile_function:%d\n",self->need_profile_function);
               if (self->need_profile_function){
                  n_debug("mtcsfinal.c final_scan_insn_1 99-1-- if (need_profile_function)\n");
                  profile_function (self);
                  self->need_profile_function = false;
               }
               mtcs_final_app_disable(self);
               if (!DECL_IGNORED_P (current_function_decl)){
                  n_debug("mtcsfinal.c final_scan_insn_1 99-2-- if (!DECL_IGNORED_P (current_function_decl))\n");
                  mtcs_debug_end_prologue/*!debug_hooks->end_prologue*/(mtcsDebug,self->last_linenum, self->last_filename);
               }
               n_debug("mtcsfinal.c final_scan_insn_1 99-3--seen:%d SEEN_EMITTED:%d SEEN_NOTE:%d\n",*seen,SEEN_EMITTED,SEEN_NOTE);
               if ((*seen & (SEEN_EMITTED | SEEN_NOTE)) == SEEN_NOTE){
                  n_debug("mtcsfinal.c final_scan_insn_1 99-4--  if ((*seen & (SEEN_EMITTED | SEEN_NOTE)) == SEEN_NOTE)\n");
                  *seen |= SEEN_EMITTED;
                  self->force_source_line = true;
               }else
                  *seen |= SEEN_NOTE;

               break;

            case NOTE_INSN_BLOCK_BEG:
               n_debug("mtcsfinal.c final_scan_insn_1 100 NOTE_INSN_BLOCK_BEG\n");
               if (debug_info_level >= DINFO_LEVEL_NORMAL
               || mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(mtcsOpts,mtcsOptionsItem)
               || mtcsOptionsItem->x_write_symbols == VMS_DEBUG){
                  n_debug("mtcsfinal.c final_scan_insn_1 100-1-- if (debug_info_level >= DINFO_LEVEL_NORMAL"
                        "|| dwarf_debuginfo_p ()  || write_symbols == VMS_DEBUG)\n");
                  int n = BLOCK_NUMBER (NOTE_BLOCK (insn));

                  mtcs_final_app_disable(self);
                  ++self->block_depth;
                  self->high_block_linenum = self->last_linenum;

                  /* Output debugging info about the symbol-block beginning.  */
                  if (!DECL_IGNORED_P (current_function_decl)){
                     n_debug("mtcsfinal.c final_scan_insn_1 100-2--if (!DECL_IGNORED_P (current_function_decl))\n");
                     mtcs_debug_begin_block/*!debug_hooks->begin_block*/(mtcsDebug,self->last_linenum, n,NOTE_BLOCK (insn));
                  }

                  /* Mark this block as output.  */
                  TREE_ASM_WRITTEN (NOTE_BLOCK (insn)) = 1;
                  BLOCK_IN_COLD_SECTION_P (NOTE_BLOCK (insn)) = in_cold_section_p;
               }
               break;

            case NOTE_INSN_BLOCK_END:
               n_debug("mtcsfinal.c final_scan_insn_1 101 NOTE_INSN_BLOCK_END\n");
               maybe_output_next_view (self,seen);

               if (debug_info_level >= DINFO_LEVEL_NORMAL
               || mtcs_opts_dwarf_debuginfo_p/*!dwarf_debuginfo_p*/(mtcsOpts,mtcsOptionsItem)
               || mtcsOptionsItem->x_write_symbols == VMS_DEBUG){
                  n_debug("mtcsfinal.c final_scan_insn_1 101-1--if (debug_info_level >= DINFO_LEVEL_NORMAL"
                        "|| dwarf_debuginfo_p () || write_symbols == VMS_DEBUG)\n");
                  int n = BLOCK_NUMBER (NOTE_BLOCK (insn));
                  mtcs_final_app_disable(self);
                  /* End of a symbol-block.  */
                  --self->block_depth;
                  gcc_assert (self->block_depth >= 0);

                  if (!DECL_IGNORED_P (current_function_decl)){
                     n_debug("mtcsfinal.c final_scan_insn_1 101-2--if (!DECL_IGNORED_P (current_function_decl))\n");
                     mtcs_debug_end_block/*!debug_hooks->end_block*/(mtcsDebug,self->high_block_linenum, n);
                  }
                  gcc_assert (BLOCK_IN_COLD_SECTION_P (NOTE_BLOCK (insn)) == in_cold_section_p);
               }
               break;

            case NOTE_INSN_DELETED_LABEL:
               n_debug("mtcsfinal.c final_scan_insn_1 102 NOTE_INSN_DELETED_LABEL\n");
               /* Emit the label.  We may have deleted the CODE_LABEL because
               the label could be proved to be unreachable, though still
               referenced (in the form of having its address taken.  */
               target_asm_out_output_debug_label/*ASM_OUTPUT_DEBUG_LABEL*/(mtcsMachine->asmOut,"L", CODE_LABEL_NUMBER (insn));
               break;

            case NOTE_INSN_DELETED_DEBUG_LABEL:
               n_debug("mtcsfinal.c final_scan_insn_1 103 NOTE_INSN_DELETED_DEBUG_LABEL\n");
               /* Similarly, but need to use different namespace for it.  */
               if (CODE_LABEL_NUMBER (insn) != -1)
                  target_asm_out_output_debug_label/*ASM_OUTPUT_DEBUG_LABEL*/(mtcsMachine->asmOut,"LDL", CODE_LABEL_NUMBER (insn));
               break;

            case NOTE_INSN_VAR_LOCATION:
               n_debug("mtcsfinal.c final_scan_insn_1 104 NOTE_INSN_VAR_LOCATION\n");

               if (!DECL_IGNORED_P (current_function_decl)){
                  n_debug("mtcsfinal.c final_scan_insn_1 104-1-- if (!DECL_IGNORED_P (current_function_decl))\n");
                  mtcs_debug_var_location/*!debug_hooks->var_location*/(mtcsDebug,insn);
                  set_next_view_needed (seen);
               }
               break;

            case NOTE_INSN_BEGIN_STMT:
               n_debug("mtcsfinal.c final_scan_insn_1 105 NOTE_INSN_BEGIN_STMT\n");
               gcc_checking_assert (cfun->debug_nonbind_markers);
               if (!DECL_IGNORED_P (current_function_decl) && notice_source_line (self,insn, NULL)){
                  n_debug("mtcsfinal.c final_scan_insn_1 105-1-- if (!DECL_IGNORED_P"
                        "(current_function_decl) && notice_source_line (insn, NULL))\n");
output_source_line:
                  mtcs_debug_source_line/*!(*debug_hooks->source_line)*/(mtcsDebug,
                        self->last_linenum, self->last_columnnum,self->last_filename, self->last_discriminator,true);
                  clear_next_view_needed (seen);
               }
               break;

            case NOTE_INSN_INLINE_ENTRY:
               n_debug("mtcsfinal.c final_scan_insn_1 106 NOTE_INSN_INLINE_ENTRY\n");
               gcc_checking_assert (cfun->debug_nonbind_markers);
               if (!DECL_IGNORED_P (current_function_decl) && notice_source_line (self,insn, NULL)){
                  n_debug("mtcsfinal.c final_scan_insn_1 106-1-- if (!DECL_IGNORED_P"
                        "(current_function_decl) && notice_source_line (insn, NULL))\n");
                  mtcs_debug_inline_entry/*!(*debug_hooks->inline_entry)*/(mtcsDebug,LOCATION_BLOCK(NOTE_MARKER_LOCATION (insn)));
                  goto output_source_line;
               }
               break;

            default:
               gcc_unreachable ();
               break;
         }
         break;

      case BARRIER:
         n_debug("mtcsfinal.c final_scan_insn_1 107 BARRIER\n");
         break;

      case CODE_LABEL:
         n_debug("mtcsfinal.c final_scan_insn_1 108 CODE_LABEL\n");

         /* The target port might emit labels in the output function for
         some insn, e.g. sh.cc output_branchy_insn.  */
         if (CODE_LABEL_NUMBER (insn) <= self->max_labelno){
            align_flags alignment = LABEL_TO_ALIGNMENT (insn);
            n_debug("mtcsfinal.c final_scan_insn_1 108-1--if (CODE_LABEL_NUMBER (insn) <= max_labelno) %d <=%d \n",
            CODE_LABEL_NUMBER (insn),self->max_labelno);
            if (alignment.levels[0].log && NEXT_INSN (insn)){
               n_debug("mtcsfinal.c final_scan_insn_1 108-2--  if (alignment.levels[0].log && NEXT_INSN (insn))\n");
               if(mtcsAsm->output_max_skip_align){
                  mtcsAsm->output_max_skip_align(mtcsAsm, alignment.levels[0].log, alignment.levels[0].maxskip);
                  mtcsAsm->output_max_skip_align(mtcsAsm, alignment.levels[1].log, alignment.levels[1].maxskip);
               }else{
                  if(mtcsAsm->output_align_with_nop)
                     mtcsAsm->output_align_with_nop(mtcsAsm, alignment.levels[0].log);
                  else{
                     if(mtcsAsm->output_align==NULL)
                        n_debug("mtcsfinal.c mtcsasm 必须实现 output_align");
                     mtcsAsm->output_align(mtcsAsm, alignment.levels[0].log);
                  }
               }
               /*!
               #ifdef ASM_OUTPUT_MAX_SKIP_ALIGN
               // Output both primary and secondary alignment.
               ASM_OUTPUT_MAX_SKIP_ALIGN (file, alignment.levels[0].log,
               alignment.levels[0].maxskip);
               ASM_OUTPUT_MAX_SKIP_ALIGN (file, alignment.levels[1].log,
               alignment.levels[1].maxskip);
               #else
               #ifdef ASM_OUTPUT_ALIGN_WITH_NOP
               ASM_OUTPUT_ALIGN_WITH_NOP (file, alignment.levels[0].log);
               #else
               ASM_OUTPUT_ALIGN (file, alignment.levels[0].log);
               #endif
               #endif*/
            }
         }
         CC_STATUS_INIT;
         if (!DECL_IGNORED_P (current_function_decl) && LABEL_NAME (insn)){
            n_debug("mtcsfinal.c final_scan_insn_1 108-6-- \n");
            mtcs_debug_label/*!debug_hooks->label*/(mtcsDebug,as_a <rtx_code_label *> (insn));
         }
         mtcs_final_app_disable(self);
         /* If this label is followed by a jump-table, make sure we put
         the label in the read-only section.  Also possibly write the
         label and jump table together.  */
         table = jump_table_for_label (as_a <rtx_code_label *> (insn));
         n_debug("mtcsfinal.c final_scan_insn_1 108-7-- table:%p\n",table);

         if (table){
            if(mtcsAsm->output_addr_vec/*!ASM_OUTPUT_ADDR_VEC*/ && mtcsAsm->output_addr_diff_vec/*!ASM_OUTPUT_ADDR_DIFF_VEC*/)
               n_debug("mtcsfinal.c final_scan_insn_1 108-8-- table:%p\n",table);
            else{
               if (! mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)){
                  n_debug("mtcsfinal.c final_scan_insn_1 108-9-- if (! JUMP_TABLES_IN_TEXT_SECTION)\n");
                  int log_align;
                  section *dsec=target_asm_out_function_rodata_section/*!targetm.asm_out.function_rodata_section*/
                        (mtcsMachine->asmOut,current_function_decl,jumptable_relocatable(self));
                  mtcs_asm_switch_to_section (mtcsAsm,dsec);
                  log_align = mtcs_align_get_addr_vec_align/*!ADDR_VEC_ALIGN*/(mtcsAlign,table);
                  mtcsAsm->output_align/*ASM_OUTPUT_ALIGN (file,*/(mtcsAsm, log_align);
               }else{
                  n_debug("mtcsfinal.c final_scan_insn_1 108-12--switch_to_section (current_function_section ());\n");
                  mtcs_asm_switch_to_section (mtcsAsm,mtcs_asm_current_function_section/*!current_function_section*/(mtcsAsm));
               }
               if(mtcsAsm->output_case_label){ //#ifdef ASM_OUTPUT_CASE_LABEL //host=1 nvptx=gcn=0
                  n_debug("mtcsfinal.c final_scan_insn_1 108-13-- #ifdef ASM_OUTPUT_CASE_LABEL\n");
                  mtcsAsm->output_case_label(mtcsAsm,"L", CODE_LABEL_NUMBER (insn), table);
               }else{
                  n_debug("mtcsfinal.c final_scan_insn_1 108-14--\n");
                  target_asm_out_internal_label/*!targetm.asm_out.internal_label*/(mtcsMachine->asmOut,"L", CODE_LABEL_NUMBER (insn));
               }
            }
            break;
         }
         if (LABEL_ALT_ENTRY_P (insn)){
            n_debug("mtcsfinal.c final_scan_insn_1 108-15--if (LABEL_ALT_ENTRY_P (insn))\n");
            output_alternate_entry_point (self, insn);
         }else{
            n_debug("mtcsfinal.c final_scan_insn_1 108-16--\n");
            target_asm_out_internal_label/*!targetm.asm_out.internal_label*/(mtcsMachine->asmOut, "L", CODE_LABEL_NUMBER (insn));
         }
         break;

      default:
      {
         n_debug("mtcsfinal.c final_scan_insn_1 109 default NAME:%s\n",GET_RTX_NAME(GET_CODE (insn)));
         rtx body = PATTERN (insn);
         int insn_code_number;
         const char *templ;
         bool is_stmt, *is_stmt_p;

         if (MAY_HAVE_DEBUG_MARKER_INSNS && cfun->debug_nonbind_markers){
            n_debug("mtcsfinal.c final_scan_insn_1 109-1-- if (MAY_HAVE_DEBUG_MARKER_INSNS && cfun->debug_nonbind_markers)\n");
            is_stmt = false;
            is_stmt_p = NULL;
         }else{
            n_debug("mtcsfinal.c final_scan_insn_1 109-2-- is_stmt_p\n");
            is_stmt_p = &is_stmt;
         }

         /* Reset this early so it is correct for ASM statements.  */
         current_insn_predicate = NULL_RTX;

         /* An INSN, JUMP_INSN or CALL_INSN.
         First check for special kinds that recog doesn't recognize.  */

         if (GET_CODE (body) == USE /* These are just declarations.  */ || GET_CODE (body) == CLOBBER){
            n_debug("mtcsfinal.c final_scan_insn_1 109-3--GET_CODE (body) == USE || GET_CODE (body) == CLOBBER\n");
            break;
         }

         /* Detect insns that are really jump-tables
         and output them as such.  */
         if (JUMP_TABLE_DATA_P (insn)){
            n_debug("mtcsfinal.c final_scan_insn_1 109-4--if (JUMP_TABLE_DATA_P (insn))\n");

            //                #if !(defined(ASM_OUTPUT_ADDR_VEC) || defined(ASM_OUTPUT_ADDR_DIFF_VEC))
            //                 n_debug("mtcsfinal.c final_scan_insn_1 109-5--#if !(defined(ASM_OUTPUT_ADDR_VEC) || defined(ASM_OUTPUT_ADDR_DIFF_VEC))\n");
            //                    int vlen, idx;
            //                #endif

            if (! mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)){
               n_debug("mtcsfinal.c final_scan_insn_1 109-6-- if (! JUMP_TABLES_IN_TEXT_SECTION)\n");
               section *dsec=target_asm_out_function_rodata_section/*!targetm.asm_out.function_rodata_section*/
                     (mtcsMachine->asmOut,current_function_decl,jumptable_relocatable (self));
               mtcs_asm_switch_to_section (mtcsAsm,dsec);
            }else{
               n_debug("mtcsfinal.c final_scan_insn_1 109-7-- if (JUMP_TABLES_IN_TEXT_SECTION)\n");
               mtcs_asm_switch_to_section (mtcsAsm,mtcs_asm_current_function_section/*!current_function_section*/(mtcsAsm));
            }


            mtcs_final_app_disable(self);
            int vlen, idx;
            if(!(mtcsAsm->output_addr_vec/*!ASM_OUTPUT_ADDR_VEC*/
            || mtcsAsm->output_addr_diff_vec/*!ASM_OUTPUT_ADDR_DIFF_VEC*/)){
               //  #if defined(ASM_OUTPUT_ADDR_VEC) || defined(ASM_OUTPUT_ADDR_DIFF_VEC)
               n_debug("mtcsfinal.c final_scan_insn_1 109-8-- #if defined(ASM_OUTPUT_ADDR_VEC) || defined(ASM_OUTPUT_ADDR_DIFF_VEC)\n");
               if (GET_CODE (body) == ADDR_VEC){
                  n_debug("mtcsfinal.c final_scan_insn_1 109-9--  if (GET_CODE (body) == ADDR_VEC)\n");
                  if(mtcsAsm->output_addr_vec/*! #ifdef ASM_OUTPUT_ADDR_VEC*/)
                     mtcsAsm->output_addr_vec/*!ASM_OUTPUT_ADDR_VEC*/(mtcsAsm,PREV_INSN (insn), body);
                  else // #else
                     gcc_unreachable ();
                  //#endif
               }else{
                  n_debug("mtcsfinal.c final_scan_insn_1 109-10--  if !(GET_CODE (body) == ADDR_VEC)\n");
                  if(mtcsAsm->output_addr_diff_vec/*! #ifdef ASM_OUTPUT_ADDR_DIFF_VEC*/)
                     mtcsAsm->output_addr_diff_vec/*!ASM_OUTPUT_ADDR_DIFF_VEC*/(mtcsAsm,PREV_INSN (insn), body);
                  else //#else
                     gcc_unreachable ();
                  //#endif
               }
            }else{//#else
               n_debug("mtcsfinal.c final_scan_insn_1 109-11-- vlen = XVECLEN (body, GET_CODE (body) == ADDR_DIFF_VEC);\n");
               vlen = XVECLEN (body, GET_CODE (body) == ADDR_DIFF_VEC);
               for (idx = 0; idx < vlen; idx++){
                  n_debug("mtcsfinal.c final_scan_insn_1 109-12-- vlen:%d idx:%d\n",vlen,idx);
                  if (GET_CODE (body) == ADDR_VEC){
                     n_debug("mtcsfinal.c final_scan_insn_1 109-13-- if (GET_CODE (body) == ADDR_VEC)\n");
                     if(mtcsAsm->output_addr_vec_elt/*!#ifdef ASM_OUTPUT_ADDR_VEC_ELT host=1 nvptx=0 gcn=1*/)
                        mtcsAsm->output_addr_vec_elt/*!ASM_OUTPUT_ADDR_VEC_ELT*/(mtcsAsm,CODE_LABEL_NUMBER (XEXP (XVECEXP (body, 0, idx), 0)));
                     else //#else
                        gcc_unreachable ();
                     //#endif
                  }else{
                     n_debug("mtcsfinal.c final_scan_insn_1 109-14-- if !(GET_CODE (body) == ADDR_VEC)\n");

                     if(mtcsAsm->output_addr_vec_diff_elt/*!#ifdef ASM_OUTPUT_ADDR_DIFF_ELT host=1 nvptx=0*/)
                        mtcsAsm->output_addr_vec_diff_elt/*!ASM_OUTPUT_ADDR_DIFF_ELT(file,*/(mtcsAsm,body,CODE_LABEL_NUMBER (XEXP (XVECEXP (body, 1, idx), 0)),CODE_LABEL_NUMBER (XEXP (XEXP (body, 0), 0)));
                     else// #else
                        gcc_unreachable ();
                     // #endif
                  }
               }
               if(mtcsAsm->output_case_end/*!#ifdef ASM_OUTPUT_CASE_END host=1 nvptx=0 gcn=1*/)
                  mtcsAsm->output_case_end/*!ASM_OUTPUT_CASE_END (file,*/(mtcsAsm,CODE_LABEL_NUMBER (PREV_INSN (insn)),insn);
                  //#endif
            }//#endif
            n_debug("mtcsfinal.c final_scan_insn_1 109-16-- switch_to_section (current_function_section ());\n");

            mtcs_asm_switch_to_section (mtcsAsm,mtcs_asm_current_function_section/*!current_function_section*/(mtcsAsm));

            if (mtcsOptionsItem->x_debug_variable_location_views && !DECL_IGNORED_P (current_function_decl)){
               n_debug("mtcsfinal.c final_scan_insn_1 109-17-- if (debug_variable_location_views && !DECL_IGNORED_P (current_function_decl))\n");
               mtcs_debug_var_location/*!debug_hooks->var_location*/(mtcsDebug,insn);
            }

            break;
         }
         /* Output this line note if it is the first or the last line
         note in a row.  */
         if (!DECL_IGNORED_P (current_function_decl)  && notice_source_line (self,insn, is_stmt_p)){
            n_debug("mtcsfinal.c final_scan_insn_1 109-18--  if (!DECL_IGNORED_P (current_function_decl)"
                  "&& notice_source_line (insn, is_stmt_p))\n");

            if (mtcsOptionsItem->x_flag_verbose_asm){
               n_debug("mtcsfinal.c final_scan_insn_1 109-19-- flag_verbose_asm=true\n");
               asm_show_source (self,self->last_filename, self->last_linenum);
            }
            mtcs_debug_source_line/*!(*debug_hooks->source_line)*/(mtcsDebug,
                  self->last_linenum, self->last_columnnum ,self->last_filename, self->last_discriminator,is_stmt);
            clear_next_view_needed (seen);
         }else{
            n_debug("mtcsfinal.c final_scan_insn_1 109-20-- DECL_IGNORED_P:%d notice_source_line:%d is_stmt_p:%d\n",
            DECL_IGNORED_P (current_function_decl),notice_source_line (self,insn, is_stmt_p),*is_stmt_p);
            maybe_output_next_view (self,seen);
         }

         gcc_checking_assert (!DEBUG_INSN_P (insn));

         if (GET_CODE (body) == PARALLEL && GET_CODE (XVECEXP (body, 0, 0)) == ASM_INPUT){
            n_debug("mtcsfinal.c final_scan_insn_1 109-21--  if (GET_CODE (body) == PARALLEL "
                  "&& GET_CODE (XVECEXP (body, 0, 0)) == ASM_INPUT)\n");
            body = XVECEXP (body, 0, 0);
         }

         if (GET_CODE (body) == ASM_INPUT){
            n_debug("mtcsfinal.c final_scan_insn_1 109-22--  if (GET_CODE (body) == ASM_INPUT)\n");
            const char *string = XSTR (body, 0);
            /* There's no telling what that did to the condition codes.  */
            CC_STATUS_INIT;

            if (string[0]){
               n_debug("mtcsfinal.c final_scan_insn_1 109-23--   if (string[0])\n");

               expanded_location loc;
               app_enable ();
               loc = expand_location (ASM_INPUT_SOURCE_LOCATION (body));
               if (*loc.file && loc.line)
                  fprintf (mtcsAsm->asmFile, "%s %i \"%s\" 1\n",asmCommentStart/*!ASM_COMMENT_START*/, loc.line, loc.file);
               fprintf (mtcsAsm->asmFile, "\t%s\n", string);
               if(mtcs_config_if/*!#if HAVE_AS_LINE_ZERO*/(mtcsConfig,MTCS_HAVE_AS_LINE_ZERO)){
                  n_debug("mtcsfinal.c final_scan_insn_1 109-24--   #if HAVE_AS_LINE_ZERO\n");
                  if (*loc.file && loc.line)
                     fprintf (mtcsAsm->asmFile, "%s 0 \"\" 2\n", asmCommentStart/*!ASM_COMMENT_START*/);
               }//#endif
            }
            break;
         }

         /* Detect `asm' construct with operands.  */
         if (asm_noperands (body) >= 0){
            n_debug("mtcsfinal.c final_scan_insn_1 109-25--  if (asm_noperands (body) >= 0)\n");

            unsigned int noperands = asm_noperands (body);
            rtx *ops = XALLOCAVEC (rtx, noperands);
            const char *string;
            location_t loc;
            expanded_location expanded;

            /* There's no telling what that did to the condition codes.  */
            CC_STATUS_INIT;

            /* Get out the operand values.  */
            string = decode_asm_operands (body, ops, NULL, NULL, NULL, &loc);
            /* Inhibit dying on what would otherwise be compiler bugs.  */
            mtcsOutput->insn_noperands = noperands;
            mtcsOutput->this_is_asm_operands = insn;
            expanded = expand_location (loc);

            #ifdef FINAL_PRESCAN_INSN //host=0 nvptx=0
               n_debug("mtcsfinal.c final_scan_insn_1 109-26--   #ifdef FINAL_PRESCAN_INSN\n");
               FINAL_PRESCAN_INSN (insn, ops, mtcsOutput->insn_noperands);
            #endif

            /* Output the insn using them.  */
            if (string[0]){
               n_debug("mtcsfinal.c final_scan_insn_1 109-27--   if (string[0])\n");

               mtcs_final_app_enable(self);
               if (expanded.file && expanded.line)
                  fprintf (mtcsAsm->asmFile, "%s %i \"%s\" 1\n", asmCommentStart/*!ASM_COMMENT_START*/,expanded.line, expanded.file);
               mtcs_output_asm_insn/*!output_asm_insn*/(mtcsOutput,string, ops);
               if(mtcs_config_if/*!#if HAVE_AS_LINE_ZERO*/(mtcsConfig,MTCS_HAVE_AS_LINE_ZERO)){
                  // #if HAVE_AS_LINE_ZERO
                  n_debug("mtcsfinal.c final_scan_insn_1 109-28-- #if HAVE_AS_LINE_ZERO\n");

                  if (expanded.file && expanded.line)
                     fprintf (mtcsAsm->asmFile, "%s 0 \"\" 2\n", asmCommentStart/*!ASM_COMMENT_START*/);
                  // #endif
               }
            }

            if (mtcsMachine->asmOut->final_postscan_insn/*!targetm.asm_out.final_postscan_insn*/){
               n_debug("mtcsfinal.c final_scan_insn_1 109-29-- if (targetm.asm_out.final_postscan_insn){\n");
               target_asm_out_final_postscan_insn/*!targetm.asm_out.final_postscan_insn*/(mtcsMachine->asmOut,
                     insn, ops,mtcsOutput->insn_noperands);
            }

            mtcsOutput->this_is_asm_operands = 0;
            break;
         }

         mtcs_final_app_disable(self);
         n_debug("mtcsfinal.c final_scan_insn_1 109-30--\n");

         if (rtx_sequence *seq = dyn_cast <rtx_sequence *> (body)){
            n_debug("mtcsfinal.c final_scan_insn_1 109-31--if (rtx_sequence *seq = dyn_cast <rtx_sequence *> (body))\n");
            /* A delayed-branch sequence */
            int i;
            self->final_sequence = seq;
            /* The first insn in this SEQUENCE might be a JUMP_INSN that will
            force the restoration of a comparison that was previously
            thought unnecessary.  If that happens, cancel this sequence
            and cause that insn to be restored.  */

            next = mtcs_final_final_scan_insn (self,seq->insn (0), 0, 1, seen);
            if (next != seq->insn (1)){
               n_debug("mtcsfinal.c final_scan_insn_1 109-32-- if (next != seq->insn (1))\n");
               self->final_sequence = 0;
               return next;
            }

            for (i = 1; i < seq->len (); i++){
               n_debug("mtcsfinal.c final_scan_insn_1 109-33-- for (i = 1; i < seq->len (); i++)\n");

               rtx_insn *insn = seq->insn (i);
               rtx_insn *next = NEXT_INSN (insn);
               /* We loop in case any instruction in a delay slot gets
               split.  */
               do
                  insn = mtcs_final_final_scan_insn (self,insn,  0, 1, seen);
               while (insn != next);
            }
            #ifdef DBR_OUTPUT_SEQEND //host=0 nvptx=0
               n_debug("mtcsfinal.c final_scan_insn_1 109-34-- #ifdef DBR_OUTPUT_SEQEND\n");
               DBR_OUTPUT_SEQEND (file);
            #endif
            self->final_sequence = 0;

            /* If the insn requiring the delay slot was a CALL_INSN, the
            insns in the delay slot are actually executed before the
            called function.  Hence we don't preserve any CC-setting
            actions in these insns and the CC must be marked as being
            clobbered by the function.  */
            if (CALL_P (seq->insn (0))){
               n_debug("mtcsfinal.c final_scan_insn_1 109-35--if (CALL_P (seq->insn (0)))\n");
               CC_STATUS_INIT;
            }
            break;
         }

         /* We have a real machine instruction as rtl.  */
         body = PATTERN (insn);
         n_debug("mtcsfinal.c final_scan_insn_1 109-36--\n");

         /* Do machine-specific peephole optimizations if desired.  */
         if (mtcs_config_ifdefine(mtcsConfig,MTCS_HAVE_peephole)
         && optimize_p && !mtcsOptionsItem->x_flag_no_peephole && !nopeepholes){
            n_debug("mtcsfinal.c final_scan_insn_1 109-37-- if (HAVE_peephole && optimize_p && !flag_no_peephole && !nopeepholes)\n");

            rtx_insn *next = peephole (insn);
            /* When peepholing, if there were notes within the peephole,
            emit them before the peephole.  */
            if (next != 0 && next != NEXT_INSN (insn)){
               n_debug("mtcsfinal.c final_scan_insn_1 109-38--  if (next != 0 && next != NEXT_INSN (insn))\n");

               rtx_insn *note, *prev = PREV_INSN (insn);
               for (note = NEXT_INSN (insn); note != next; note = NEXT_INSN (note))
                  mtcs_final_final_scan_insn (self,note,  optimize_p, nopeepholes, seen);

               /* Put the notes in the proper position for a later
               rescan.  For example, the SH target can do this
               when generating a far jump in a delayed branch
               sequence.  */
               note = NEXT_INSN (insn);
               SET_PREV_INSN (note) = prev;
               SET_NEXT_INSN (prev) = note;
               SET_NEXT_INSN (PREV_INSN (next)) = insn;
               SET_PREV_INSN (insn) = PREV_INSN (next);
               SET_NEXT_INSN (insn) = next;
               SET_PREV_INSN (next) = insn;
            }

            /* PEEPHOLE might have changed this.  */
            body = PATTERN (insn);
         }

         /* Try to recognize the instruction.
         If successful, verify that the operands satisfy the
         constraints for the instruction.  Crash if they don't,
         since `reload' should have changed them so that they do.  */
         n_debug("mtcsfinal.c final_scan_insn_1 109-39-- recog_memoized\n");

         insn_code_number = mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn);
         mtcs_output_cleanup_subreg_operands/*!cleanup_subreg_operands*/(mtcsOutput,insn);

         /* Dump the insn in the assembly for debugging (-dAP).
         If the final dump is requested as slim RTL, dump slim
         RTL to the assembly file also.  */
         if (mtcsOptionsItem->x_flag_dump_rtl_in_asm){
            n_debug("mtcsfinal.c final_scan_insn_1 109-40-- \n");

            print_rtx_head = asmCommentStart/*!ASM_COMMENT_START*/;
            if (! (dump_flags & TDF_SLIM)){
               n_debug("mtcsfinal.c final_scan_insn_1 109-41--  if (! (dump_flags & TDF_SLIM))\n");
               print_rtl_single (asm_out_file, insn);
            }else{
               n_debug("mtcsfinal.c final_scan_insn_1 109-42--  if !(! (dump_flags & TDF_SLIM))\n");
               dump_insn_slim (asm_out_file, insn);
            }
            print_rtx_head = "";
         }

         if (! mtcs_recog_constrain_operands_cached/*!constrain_operands_cached*/(mtcsRecog,insn, 1)){
            n_debug("mtcsfinal.c final_scan_insn_1 109-43--   if (! constrain_operands_cached (insn, 1))\n");
            fatal_insn_not_found (insn);
         }

         /* Some target machines need to prescan each insn before
         it is output.  */

         #ifdef FINAL_PRESCAN_INSN //host=0 nvptx=0
            n_debug("mtcsfinal.c final_scan_insn_1 109-44-- #ifdef FINAL_PRESCAN_INSN\n");
            FINAL_PRESCAN_INSN (insn,  mtcsRecog->recog_data.operand,  mtcsRecog->recog_data.n_operands);
         #endif

         if (mtcsTarget->have_conditional_execution/*!targetm.have_conditional_execution*/ (mtcsTarget)
               && GET_CODE (PATTERN (insn)) == COND_EXEC){
            n_debug("mtcsfinal.c final_scan_insn_1 109-45-- if (targetm.have_conditional_execution ()"
                  "&& GET_CODE (PATTERN (insn)) == COND_EXEC)\n");
            current_insn_predicate = COND_EXEC_TEST (PATTERN (insn));
         }

         current_output_insn = mtcsOutput->debug_insn = insn;
         n_debug("mtcsfinal.c final_scan_insn_1 109-45xx-- 获取模板 insn_code_number:%d insn:%p \n",insn_code_number,insn);

         /* Find the proper template for this insn.  */
         templ = mtcs_output_get_insn_template/*!get_insn_template*/(mtcsOutput,insn_code_number, insn);
         n_debug("mtcsfinal.c final_scan_insn_1 109-45yy-- templ:%s\n",templ);

         /* If the C code returns 0, it means that it is a jump insn
         which follows a deleted test insn, and that test insn
         needs to be reinserted.  */
         if (templ == 0){
            n_debug("mtcsfinal.c final_scan_insn_1 109-46-- if (templ == 0)\n");

            rtx_insn *prev;
            gcc_assert (prev_nonnote_insn (insn) == self->last_ignored_compare);
            /* We have already processed the notes between the setter and
            the user.  Make sure we don't process them again, this is
            particularly important if one of the notes is a block
            scope note or an EH note.  */
            for (prev = insn; prev != self->last_ignored_compare; prev = PREV_INSN (prev)){
               n_debug("mtcsfinal.c final_scan_insn_1 109-47-- for (prev = insn;"
                     "prev != last_ignored_compare; prev = PREV_INSN (prev))\n");
               if (NOTE_P (prev))
                  mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,prev);   /* Use delete_note.  */
            }
            return prev;
         }

         /* If the template is the string "#", it means that this insn must
         be split.  */
         if (templ[0] == '#' && templ[1] == '\0'){
            n_debug("mtcsfinal.c final_scan_insn_1 109-48-- if (templ[0] == '#' && templ[1] == '\0')\n");

            rtx_insn *new_rtx = mtcs_emit_try_split/*!try_split*/(mtcsEmit,body, insn, 0);
            /* If we didn't split the insn, go away.  */
            if (new_rtx == insn && PATTERN (new_rtx) == body){
               n_debug("mtcsfinal.c final_scan_insn_1 109-49-- if (new_rtx == insn && PATTERN (new_rtx) == body){\n");
               fatal_insn ("could not split insn", insn);
            }
            /* If we have a length attribute, this instruction should have
            been split in shorten_branches, to ensure that we would have
            valid length info for the splitees.  */
            gcc_assert (!mtcs_insn_attr_get_have_attr_length/*!HAVE_ATTR_length*/(mtcsInsnAttr));
            return new_rtx;
         }

         /* ??? This will put the directives in the wrong place if
         get_insn_template outputs assembly directly.  However calling it
         before get_insn_template breaks if the insns is split.  */
         if (mtcsTarget->unwind_emit_before_insn  && mtcsMachine->asmOut->unwind_emit/*!targetm.asm_out.unwind_emit*/){
            n_debug("mtcsfinal.c final_scan_insn_1 109-50-- if (targetm.asm_out.unwind_emit_before_insn"
                  "&& targetm.asm_out.unwind_emit)\n");
            target_asm_out_unwind_emit/*!targetm.asm_out.unwind_emit*/(mtcsMachine->asmOut,insn);
         }

         rtx_call_insn *call_insn = dyn_cast <rtx_call_insn *> (insn);
         if (call_insn != NULL){
            n_debug("mtcsfinal.c final_scan_insn_1 109-51-- if (call_insn != NULL)\n");

            rtx x = call_from_call_insn (call_insn);
            x = XEXP (x, 0);
            if (x && MEM_P (x) && GET_CODE (XEXP (x, 0)) == SYMBOL_REF){
               n_debug("mtcsfinal.c final_scan_insn_1 109-52-- if (x && MEM_P (x) && GET_CODE (XEXP (x, 0)) == SYMBOL_REF)\n");
               tree t;
               x = XEXP (x, 0);
               t = SYMBOL_REF_DECL (x);
               if (t){
                  n_debug("mtcsfinal.c final_scan_insn_1 109-53-- if (t)\n");
                  mtcs_asm_assemble_external (mtcsAsm,t);
               }
            }
         }
         n_debug("mtcsfinal.c final_scan_insn_1 109-54-- output_asm_insn (templ, recog_data.operand); tmpl:%s insn_numb:%d\n",
               templ,insn_code_number);

         /* Output assembler code from the template.  */
         mtcs_output_asm_insn/*!output_asm_insn*/(mtcsOutput,templ, mtcsRecog->recog_data.operand);
         /* Some target machines need to postscan each insn after
         it is output.  */
         if (mtcsMachine->asmOut->final_postscan_insn/*!targetm.asm_out.final_postscan_insn*/){
            n_debug("mtcsfinal.c final_scan_insn_1 109-55-- if (targetm.asm_out.final_postscan_insn)\n");
            target_asm_out_final_postscan_insn/*!targetm.asm_out.final_postscan_insn*/(mtcsMachine->asmOut,
                  insn, mtcsRecog->recog_data.operand,mtcsRecog->recog_data.n_operands);
         }

         if (!mtcsTarget->unwind_emit_before_insn && mtcsMachine->asmOut->unwind_emit/*!targetm.asm_out.unwind_emit*/){
            n_debug("mtcsfinal.c final_scan_insn_1 109-56-- if (!targetm.asm_out.unwind_emit_before_insn "
                  "&& targetm.asm_out.unwind_emit)\n");
            target_asm_out_unwind_emit/*!targetm.asm_out.unwind_emit*/(mtcsMachine->asmOut, insn);
         }

         /* Let the debug info back-end know about this call.  We do this only
         after the instruction has been emitted because labels that may be
         created to reference the call instruction must appear after it.  */
         if ((mtcsOptionsItem->x_debug_variable_location_views || call_insn != NULL)
         && !DECL_IGNORED_P (current_function_decl)){
            n_debug("mtcsfinal.c final_scan_insn_1 109-57-- if ((debug_variable_location_views"
                  "|| call_insn != NULL)  && !DECL_IGNORED_P (current_function_decl))\n");
            mtcs_debug_var_location/*!debug_hooks->var_location*/(mtcsDebug,insn);
         }

         current_output_insn = mtcsOutput->debug_insn = 0;
      }//end default
   }
   return NEXT_INSN (insn);
}

static void  final_start_function_1(MtcsFinal *self,rtx_insn **firstp, int *seen, int optimize_p ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
  MtcsDwarf2Out *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  self->block_depth = 0;

  mtcsOutput->this_is_asm_operands = 0;

  self->need_profile_function = false;

  self->last_filename = LOCATION_FILE (prologue_location);
  self->last_linenum = LOCATION_LINE (prologue_location);
  self->last_columnnum = LOCATION_COLUMN (prologue_location);
  self->last_discriminator = 0;
  self->force_source_line = false;
  n_debug("mtcsfinal.c final_start_function_1 00 last_filename:%s\n",self->last_filename);

  self->high_block_linenum = self->high_function_linenum = self->last_linenum;

  rtx_insn *first = *firstp;
  if (in_initial_view_p (first)){
      n_debug("mtcsfinal.c final_start_function_1 11 in_initial_view_p (first)=true\n");
      do{
          n_debug("mtcsfinal.c final_start_function_1 22 do while\n");
          mtcs_final_final_scan_insn (self,first, 0, 0, seen);
          first = NEXT_INSN (first);
      } while (in_initial_view_p (first));
      *firstp = first;
  }

  if (!DECL_IGNORED_P (current_function_decl)){
      n_debug("mtcsfinal.c final_start_function_1 33 if (!DECL_IGNORED_P (current_function_decl))\n");
      mtcs_debug_begin_prologue/*!debug_hooks->begin_prologue*/(mtcsDebug,self->last_linenum, self->last_columnnum,self->last_filename);
  }

  if (!dwarf2_debug_info_emitted_p(self,current_function_decl)){
      n_debug("mtcsfinal.c final_start_function_1 44  if (!dwarf2_debug_info_emitted_p (current_function_decl))\n");
      mtcs_dwarf2_out_dwarf2out_begin_prologue (mtcsDwarf2Out,0, 0, NULL);
  }

  if (DECL_IGNORED_P (current_function_decl) && self->last_linenum && self->last_filename){
      n_debug("mtcsfinal.c final_start_function_1 55 if (DECL_IGNORED_P (current_function_decl) && last_linenum && last_filename)\n");

    mtcs_debug_set_ignored_loc/*!debug_hooks->set_ignored_loc*/(mtcsDebug,self->last_linenum, self->last_columnnum, self->last_filename);
  }

#ifdef LEAF_REG_REMAP
  n_debug("mtcsfinal.c final_start_function_1 66 #ifdef LEAF_REG_REMAP\n");

  if (mtcsRtlData/*!crtl*/->uses_only_leaf_regs){
      n_debug("mtcsfinal.c final_start_function_1 77 #ifdef LEAF_REG_REMAP  if (crtl->uses_only_leaf_regs)\n");

    leaf_renumber_regs (first);
  }
#endif

  /* The Sun386i and perhaps other machines don't work right
     if the profiling code comes after the prologue.  */
  if (mtcsTarget->profile_before_prologue (mtcsTarget) && mtcsRtlData/*!crtl*/->profile){
      n_debug("mtcsfinal.c final_start_function_1 88  if (targetm.profile_before_prologue () && crtl->profile)\n");

      if (mtcsMachine->asmOut->function_prologue==target_asm_out_default_function_pro_epilogue
            /*!targetm.asm_out.function_prologue == default_function_pro_epilogue*/
            && target_rtx_have_prologue /*!targetm.have_prologue*/(mtcsMachine->tmrtx)) {
          n_debug("mtcsfinal.c final_start_function_1 99  if (targetm.asm_out.function_prologue == default_function_pro_epilogue && targetm.have_prologue ())\n");

          rtx_insn *insn;
          for (insn = first; insn; insn = NEXT_INSN (insn)){
              if (!NOTE_P (insn)){
                  n_debug("mtcsfinal.c final_start_function_1 100  if (!NOTE_P (insn))\n");
                insn = NULL;
                break;
              }else if (NOTE_KIND (insn) == NOTE_INSN_BASIC_BLOCK || NOTE_KIND (insn) == NOTE_INSN_FUNCTION_BEG){
                  n_debug("mtcsfinal.c final_start_function_1 101  else if (NOTE_KIND (insn) == NOTE_INSN_BASIC_BLOCK || NOTE_KIND (insn) == NOTE_INSN_FUNCTION_BEG)\n");

                break;
              }else if (NOTE_KIND (insn) == NOTE_INSN_DELETED || NOTE_KIND (insn) == NOTE_INSN_VAR_LOCATION){
                  n_debug("mtcsfinal.c final_start_function_1 102  else if (NOTE_KIND (insn) == NOTE_INSN_DELETED|| NOTE_KIND (insn) == NOTE_INSN_VAR_LOCATION)\n");

                continue;
              }else{
                  n_debug("mtcsfinal.c final_start_function_1 103 insn=NULL\n");

                insn = NULL;
                break;
              }
          }

          if (insn){
              n_debug("mtcsfinal.c final_start_function_1 104 insn!=NULL need_profile_function=true\n");
              self->need_profile_function = true;
          }else{
              n_debug("mtcsfinal.c final_start_function_1 105  profile_function (file);\n");
              profile_function (self);
          }
     }else{
         n_debug("mtcsfinal.c final_start_function_1 106 else  profile_function (file);\n");
        profile_function (self);
     }
  }

  /* If debugging, assign block numbers to all of the blocks in this
     function.  */
  if (write_symbols){
      n_debug("mtcsfinal.c final_start_function_1 107 write_symbolsn\n");
      reemit_insn_block_notes(self);
      number_blocks (current_function_decl);
      /* We never actually put out begin/end notes for the top-level
     block in the function.  But, conceptually, that block is
     always needed.  */
      TREE_ASM_WRITTEN (DECL_INITIAL (current_function_decl)) = 1;
  }

  unsigned HOST_WIDE_INT min_frame_size = constant_lower_bound (mtcs_func_get_frame_size/*!get_frame_size*/(mtcsFunc));
  if (min_frame_size > (unsigned HOST_WIDE_INT) mtcsOptionsItem->x_warn_frame_larger_than_size){
      /* Issue a warning */
      warning (OPT_Wframe_larger_than_,"the frame size of %wu bytes is larger than %wu bytes",
            min_frame_size, mtcsOptionsItem->x_warn_frame_larger_than_size);
  }
  n_debug("mtcsfinal.c final_start_function_1 108  targetm.asm_out.function_prologue (file);\n");

  /* First output the function prologue: code to set up the stack frame.  */
  target_asm_out_function_prologue/*targetm.asm_out.function_prologue*/(mtcsMachine->asmOut);

  /* If the machine represents the prologue as RTL, the profiling code must
     be emitted when NOTE_INSN_PROLOGUE_END is scanned.  */
  if (! target_rtx_have_prologue /*!targetm.have_prologue*/(mtcsMachine->tmrtx)){
      n_debug("mtcsfinal.c final_start_function_1 109  if (! targetm.have_prologue ())\n");
    profile_after_prologue (self);
  }
}

/* Create a symbol with label LABEL and place it at byte offset
   OFFSET in BLOCK.  OFFSET can be negative if the symbol's offset
   is not yet known.  LABEL must be a garbage-collected string.  */
static rtx create_block_symbol (MtcsFinal *self,const char *label, struct object_block *block, HOST_WIDE_INT offset)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx symbol;
  unsigned int size;

  /* Create the extended SYMBOL_REF.  */
  size = RTX_HDR_SIZE + sizeof (struct block_symbol);
  symbol = (rtx) ggc_internal_alloc (size);

  /* Initialize the normal SYMBOL_REF fields.  */
  memset (symbol, 0, size);
  PUT_CODE (symbol, SYMBOL_REF);
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,symbol, mtcs_mode_get_Pmode(mtcsMode));
  XSTR (symbol, 0) = label;
  SYMBOL_REF_FLAGS (symbol) = SYMBOL_FLAG_HAS_BLOCK_INFO;

  /* Initialize the block_symbol stuff.  */
  SYMBOL_REF_BLOCK (symbol) = block;
  SYMBOL_REF_BLOCK_OFFSET (symbol) = offset;
  return symbol;
}

rtx_insn *mtcs_final_final_scan_insn (MtcsFinal *self,rtx_insn *insn, int optimize_p,int nopeepholes, int *seen)
{
  static int *enclosing_seen;
  static int recursion_counter;

  gcc_assert (seen || recursion_counter);
  gcc_assert (!recursion_counter || !seen || seen == enclosing_seen);

  n_debug("mtcsfinal.c mtcs_final_final_scan_insn 00 optimize_p:%d nopeepholes:%d seen:%d recursion_counter:%d\n",
          optimize_p,nopeepholes,*seen,recursion_counter);
  mtcs_print_rtl(stderr,insn);

  if (!recursion_counter++){
    enclosing_seen = seen;
    n_debug("mtcsfinal.c mtcs_final_final_scan_insn 11 recursion_counter:%d\n",recursion_counter);
  }else if (!seen){
      n_debug("mtcsfinal.c mtcs_final_final_scan_insn 22 !seen: enclosing_seen:%d\n",*enclosing_seen);
      seen = enclosing_seen;
  }
  n_debug("mtcsfinal.c mtcs_final_final_scan_insn 33 开始 scan_insn_1 recursion_counter:%d\n",recursion_counter);
  mtcs_print_rtl_single(stderr,insn);
  rtx_insn *ret = final_scan_insn_1 (self,insn,  optimize_p, nopeepholes, seen);
  n_debug("mtcsfinal.c mtcs_final_final_scan_insn 44 结束 scan_insn_1 ret:%p\n",ret);

  if (!--recursion_counter)
    enclosing_seen = NULL;

  return ret;
}

/* Collect hard register usage for the current function.  */

static void collect_fn_hard_reg_usage (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsCgraph *mtcsCgraph=mtcs_target_get_cgraph(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

   rtx_insn *insn;
   //#ifdef STACK_REGS host=1 nvptx=0
   //  int i;
   //#endif
   MtcsCgraphRtlInfo *node;/*!struct cgraph_rtl_info *node;*/
   HardRegSet/*!HARD_REG_SET*/ function_used_regs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};

   /* ??? To be removed when all the ports have been fixed.  */
   if (!mtcsTarget->call_fusage_contains_non_callee_clobbers)
      return;

   /* Be conservative - mark fixed and global registers as used.  */
   function_used_regs =mtcsReg->hardRegs.x_fixed_reg_set;/*!fixed_reg_set*/

   //#ifdef STACK_REGS host=1 nvptx=0
   //  /* Handle STACK_REGS conservatively, since the df-framework does not
   //     provide accurate information for them.  */
   //
   //  for (i = FIRST_STACK_REG; i <= LAST_STACK_REG; i++)
   //    SET_HARD_REG_BIT (function_used_regs, i);
   //#endif

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn != NULL_RTX; insn = next_insn (insn)){
      HardRegSet insn_used_regs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
      if (!NONDEBUG_INSN_P (insn))
         continue;

      if (CALL_P (insn) && !self_recursive_call_p (insn))
         function_used_regs|= mtcs_func_abi_insn_callee_abi/*!insn_callee_abi*/(mtcsFuncAbi,insn).full_and_partial_reg_clobbers ();

      mtcs_rtlanal_find_all_hard_reg_sets/*!find_all_hard_reg_sets*/(mtcsRtlanal,insn, &insn_used_regs, false);
      function_used_regs |= insn_used_regs;

      if (mtcs_reg_hard_reg_set_subset_p (&mtcsRtlData/*!crtl*/->abi->full_and_partial_reg_clobbers(),&function_used_regs))
         return;
   }

   /* Mask out fully-saved registers, so that they don't affect equality
   comparisons between function_abis.  */
   function_used_regs &= mtcsRtlData/*!crtl*/->abi->full_and_partial_reg_clobbers ();

   node = mtcs_cgraph_get_rtl_info/*!cgraph_node::rtl_info (current_function_decl);*/(mtcsCgraph,current_function_decl);
   gcc_assert (node != NULL);
   node->function_used_regs = function_used_regs;
}

void mtcs_final_final_end_function (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsDwarf2Cfi *mtcsDwarf2Cfi=mtcs_target_get_dwarf2_cfi(mtcsTarget);
   MtcsDwarf2Out *mtcsDwarf2Out=mtcs_target_get_dwarf2_out(mtcsTarget);
   MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   mtcs_final_app_disable (self);

   if (!DECL_IGNORED_P (current_function_decl))
      mtcs_debug_end_function/*!debug_hooks->end_function*/(mtcsDebug,self->high_function_linenum);

   /* Finally, output the function epilogue:
   code to restore the stack frame and return to the caller.  */
   target_asm_out_function_epilogue/*!targetm.asm_out.function_epilogue*/(mtcsMachine->asmOut);

   /* And debug output.  */
   if (!DECL_IGNORED_P (current_function_decl))
      mtcs_debug_end_epilogue/*!debug_hooks->end_epilogue*/(mtcsDebug,self->last_linenum, self->last_filename);

   if (!dwarf2_debug_info_emitted_p(self,current_function_decl)  && mtcs_dwarf2_cfi_dwarf2out_do_frame (mtcsDwarf2Cfi))
      mtcs_dwarf2_out_dwarf2out_end_epilogue/*!dwarf2out_end_epilogue*/(mtcsDwarf2Out,self->last_linenum, self->last_filename);

   self->some_local_dynamic_name = 0;
}

/* Initialize data in final at the beginning of a compilation.  */
//原型 init_final output.h final.cc
void mtcs_final_init_final (MtcsFinal *self,const char *mainName)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);

   self->app_on=FALSE;
   self->final_sequence=NULL;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_ASSEMBLER_DIALECT))
      self->dialect_number =mtcs_config_get_value/*!ASSEMBLER_DIALECT*/(mtcsConfig,MTCS_ASSEMBLER_DIALECT);
   /*
   #ifdef ASSEMBLER_DIALECT //host=1 nvptx=0
   dialect_number = ASSEMBLER_DIALECT;
   #endif
   */
}

static unsigned int rest_of_handle_final (MtcsFinal *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);
    MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

    const char *fnname = mtcs_asm_get_fnname_from_decl (mtcsAsm,current_function_decl);

    /* Turn debug markers into notes if the var-tracking pass has not
       been invoked.  */
    if (!flag_var_tracking && MAY_HAVE_DEBUG_MARKER_INSNS){
      delete_vta_debug_insns (false);
      n_error("不未支持 delete_vta_debug_insn\n");
      return;
    }

    n_debug("mtcsfinal.c rest_of_handle_final 00 assemble_start_function fnname:%s\n",fnname);
    mtcs_asm_print(mtcsAsm);

    mtcs_asm_start_function(mtcsAsm,current_function_decl,fnname);
    n_debug("mtcsfinal.c rest_of_handle_final 11 final_start_function_1 fnname:%s\n",fnname);

    rtx_insn *first = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
    n_debug("mtcsfinal.c rest_of_handle_final 22 进入函数体写入汇编 fun:%s first insn :%p optimize:%d\n",
          fnname,first,mtcsOptionsItem->x_optimize);
    mtcs_asm_print(mtcsAsm);

    int seen = 0;
    final_start_function_1(self,&first, &seen, optimize);//optimize来自options.h
    n_debug("mtcsfinal.c rest_of_handle_final 33 final_1\n");
    mtcs_asm_print(mtcsAsm);

    final_1 (self,first, seen, mtcsOptionsItem->x_optimize);
    n_debug("mtcsfinal.c rest_of_handle_final 33--- final_1\n");
    mtcs_asm_print(mtcsAsm);
    if (mtcsOptionsItem->x_flag_ipa_ra  && !lookup_attribute ("noipa", DECL_ATTRIBUTES (current_function_decl))
        /* Functions with naked attributes are supported only with basic asm
       statements in the body, thus for supported use cases the information
       on clobbered registers is not available.  */
        && !lookup_attribute ("naked", DECL_ATTRIBUTES (current_function_decl))){
        n_debug("mtcsfinal.c rest_of_handle_final 44 collect_fn_hard_reg_usage\n");
        collect_fn_hard_reg_usage (self);
    }
    n_debug("mtcsfinal.c rest_of_handle_final 55 mtcs_final_final_end_function\n");
    mtcs_final_final_end_function (self);

    /* The IA-64 ".handlerdata" directive must be issued before the ".endp"
       directive that closes the procedure descriptor.  Similarly, for x64 SEH.
       Otherwise it's not strictly necessary, but it doesn't hurt either.  */
    n_debug("mtcsfinal.c rest_of_handle_final 66 mtcs_except_output_function_exception_table\n");
    mtcs_asm_print(mtcsAsm);

    mtcs_except_output_function_exception_table (mtcsExcept,mtcsRtlData/*!crtl*/->has_bb_partition ? 1 : 0);
    n_debug("mtcsfinal.c rest_of_handle_final 77 mtcs_asm_assemble_end_function\n");
    mtcs_asm_assemble_end_function (mtcsAsm,current_function_decl, fnname);
    n_debug("mtcsfinal.c rest_of_handle_final 88 结束函数体写入汇编 fun:%s\n",fnname);
    mtcs_asm_print(mtcsAsm);
    /* Free up reg info memory.  */
    mtcs_reg_free_reg_info/*!free_reg_info*/(mtcsReg);
    if (! mtcsOptionsItem->x_quiet_flag)
      fflush (mtcsAsm->asmFile/*!asm_out_file*/);

    /* Note that for those inline functions where we don't initially
       know for certain that we will be generating an out-of-line copy,
       the first invocation of this routine (rest_of_compilation) will
       skip over this code by doing a `goto exit_rest_of_compilation;'.
       Later on, wrapup_global_declarations will (indirectly) call
       rest_of_compilation again for those inline functions that need
       to have out-of-line copies generated.  During that call, we
       *will* be routed past here.  */

    //timevar_push (TV_SYMOUT);
    if (!DECL_IGNORED_P (current_function_decl))
      mtcs_debug_function_decl/*!debug_hooks->function_decl*/(mtcsDebug,current_function_decl);
   // timevar_pop (TV_SYMOUT);

    /* Release the blocks that are linked to DECL_INITIAL() to free the memory.  */
    DECL_INITIAL (current_function_decl) = error_mark_node;

    if (DECL_STATIC_CONSTRUCTOR (current_function_decl)
        && mtcsTarget/*!targetm.have_ctors_dtors*/->have_ctors_dtors)
       target_asm_out_constructor/*!targetm.asm_out.constructor*/(mtcsMachine->asmOut,
             XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,current_function_decl), 0),
                   decl_init_priority_lookup (current_function_decl));
    if (DECL_STATIC_DESTRUCTOR (current_function_decl)
        && mtcsTarget/*!targetm.have_ctors_dtors*/->have_ctors_dtors)
       target_asm_out_destructor/*!targetm.asm_out.destructor*/(mtcsMachine->asmOut,
             XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,current_function_decl), 0),
                  decl_fini_priority_lookup (current_function_decl));

    return 0;
}

/* Given the body of an INSN known to be generated by an ASM statement, return
   the number of machine instructions likely to be generated for this insn.
   This is used to compute its length.  */
static int asm_insn_count (rtx body)
{
   const char *templ;
   if (GET_CODE (body) == ASM_INPUT)
      templ = XSTR (body, 0);
   else
      templ = decode_asm_operands (body, NULL, NULL, NULL, NULL, NULL);
   return asm_str_count (templ);
}

/* Obtain the current length of an insn.  If branch shortening has been done,
   get its actual length.  Otherwise, use FALLBACK_FN to calculate the
   length.  */
//原型 get_attr_length_1 final.cc
static int get_attr_length_1 (MtcsFinal *self,rtx_insn *insn, int (*fallback_fn) (MtcsInsnAttr *,rtx_insn *))
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);
   MtcsEmit *mtcsEmit = mtcs_target_get_emit(mtcsTarget);

   rtx body;
   int i;
   int length = 0;

   if (!mtcs_insn_attr_get_have_attr_length/*!HAVE_ATTR_length*/(mtcsInsnAttr))
      return 0;

   if (self->insn_lengths_max_uid > INSN_UID (insn))
      return self->insn_lengths[INSN_UID (insn)];
   else
      switch (GET_CODE (insn)){
         case NOTE:
         case BARRIER:
         case CODE_LABEL:
         case DEBUG_INSN:
            return 0;

         case CALL_INSN:
         case JUMP_INSN:
            length = fallback_fn (mtcsInsnAttr,insn);
            break;

         case INSN:
            body = PATTERN (insn);
            if (GET_CODE (body) == USE || GET_CODE (body) == CLOBBER)
               return 0;
            else if (GET_CODE (body) == ASM_INPUT || asm_noperands (body) >= 0)
               length = asm_insn_count (body) * fallback_fn (mtcsInsnAttr,insn);
            else if (rtx_sequence *seq = dyn_cast <rtx_sequence *> (body))
               for (i = 0; i < seq->len (); i++)
                  length += get_attr_length_1(self,seq->insn (i), fallback_fn);
            else
               length = fallback_fn (mtcsInsnAttr,insn);
            break;

         default:
            break;
      }

   if(mtcsEmit->adjust_insn_length){
      length = mtcsEmit->adjust_insn_length(mtcsEmit,insn,length);
   }
   /*!
#ifdef ADJUST_INSN_LENGTH // host=0 nvptx=0
   ADJUST_INSN_LENGTH (insn, length);
#endif
   */
   return length;
}

/* Obtain the current length of an insn.  If branch shortening has been done,
   get its actual length.  Otherwise, get its minimum length.  */
//原型 get_attr_min_length output.h final.cc
int mtcs_final_get_attr_min_length (MtcsFinal *self,rtx_insn *insn)
{
  return get_attr_length_1 (self,insn, mtcs_insn_attr_insn_min_length/*!insn_min_length*/);
}

/* Obtain the current length of an insn.  If branch shortening has been done,
   get its actual length.  Otherwise, get its maximum length.  */
//原型 get_attr_length output.h final.cc
int mtcs_final_get_attr_length (MtcsFinal *self,rtx_insn *insn)
{
  return get_attr_length_1 (self,insn, mtcs_insn_attr_insn_default_length/*!insn_default_length*/);
}

/* Compute branch alignments based on CFG profile.  */
//原型 compute_alignments rtl.h final.cc
void mtcs_final_compute_alignments (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsLoopinit *mtcsLoopinit =mtcs_target_get_loopinit(mtcsTarget);
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   basic_block bb;
   align_flags max_alignment;

   self->label_align.truncate (0);

   self->max_labelno = mtcs_rtl_max_label_num/*!max_label_num*/(mtcsRTL);
   self->min_labelno = mtcs_rtl_data_get_first_label_num/*!get_first_label_num*/(mtcsRtlData);
   self->label_align.safe_grow_cleared (self->max_labelno - self->min_labelno + 1, true);

   /* If not optimizing or optimizing for size, don't assign any alignments.  */
   if (!mtcsOptionsItem->x_optimize || optimize_function_for_size_p (cfun))
      return;

   if (dump_file){
      dump_reg_info (dump_file);
      mtcs_cfg_context_dump_flow_info/*!dump_flow_info*/(mtcsCfgContext,dump_file, TDF_DETAILS);
      flow_loops_dump (dump_file, NULL, 1);
   }
   mtcs_loopinit_loop_optimizer_init/*!loop_optimizer_init*/(mtcsLoopinit,AVOID_CFG_MODIFICATIONS);
   profile_count count_threshold = cfun->cfg->count_max / mtcsOptionsItem->x_param_align_threshold;

   if (dump_file){
      fprintf (dump_file, "count_max: ");
      cfun->cfg->count_max.dump (dump_file);
      fprintf (dump_file, "\n");
   }
   FOR_EACH_BB_FN (bb, cfun){
      rtx_insn *label = BB_HEAD (bb);
      bool has_fallthru = 0;
      edge e;
      edge_iterator ei;

      if (!LABEL_P (label) || optimize_bb_for_size_p (bb)){
         if (dump_file)
            fprintf (dump_file, "BB %4i loop %2i loop_depth %2i skipped.\n",
         bb->index,  bb->loop_father->num,  bb_loop_depth (bb));
         continue;
      }
      max_alignment =mtcs_align_get_label_align/*!LABEL_ALIGN*/(mtcsAlign,label);
      profile_count fallthru_count = profile_count::zero ();
      profile_count branch_count = profile_count::zero ();

      FOR_EACH_EDGE (e, ei, bb->preds){
         if (e->flags & EDGE_FALLTHRU)
            has_fallthru = 1, fallthru_count += e->count ();
         else
            branch_count += e->count ();
      }
      if (dump_file){
         fprintf (dump_file, "BB %4i loop %2i loop_depth %2i fall ",bb->index, bb->loop_father->num,bb_loop_depth (bb));
         fallthru_count.dump (dump_file);
         fprintf (dump_file, " branch ");
         branch_count.dump (dump_file);
         if (!bb->loop_father->inner && bb->loop_father->num)
            fprintf (dump_file, " inner_loop");
         if (bb->loop_father->header == bb)
            fprintf (dump_file, " loop_header");
         fprintf (dump_file, "\n");
      }
      if (!fallthru_count.initialized_p () || !branch_count.initialized_p ())
         continue;

      /* There are two purposes to align block with no fallthru incoming edge:
      1) to avoid fetch stalls when branch destination is near cache boundary
      2) to improve cache efficiency in case the previous block is not executed
      (so it does not need to be in the cache).

      We to catch first case, we align frequently executed blocks.
      To catch the second, we align blocks that are executed more frequently
      than the predecessor and the predecessor is likely to not be executed
      when function is called.  */

      if (!has_fallthru  && (branch_count > count_threshold
      || (bb->count > bb->prev_bb->count * 10
      && (bb->prev_bb->count <= ENTRY_BLOCK_PTR_FOR_FN (cfun)->count / 2)))){
         align_flags alignment = mtcsOptionsItem->x_flag_align_jumps/*!JUMP_ALIGN (label)*/;
         if (dump_file)
            fprintf (dump_file, "  jump alignment added.\n");
         max_alignment = align_flags::max (max_alignment, alignment);
      }
      /* In case block is frequent and reached mostly by non-fallthru edge,
      align it.  It is most likely a first block of loop.  */
      if (has_fallthru
      && !(single_succ_p (bb)
      && single_succ (bb) == EXIT_BLOCK_PTR_FOR_FN (cfun))
      && optimize_bb_for_speed_p (bb)
      && branch_count + fallthru_count > count_threshold
      && (branch_count > fallthru_count * mtcsOptionsItem->x_param_align_loop_iterations)){
         align_flags alignment = mtcsOptionsItem->x_flag_align_loops/*!LOOP_ALIGN (label)*/;
         if (dump_file)
            fprintf (dump_file, "  internal loop alignment added.\n");
         max_alignment = align_flags::max (max_alignment, alignment);
      }
      LABEL_TO_ALIGNMENT (label) = max_alignment ;
   }

   loop_optimizer_finalize ();
   free_dominance_info (CDI_DOMINATORS);
}

/* Grow the LABEL_ALIGN array after new labels are created.  */
static void grow_label_align (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   int old = self->max_labelno;
   int n_labels;
   int n_old_labels;

   self->max_labelno = mtcs_rtl_max_label_num/*!max_label_num*/(mtcsRTL);

   n_labels = self->max_labelno - self->min_labelno + 1;
   n_old_labels = old - self->min_labelno + 1;

   self->label_align.safe_grow_cleared (n_labels, true);

   /* Range of labels grows monotonically in the function.  Failing here
   means that the initialization of array got lost.  */
   gcc_assert (n_old_labels <= n_labels);
}


/* The differences in addresses
   between a branch and its target might grow or shrink depending on
   the alignment the start insn of the range (the branch for a forward
   branch or the label for a backward branch) starts out on; if these
   differences are used naively, they can even oscillate infinitely.
   We therefore want to compute a 'worst case' address difference that
   is independent of the alignment the start insn of the range end
   up on, and that is at least as large as the actual difference.
   The function align_fuzz calculates the amount we have to add to the
   naively computed difference, by traversing the part of the alignment
   chain of the start insn of the range that is in front of the end insn
   of the range, and considering for each alignment the maximum amount
   that it might contribute to a size increase.

   For casesi tables, we also want to know worst case minimum amounts of
   address difference, in case a machine description wants to introduce
   some common offset that is added to all offsets in a table.
   For this purpose, align_fuzz with a growth argument of 0 computes the
   appropriate adjustment.  */

/* Compute the maximum delta by which the difference of the addresses of
   START and END might grow / shrink due to a different address for start
   which changes the size of alignment insns between START and END.
   KNOWN_ALIGN_LOG is the alignment known for START.
   GROWTH should be ~0 if the objective is to compute potential code size
   increase, and 0 if the objective is to compute potential shrink.
   The return value is undefined for any other value of GROWTH.  */

static int align_fuzz (MtcsFinal *self,rtx start, rtx end, int known_align_log, unsigned int growth)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAddr *mtcsAddr=mtcs_target_get_addr(mtcsTarget);

   int uid = INSN_UID (start);
   rtx align_label;
   int known_align = 1 << known_align_log;
   int end_shuid = INSN_SHUID (end);
   int fuzz = 0;

   for (align_label = self->uid_align[uid]; align_label; align_label = self->uid_align[uid]){
      int align_addr, new_align;

      uid = INSN_UID (align_label);
      align_addr = mtcs_addr_insn_addresses/*!INSN_ADDRESSES*/(mtcsAddr,uid) - self->insn_lengths[uid];
      if (self->uid_shuid[uid] > end_shuid)
         break;
      align_flags alignment = LABEL_TO_ALIGNMENT (align_label);
      new_align = 1 << alignment.levels[0].log;
      if (new_align < known_align)
         continue;
      fuzz += (-align_addr ^ growth) & (new_align - known_align);
      known_align = new_align;
   }
   return fuzz;
}

/* Make a pass over all insns and compute their actual lengths by shortening
   any branches of variable length if possible.  */

/* shorten_branches might be called multiple times:  for example, the SH
   port splits out-of-range conditional branches in MACHINE_DEPENDENT_REORG.
   In order to do this, it needs proper length information, which it obtains
   by calling shorten_branches.  This cannot be collapsed with
   shorten_branches itself into a single pass unless we also want to integrate
   reorg.cc, since the branch splitting exposes new instructions with delay
   slots.  */
//原型 shorten_branches output.h(insn-attr.h host也声明) final.cc
void mtcs_final_shorten_branches (MtcsFinal *self,rtx_insn *first)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);
   MtcsAddr *mtcsAddr=mtcs_target_get_addr(mtcsTarget);
   MtcsAlign *mtcsAlign =mtcs_target_get_align(mtcsTarget);
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   MtcsEmit *mtcsEmit = mtcs_target_get_emit(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   rtx_insn *insn;
   int max_uid;
   int i;
   rtx_insn *seq;
   bool something_changed = true;
   char *varying_length;
   rtx body;
   int uid;
   rtx align_tab[MAX_CODE_ALIGN + 1];

   /* Compute maximum UID and allocate label_align / uid_shuid.  */
   max_uid = mtcs_rtl_data_get_max_uid/*!get_max_uid*/(mtcsRtlData);

   /* Free uid_shuid before reallocating it.  */
   free (self->uid_shuid);

   self->uid_shuid = XNEWVEC (int, max_uid);

   if (self->max_labelno != mtcs_rtl_max_label_num/*!max_label_num*/(mtcsRTL))
      grow_label_align(self);

   /* Initialize label_align and set up uid_shuid to be strictly
   monotonically rising with insn order.  */
   /* We use alignment here to keep track of the maximum alignment we want to
   impose on the next CODE_LABEL (or the current one if we are processing
   the CODE_LABEL itself).  */

   align_flags max_alignment;

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData), i = 1; insn; insn = NEXT_INSN (insn)){
      INSN_SHUID (insn) = i++;
      if (INSN_P (insn))
         continue;

      if (rtx_code_label *label = dyn_cast <rtx_code_label *> (insn)){
         /* Merge in alignments computed by compute_alignments.  */
         align_flags alignment = LABEL_TO_ALIGNMENT (label);
         max_alignment = align_flags::max (max_alignment, alignment);

         rtx_jump_table_data *table = jump_table_for_label (label);
         if (!table){
            align_flags alignment = mtcs_align_get_label_align/*!LABEL_ALIGN*/(mtcsAlign,label);
            max_alignment = align_flags::max (max_alignment, alignment);
         }
         /* ADDR_VECs only take room if read-only data goes into the text
         section.  */
         if ((mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)
         || mtcsAsm->readonly_data_section == text_section)  && table){
            align_flags alignment = align_flags (mtcs_align_get_addr_vec_align/*!ADDR_VEC_ALIGN*/(mtcsAlign,table));
            max_alignment = align_flags::max (max_alignment, alignment);
         }
         LABEL_TO_ALIGNMENT (label) = max_alignment;
         max_alignment = align_flags ();
      }else if (BARRIER_P (insn)){
         rtx_insn *label;

         for (label = insn; label && ! INSN_P (label); label = NEXT_INSN (label))
            if (LABEL_P (label)){
               align_flags alignment  = align_flags (
                     mtcs_align_get_label_align_after_barrier/*!LABEL_ALIGN_AFTER_BARRIER*/(mtcsAlign,insn));
               max_alignment = align_flags::max (max_alignment, alignment);
               break;
            }
      }
   }
   if (!mtcs_insn_attr_get_have_attr_length/*!HAVE_ATTR_length*/(mtcsInsnAttr))
      return;

   /* Allocate the rest of the arrays.  */
   self->insn_lengths = XNEWVEC (int, max_uid);
   self->insn_lengths_max_uid = max_uid;
   /* Syntax errors can lead to labels being outside of the main insn stream.
   Initialize insn_addresses, so that we get reproducible results.  */
   mtcs_addr_insn_addresses_alloc/*!INSN_ADDRESSES_ALLOC*/(mtcsAddr,max_uid);

   varying_length = XCNEWVEC (char, max_uid);

   /* Initialize uid_align.  We scan instructions
   from end to start, and keep in align_tab[n] the last seen insn
   that does an alignment of at least n+1, i.e. the successor
   in the alignment chain for an insn that does / has a known
   alignment of n.  */
   self->uid_align = XCNEWVEC (rtx, max_uid);

   for (i = MAX_CODE_ALIGN + 1; --i >= 0;)
      align_tab[i] = NULL_RTX;
   seq = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   for (; seq; seq = PREV_INSN (seq)){
      int uid = INSN_UID (seq);
      int log;
      log = (LABEL_P (seq) ? LABEL_TO_ALIGNMENT (seq).levels[0].log : 0);
      self->uid_align[uid] = align_tab[0];
      if (log){
         /* Found an alignment label.  */
         gcc_checking_assert (log < MAX_CODE_ALIGN + 1);
         self->uid_align[uid] = align_tab[log];
         for (i = log - 1; i >= 0; i--)
            align_tab[i] = seq;
      }
   }

   /* When optimizing, we start assuming minimum length, and keep increasing
   lengths as we find the need for this, till nothing changes.
   When not optimizing, we start assuming maximum lengths, and
   do a single pass to update the lengths.  */
   bool increasing = mtcsOptionsItem->x_optimize != 0;

   if(mtcsMode->case_vector_shorten_mode!=NULL/*!#ifdef CASE_VECTOR_SHORTEN_MODE*/){
      if (mtcsOptionsItem->x_optimize){
         /* Look for ADDR_DIFF_VECs, and initialize their minimum and maximum
         label fields.  */

         int min_shuid = INSN_SHUID (mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData)) - 1;
         int max_shuid = INSN_SHUID (mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData)) + 1;
         int rel;

         for (insn = first; insn != 0; insn = NEXT_INSN (insn)){
            rtx min_lab = NULL_RTX, max_lab = NULL_RTX, pat;
            int len, i, min, max, insn_shuid;
            int min_align;
            addr_diff_vec_flags flags;

            if (! JUMP_TABLE_DATA_P (insn) || GET_CODE (PATTERN (insn)) != ADDR_DIFF_VEC)
               continue;
            pat = PATTERN (insn);
            len = XVECLEN (pat, 1);
            gcc_assert (len > 0);
            min_align = MAX_CODE_ALIGN;
            for (min = max_shuid, max = min_shuid, i = len - 1; i >= 0; i--){
               rtx lab = XEXP (XVECEXP (pat, 1, i), 0);
               int shuid = INSN_SHUID (lab);
               if (shuid < min){
                  min = shuid;
                  min_lab = lab;
               }
               if (shuid > max){
                  max = shuid;
                  max_lab = lab;
               }

               int label_alignment = LABEL_TO_ALIGNMENT (lab).levels[0].log;
               if (min_align > label_alignment)
                  min_align = label_alignment;
            }
            XEXP (pat, 2) = gen_rtx_LABEL_REF (Pmode, min_lab);
            XEXP (pat, 3) = gen_rtx_LABEL_REF (Pmode, max_lab);
            insn_shuid = INSN_SHUID (insn);
            rel = INSN_SHUID (XEXP (XEXP (pat, 0), 0));
            memset (&flags, 0, sizeof (flags));
            flags.min_align = min_align;
            flags.base_after_vec = rel > insn_shuid;
            flags.min_after_vec  = min > insn_shuid;
            flags.max_after_vec  = max > insn_shuid;
            flags.min_after_base = min > rel;
            flags.max_after_base = max > rel;
            ADDR_DIFF_VEC_FLAGS (pat) = flags;

            if (increasing)
               mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,
                     pat,mtcsMode->case_vector_shorten_mode/*!CASE_VECTOR_SHORTEN_MODE*/(mtcsMode,0, 0, pat));
         }
      }
   } //#endif /* CASE_VECTOR_SHORTEN_MODE */

   /* Compute initial lengths, addresses, and varying flags for each insn.  */
   int (*length_fun) (MtcsInsnAttr *,rtx_insn *) = increasing ?
         mtcs_insn_attr_insn_min_length/*!insn_min_length*/ : mtcs_insn_attr_insn_default_length/*!insn_default_length*/;

   for (mtcsAddr->insn_current_address/*!insn_current_address*/  = 0, insn = first;
   insn != 0;  mtcsAddr->insn_current_address/*!insn_current_address*/ += self->insn_lengths[uid], insn = NEXT_INSN (insn)){
      uid = INSN_UID (insn);

      self->insn_lengths[uid] = 0;

      if (LABEL_P (insn)){
         int log = LABEL_TO_ALIGNMENT (insn).levels[0].log;
         if (log){
            int align = 1 << log;
            int new_address = (mtcsAddr->insn_current_address/*!insn_current_address*/  + align - 1) & -align;
            self->insn_lengths[uid] = new_address - mtcsAddr->insn_current_address/*!insn_current_address*/ ;
         }
      }

      mtcsAddr->insn_addresses_[uid]/*!INSN_ADDRESSES*/ = mtcsAddr->insn_current_address/*!insn_current_address*/  + self->insn_lengths[uid];

      if (NOTE_P (insn) || BARRIER_P (insn)  || LABEL_P (insn) || DEBUG_INSN_P (insn))
         continue;
      if (insn->deleted ())
         continue;

      body = PATTERN (insn);
      if (rtx_jump_table_data *table = dyn_cast <rtx_jump_table_data *> (insn)){
         /* This only takes room if read-only data goes into the text
         section.  */
         if (mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)
         || mtcsAsm->readonly_data_section == text_section)
            self->insn_lengths[uid] = (XVECLEN (body, GET_CODE (body) == ADDR_DIFF_VEC) * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,table->get_data_mode ()));
            /* Alignment is handled by ADDR_VEC_ALIGN.  */
      }else if (GET_CODE (body) == ASM_INPUT || asm_noperands (body) >= 0)
         self->insn_lengths[uid] = asm_insn_count (body)
                       * mtcs_insn_attr_insn_default_length/*!insn_default_length*/(mtcsInsnAttr,insn);
      else if (rtx_sequence *body_seq = dyn_cast <rtx_sequence *> (body)){
         int i;
         int const_delay_slots;
         if (mtcs_insn_attr_get_delay_slots/*!DELAY_SLOTS*/(mtcsInsnAttr))
            const_delay_slots = mtcs_insn_attr_const_num_delay_slots/*!const_num_delay_slots*/(mtcsInsnAttr,body_seq->insn (0));
         else
            const_delay_slots = 0;

         int (*inner_length_fun) (MtcsInsnAttr *,rtx_insn *)  = const_delay_slots
         ? length_fun : mtcs_insn_attr_insn_default_length/*!insn_default_length*/;
         /* Inside a delay slot sequence, we do not do any branch shortening
         if the shortening could change the number of delay slots
         of the branch.  */
         for (i = 0; i < body_seq->len (); i++){
            rtx_insn *inner_insn = body_seq->insn (i);
            int inner_uid = INSN_UID (inner_insn);
            int inner_length;

            if (GET_CODE (PATTERN (inner_insn)) == ASM_INPUT  || asm_noperands (PATTERN (inner_insn)) >= 0)
               inner_length = (asm_insn_count (PATTERN (inner_insn))
                     * mtcs_insn_attr_insn_default_length/*!insn_default_length*/(mtcsInsnAttr,inner_insn));
            else
               inner_length = inner_length_fun (mtcsInsnAttr,inner_insn);

            self->insn_lengths[inner_uid] = inner_length;
            if (const_delay_slots){
               if ((varying_length[inner_uid] = mtcs_insn_attr_is_insn_variable_length_p/*!insn_variable_length_p*/
                     (mtcsInsnAttr,inner_insn)) != 0)
                  varying_length[uid] = 1;
               mtcsAddr->insn_addresses_[inner_uid]/*!INSN_ADDRESSES*/ = (mtcsAddr->insn_current_address/*!insn_current_address*/
                     + self->insn_lengths[uid]);
            }else
               varying_length[inner_uid] = 0;
            self->insn_lengths[uid] += inner_length;
         }
      }else if (GET_CODE (body) != USE && GET_CODE (body) != CLOBBER){
         self->insn_lengths[uid] = length_fun(mtcsInsnAttr,insn);
         varying_length[uid] = insn_variable_length_p (insn);
      }

      /* If needed, do any adjustment.  */
      if(mtcsEmit->adjust_insn_length){
         self->insn_lengths[uid] = mtcsEmit->adjust_insn_length(mtcsEmit,insn,self->insn_lengths[uid]);
         if (self->insn_lengths[uid] < 0)
            fatal_insn ("negative insn length", insn);
      }
      /*
      #ifdef ADJUST_INSN_LENGTH
      ADJUST_INSN_LENGTH (insn, self->insn_lengths[uid]);
      if (self->insn_lengths[uid] < 0)
      fatal_insn ("negative insn length", insn);
      #endif
      */
   }

   /* Now loop over all the insns finding varying length insns.  For each,
   get the current insn length.  If it has changed, reflect the change.
   When nothing changes for a full pass, we are done.  */

   while (something_changed){
      something_changed = false;
      self->insn_current_align = MAX_CODE_ALIGN - 1;
      for (mtcsAddr->insn_current_address/*!insn_current_address*/  = 0, insn = first; insn != 0;  insn = NEXT_INSN (insn)){
         int new_length;
         //#ifdef ADJUST_INSN_LENGTH
         int tmp_length;
         //#endif
         int length_align;

         uid = INSN_UID (insn);

         if (rtx_code_label *label = dyn_cast <rtx_code_label *> (insn)){
            int log = LABEL_TO_ALIGNMENT (label).levels[0].log;

            if(mtcsMode->case_vector_shorten_mode/*!#ifdef CASE_VECTOR_SHORTEN_MODE*/!=NULL){
               /* If the mode of a following jump table was changed, we
               may need to update the alignment of this label.  */

               if (mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)
               || mtcsAsm->readonly_data_section == text_section){
                  rtx_jump_table_data *table = jump_table_for_label (label);
                  if (table){
                     int newlog = mtcs_align_get_addr_vec_align/*!ADDR_VEC_ALIGN*/(mtcsAlign,table);
                     if (newlog != log){
                        log = newlog;
                        LABEL_TO_ALIGNMENT (insn) = log;
                        something_changed = true;
                     }
                  }
               }
            } //#endif

            if (log > self->insn_current_align){
               int align = 1 << log;
               int new_address= (mtcsAddr->insn_current_address/*!insn_current_address*/  + align - 1) & -align;
               self->insn_lengths[uid] = new_address - mtcsAddr->insn_current_address/*!insn_current_address*/;
               self->insn_current_align = log;
               mtcsAddr->insn_current_address/*!insn_current_address*/  = new_address;
            }else
               self->insn_lengths[uid] = 0;
            mtcsAddr->insn_addresses_[uid]/*!INSN_ADDRESSES*/  = mtcsAddr->insn_current_address/*!insn_current_address*/;
            continue;
         }

         length_align = mtcs_align_get_insn_length_alignment/*!INSN_LENGTH_ALIGNMENT*/(mtcsAlign,insn);
         if (length_align < self->insn_current_align)
            self->insn_current_align = length_align;

         self->insn_last_address = mtcs_addr_insn_addresses/*!INSN_ADDRESSES*/(mtcsAddr,uid);
         mtcsAddr->insn_addresses_[uid]/*!INSN_ADDRESSES*/ = mtcsAddr->insn_current_address/*!insn_current_address*/;

         if(mtcsMode->case_vector_shorten_mode!=NULL/*!#ifdef CASE_VECTOR_SHORTEN_MODE*/){
            if (mtcsOptionsItem->x_optimize  && JUMP_TABLE_DATA_P (insn)  && GET_CODE (PATTERN (insn)) == ADDR_DIFF_VEC){
               rtx_jump_table_data *table = as_a <rtx_jump_table_data *> (insn);
               rtx body = PATTERN (insn);
               int old_length = self->insn_lengths[uid];
               rtx_insn *rel_lab =    safe_as_a <rtx_insn *> (XEXP (XEXP (body, 0), 0));
               rtx min_lab = XEXP (XEXP (body, 2), 0);
               rtx max_lab = XEXP (XEXP (body, 3), 0);
               int rel_addr = INSN_ADDRESSES (INSN_UID (rel_lab));
               int min_addr = INSN_ADDRESSES (INSN_UID (min_lab));
               int max_addr = INSN_ADDRESSES (INSN_UID (max_lab));
               rtx_insn *prev;
               int rel_align = 0;
               addr_diff_vec_flags flags;
               scalar_int_mode vec_mode;

               /* Avoid automatic aggregate initialization.  */
               flags = ADDR_DIFF_VEC_FLAGS (body);

               /* Try to find a known alignment for rel_lab.  */
               for (prev = rel_lab; prev && ! self->insn_lengths[INSN_UID (prev)]
                                         && ! (varying_length[INSN_UID (prev)] & 1); prev = PREV_INSN (prev))
                  if (varying_length[INSN_UID (prev)] & 2) {
                     rel_align = LABEL_TO_ALIGNMENT (prev).levels[0].log;
                     break;
                  }

               /* See the comment on addr_diff_vec_flags in rtl.h for the
               meaning of the flags values.  base: REL_LAB   vec: INSN  */
               /* Anything after INSN has still addresses from the last
               pass; adjust these so that they reflect our current
               estimate for this pass.  */
               if (flags.base_after_vec)
                  rel_addr += mtcsAddr->insn_current_address/*!insn_current_address*/ - self->insn_last_address;
               if (flags.min_after_vec)
                  min_addr += mtcsAddr->insn_current_address/*!insn_current_address*/ - self->insn_last_address;
               if (flags.max_after_vec)
                  max_addr += mtcsAddr->insn_current_address/*!insn_current_address*/ - self->insn_last_address;
               /* We want to know the worst case, i.e. lowest possible value
               for the offset of MIN_LAB.  If MIN_LAB is after REL_LAB,
               its offset is positive, and we have to be wary of code shrink;
               otherwise, it is negative, and we have to be vary of code
               size increase.  */
               if (flags.min_after_base){
                  /* If INSN is between REL_LAB and MIN_LAB, the size
                  changes we are about to make can change the alignment
                  within the observed offset, therefore we have to break
                  it up into two parts that are independent.  */
                  if (! flags.base_after_vec && flags.min_after_vec){
                     min_addr -= align_fuzz(self,rel_lab, insn, rel_align, 0);
                     min_addr -= align_fuzz(self,insn, min_lab, 0, 0);
                  }else
                     min_addr -= align_fuzz(self,rel_lab, min_lab, rel_align, 0);
               }else{
                  if (flags.base_after_vec && ! flags.min_after_vec){
                     min_addr -= align_fuzz(self,min_lab, insn, 0, ~0);
                     min_addr -= align_fuzz(self,insn, rel_lab, 0, ~0);
                  }else
                     min_addr -= align_fuzz(self,min_lab, rel_lab, 0, ~0);
               }
               /* Likewise, determine the highest lowest possible value
               for the offset of MAX_LAB.  */
               if (flags.max_after_base){
                  if (! flags.base_after_vec && flags.max_after_vec){
                     max_addr += align_fuzz(self,rel_lab, insn, rel_align, ~0);
                     max_addr += align_fuzz(self,insn, max_lab, 0, ~0);
                  }else
                     max_addr += align_fuzz(self,rel_lab, max_lab, rel_align, ~0);
               }else{
                  if (flags.base_after_vec && ! flags.max_after_vec){
                     max_addr += align_fuzz(self,max_lab, insn, 0, 0);
                     max_addr += align_fuzz(self,insn, rel_lab, 0, 0);
                  }else
                     max_addr += align_fuzz(self,max_lab, rel_lab, 0, 0);
               }
               vec_mode = mtcsMode->case_vector_shorten_mode/*!CASE_VECTOR_SHORTEN_MODE*/(mtcsMode,
                     min_addr - rel_addr,max_addr - rel_addr, body);
               if (!increasing
               || (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,vec_mode) >=
                     mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,table->get_data_mode ())))
                  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,body, vec_mode);
               if (mtcs_asm_jump_tables_in_text_section/*!JUMP_TABLES_IN_TEXT_SECTION*/(mtcsAsm)
               || mtcsAsm->readonly_data_section == text_section){
                  self->insn_lengths[uid]= (XVECLEN (body, 1) * mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,table->get_data_mode ()));
                  mtcsAddr->insn_current_address/*!insn_current_address*/ += self->insn_lengths[uid];
                  if (self->insn_lengths[uid] != old_length)
                     something_changed = true;
               }
               continue;
            }
         }//#endif /* CASE_VECTOR_SHORTEN_MODE */

         if (! (varying_length[uid])){
            if (NONJUMP_INSN_P (insn)  && GET_CODE (PATTERN (insn)) == SEQUENCE){
               int i;
               body = PATTERN (insn);
               for (i = 0; i < XVECLEN (body, 0); i++){
                  rtx inner_insn = XVECEXP (body, 0, i);
                  int inner_uid = INSN_UID (inner_insn);
                  mtcsAddr->insn_addresses_[inner_uid]/*!INSN_ADDRESSES*/ = mtcsAddr->insn_current_address/*!insn_current_address*/;
                  mtcsAddr->insn_current_address/*!insn_current_address*/ += self->insn_lengths[inner_uid];
               }
            }else
               mtcsAddr->insn_current_address/*!insn_current_address*/ += self->insn_lengths[uid];

            continue;
         }

         if (NONJUMP_INSN_P (insn) && GET_CODE (PATTERN (insn)) == SEQUENCE){
            rtx_sequence *seqn = as_a <rtx_sequence *> (PATTERN (insn));
            int i;

            body = PATTERN (insn);
            new_length = 0;
            for (i = 0; i < seqn->len (); i++){
               rtx_insn *inner_insn = seqn->insn (i);
               int inner_uid = INSN_UID (inner_insn);
               int inner_length;

               mtcsAddr->insn_addresses_[inner_uid]/*!INSN_ADDRESSES*/ = mtcsAddr->insn_current_address/*!insn_current_address*/;

               /* insn_current_length returns 0 for insns with a
               non-varying length.  */
               if (! varying_length[inner_uid])
                  inner_length = self->insn_lengths[inner_uid];
               else
                  inner_length = insn_current_length (inner_insn);

               if (inner_length != self->insn_lengths[inner_uid]){
                  if (!increasing || inner_length > self->insn_lengths[inner_uid]){
                     self->insn_lengths[inner_uid] = inner_length;
                     something_changed = true;
                  }else
                     inner_length = self->insn_lengths[inner_uid];
               }
               mtcsAddr->insn_current_address/*!insn_current_address*/ += inner_length;
               new_length += inner_length;
            }
         }else{
            new_length = insn_current_length (insn);
            mtcsAddr->insn_current_address/*!insn_current_address*/ += new_length;
         }

         if(mtcsEmit->adjust_insn_length){
            tmp_length = new_length;
            new_length = mtcsEmit->adjust_insn_length(mtcsEmit,insn,new_length);

            mtcsAddr->insn_current_address/*!insn_current_address*/ += (new_length - tmp_length);
         }
         /*
         #ifdef ADJUST_INSN_LENGTH
         // If needed, do any adjustment.
         tmp_length = new_length;
         ADJUST_INSN_LENGTH (insn, new_length);
         insn_current_address += (new_length - tmp_length);
         #endif
         */

         if (new_length != self->insn_lengths[uid]  && (!increasing || new_length > self->insn_lengths[uid])){
            self->insn_lengths[uid] = new_length;
            something_changed = true;
         }else
            mtcsAddr->insn_current_address/*!insn_current_address*/ += self->insn_lengths[uid] - new_length;
      }
      /* For a non-optimizing compile, do only a single pass.  */
      if (!increasing)
         break;
   }//end while
   mtcsRtlData/*!crtl*/->max_insn_address = mtcsAddr->insn_current_address/*!insn_current_address*/;
   free (varying_length);
}

static unsigned int rest_of_handle_shorten_branches (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   /* Shorten branches.  */
   mtcs_final_shorten_branches/*!shorten_branches*/(self,mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData));
   return 0;
}

/* Indicate that branch shortening hasn't yet been done.  */
//原型 init_insn_lengths output.h final.cc
void mtcs_final_init_insn_lengths (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAddr *mtcsAddr=mtcs_target_get_addr(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);

   if (self->uid_shuid){
      free (self->uid_shuid);
      self->uid_shuid = 0;
   }
   if (self->insn_lengths){
      free (self->insn_lengths);
      self->insn_lengths = 0;
      self->insn_lengths_max_uid = 0;
   }
   if (mtcs_insn_attr_get_have_attr_length/*!HAVE_ATTR_length*/(mtcsInsnAttr))
      mtcs_addr_insn_addresses_free/*!INSN_ADDRESSES_FREE*/(mtcsAddr);
   if (self->uid_align){
      free (self->uid_align);
      self->uid_align = 0;
   }
}

static unsigned int rest_of_clean_state (MtcsFinal *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig=mtcs_target_get_config(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   rtx_insn *insn, *next;
   FILE *final_output = NULL;
   int save_unnumbered = mtcsOptionsItem->x_flag_dump_unnumbered;
   int save_noaddr = mtcsOptionsItem->x_flag_dump_noaddr;

   if (mtcsOptionsItem->x_flag_dump_final_insns){
      final_output = fopen (mtcsOptionsItem->x_flag_dump_final_insns, "a");
      if (!final_output){
         error ("could not open final insn dump file %qs: %m",mtcsOptionsItem->x_flag_dump_final_insns);
         mtcsOptionsItem->x_flag_dump_final_insns = NULL;
      }else{
         mtcsOptionsItem->x_flag_dump_noaddr = mtcsOptionsItem->x_flag_dump_unnumbered = 1;
         if (mtcsOptionsItem->x_flag_compare_debug_opt || mtcsOptionsItem->x_flag_compare_debug)
            dump_flags |= TDF_NOUID | TDF_COMPARE_DEBUG;
         dump_function_header (final_output, current_function_decl,dump_flags);
         final_insns_dump_p = true;

         for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
            if (LABEL_P (insn))
               INSN_UID (insn) = CODE_LABEL_NUMBER (insn);
            else{
               if (NOTE_P (insn))
                  set_block_for_insn (insn, NULL);
               n_debug("mtcsfinal.c rest_of_clean_state 设 UID=0 :%p\n",insn);
               INSN_UID (insn) = 0;
            }
      }
   }

   /* It is very important to decompose the RTL instruction chain here:
   debug information keeps pointing into CODE_LABEL insns inside the function
   body.  If these remain pointing to the other insns, we end up preserving
   whole RTL chain and attached detailed debug info in memory.  */
   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = next){
      next = NEXT_INSN (insn);
      SET_NEXT_INSN (insn) = NULL;
      SET_PREV_INSN (insn) = NULL;

      rtx_insn *call_insn = insn;
      if (NONJUMP_INSN_P (call_insn)  && GET_CODE (PATTERN (call_insn)) == SEQUENCE){
         rtx_sequence *seq = as_a <rtx_sequence *> (PATTERN (call_insn));
         call_insn = seq->insn (0);
      }
      if (CALL_P (call_insn)){
         rtx note = find_reg_note (call_insn, REG_CALL_ARG_LOCATION, NULL_RTX);
         if (note)
            mtcs_rtlanal_remove_note/*!remove_note*/(mtcsRtlanal,call_insn, note);
      }

      if (final_output
      && (!NOTE_P (insn)
      || (NOTE_KIND (insn) != NOTE_INSN_VAR_LOCATION
      && NOTE_KIND (insn) != NOTE_INSN_BEGIN_STMT
      && NOTE_KIND (insn) != NOTE_INSN_INLINE_ENTRY
      && NOTE_KIND (insn) != NOTE_INSN_BLOCK_BEG
      && NOTE_KIND (insn) != NOTE_INSN_BLOCK_END
      && NOTE_KIND (insn) != NOTE_INSN_DELETED_DEBUG_LABEL)))
         mtcs_print_rtl_single/*!print_rtl_single*/(final_output, insn);
   }

   if (final_output){
      mtcsOptionsItem->x_flag_dump_noaddr = save_noaddr;
      mtcsOptionsItem->x_flag_dump_unnumbered = save_unnumbered;
      final_insns_dump_p = false;

      if (fclose (final_output)){
         error ("could not close final insn dump file %qs: %m", mtcsOptionsItem->x_flag_dump_final_insns);
         mtcsOptionsItem->x_flag_dump_final_insns = NULL;
      }
   }

   flag_rerun_cse_after_global_opts = 0;
   reload_completed = 0;
   epilogue_completed = 0;
   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){

   //#ifdef STACK_REGS
      mtcs_rtl_set_regstack_completed/*!regstack_completed = 0*/(mtcsRTL,0);//rtl.h声明
   //#endif
   }

   /* Clear out the insn_length contents now that they are no
   longer valid.  */
   mtcs_final_init_insn_lengths/*!init_insn_lengths*/(self);

   /* Show no temporary slots allocated.  */
   mtcs_func_init_temp_slots/*!init_temp_slots*/(mtcsFunc);

   mtcs_cfg_rtl_free_bb_for_insn/*!free_bb_for_insn*/(mtcsCfgRtl);

   if (cfun->gimple_df)
      delete_tree_ssa (cfun);

   /* We can reduce stack alignment on call site only when we are sure that
   the function body just produced will be actually used in the final
   executable.  */
   if (mtcsOptionsItem->x_flag_ipa_stack_alignment
   && mtcs_asm_decl_binds_to_current_def_p/*!decl_binds_to_current_def_p*/(mtcsAsm,current_function_decl)){
      unsigned int pref = mtcsRtlData/*!crtl*/->preferred_stack_boundary;
      if (mtcsRtlData/*!crtl*/->stack_alignment_needed > mtcsRtlData/*!crtl*/->preferred_stack_boundary)
         pref = mtcsRtlData/*!crtl*/->stack_alignment_needed;
      cgraph_node::rtl_info (current_function_decl)->preferred_incoming_stack_boundary = pref;
   }

   /* Make sure volatile mem refs aren't considered valid operands for
   arithmetic insns.  We must call this here if this is a nested inline
   function, since the above code leaves us in the init_recog state,
   and the function context push/pop code does not save/restore volatile_ok.

   ??? Maybe it isn't necessary for expand_start_function to call this
   anymore if we do it here?  */

   mtcs_recog_init_recog_no_volatile/*!init_recog_no_volatile*/(mtcsRecog);

   /* We're done with this function.  Free up memory if we can.  */
   free_after_parsing (cfun);
   mtcs_func_free_after_compilation/*!free_after_compilation*/(mtcsFunc,cfun);
   return 0;
}

static void mtcsFinalInit(MtcsFinal *self)
{
    self->final_sequence=NULL;
    self->app_on=false;
    self->saw_no_split_stack=false;
    self->force_source_line = false;
}

MtcsFinal *mtcs_final_new(MtcsMode *mtcsMode)
{
    MtcsFinal *self = n_slice_alloc0 (sizeof(MtcsFinal));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsFinalInit(self);
    return self;
}

/***********************以下是基于MtcsFinal 的4个 rtl pass **********************************/
//原型 NEXT_PASS (pass_compute_alignments, 1);  RTL_PASS  final.cc  alignments   y 无条件执行 compute_alignments
static nuint compute_alignments_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassComputeAlignments *self=(MtcsPassComputeAlignments *)mtcsPass;
      MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsFinal *mtcsFinal=mtcs_target_get_final(mtcsTarget);
      mtcs_final_compute_alignments(mtcsFinal);
      return 0;
}

static void mtcsPassComputeAlignmentsInit(MtcsPassComputeAlignments *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =compute_alignments_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassComputeAlignments *mtcs_pass_compute_alignments_new (MtcsMode *mtcsMode)
{
   MtcsPassComputeAlignments *self = n_slice_alloc0 (sizeof(MtcsPassComputeAlignments));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"alignments");
   mtcsPassComputeAlignmentsInit(self);
   return self;
}


//原型 NEXT_PASS (pass_shorten_branches, 1);  RTL_PASS  final.cc  shorten   y 无条件执行 rest_of_handle_shorten_branches
static nuint shorten_branches_execute_cb(MtcsPass *mtcsPass,function *func)
{
      MtcsPassShortenBranches *self=(MtcsPassShortenBranches *)mtcsPass;
      MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsFinal *mtcsFinal=mtcs_target_get_final(mtcsTarget);
      return rest_of_handle_shorten_branches(mtcsFinal);
}

static void mtcsPassShortenBranchesInit(MtcsPassShortenBranches *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =shorten_branches_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassShortenBranches *mtcs_pass_shorten_branches_new (MtcsMode *mtcsMode)
{
   MtcsPassShortenBranches *self = n_slice_alloc0 (sizeof(MtcsPassShortenBranches));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"shorten");
   mtcsPassShortenBranchesInit(self);
   return self;
}

//原型 NEXT_PASS (pass_final, 1);  RTL_PASS  final.cc  final   y 无条件执行 rest_of_handle_final
static nuint final_execute_cb(MtcsPass *mtcsPass,function *func)
{
      MtcsPassFinal *self=(MtcsPassFinal *)mtcsPass;
      MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsFinal *mtcsFinal=mtcs_target_get_final(mtcsTarget);
      return rest_of_handle_final(mtcsFinal);
}

static void mtcsPassFinalInit(MtcsPassFinal *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =final_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassFinal *mtcs_pass_final_new (MtcsMode *mtcsMode)
{
   MtcsPassFinal *self = n_slice_alloc0 (sizeof(MtcsPassFinal));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"final");
   mtcsPassFinalInit(self);
   return self;
}

//原型 NEXT_PASS (pass_clean_state, 1);  RTL_PASS  final.cc *clean_state   y 无条件执行 rest_of_clean_state
static nuint clean_state_execute_cb(MtcsPass *mtcsPass,function *func)
{
      MtcsPassFinal *self=(MtcsPassFinal *)mtcsPass;
      MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
      MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
      MtcsFinal *mtcsFinal=mtcs_target_get_final(mtcsTarget);
      return rest_of_clean_state(mtcsFinal);
}

static void mtcsPassCleanStateInit(MtcsPassCleanState *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =clean_state_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            PROP_rtl /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassCleanState *mtcs_pass_clean_state_new (MtcsMode *mtcsMode)
{
   MtcsPassCleanState *self = n_slice_alloc0 (sizeof(MtcsPassCleanState));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"*clean_state");
   mtcsPassCleanStateInit(self);
   return self;
}

