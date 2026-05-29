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


/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"

#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"

#include "aet/aetprinttree.h"

#include "mtcstarget.h"
#include "mtcsprintrtl.h"
#include "mtcscompile.h"
#include "mtcsvar.h"


typedef struct _VarData{
   varpool_node *node;
   tree decl;
}VarData;

static void mtcsVarInit(MtcsVar *self)
{
   self->varArray=n_ptr_array_new();
   self->hostArray=n_ptr_array_new();
}

static struct varpool_node *get(MtcsVar *self,tree decl)
{
   int len=self->hostArray->len;
   int i;
   for(i=0;i<len;i++){
      VarData *item=(VarData *)n_ptr_array_index(self->hostArray,i);
      if(item->decl==decl)
         return item->node;
   }
   return NULL;
}



/* Return varpool node assigned to DECL.  Create new one when needed.  */
//原型 varpool_node * varpool_node::get_create (tree decl) varpool.cc
//变量初始值留到mtcsport中移植。
varpool_node *mtcs_var_clone(MtcsVar *self,struct varpool_node *hostNode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   tree hostDecl = hostNode->decl;
   varpool_node *node = get (self,hostDecl);
   gcc_checking_assert (VAR_P (hostDecl));
   if (node)
      return node;

   node = varpool_node::create_empty ();
   tree newdecl=copy_node(hostDecl);
   node->decl = newdecl;


   n_debug("mtcsvar.c mtcs_var_clone 00 hostNode:%p definition:%d alias:%d aux:%d no_reorder:%d decl sysmtabnode:%p force_output:%d\n",
         hostNode,hostNode->definition,hostNode->alias,hostNode->aux,hostNode->no_reorder,
         hostNode->decl->decl_with_vis.symtab_node,hostNode->force_output);
   n_debug("mtcsvar.c mtcs_var_clone 00aa hostNode:%p analyzed:%d force_output:%d used_from_other_partition:%d newdecl:%p\n",
         hostNode,hostNode->analyzed,node->force_output,node->used_from_other_partition,newdecl);


   node->definition = hostNode->definition;
   node->alias = hostNode->alias;
   node->analyzed = hostNode->analyzed;
   node->force_output = hostNode->force_output;
   int align = TYPE_ALIGN(TREE_TYPE(hostDecl));
   int newalign = TYPE_ALIGN(TREE_TYPE(newdecl));


   n_debug("mtcsvar.c mtcs_var_clone sss align %d %d %d %d\n",align,newalign,DECL_ALIGN(hostDecl),DECL_ALIGN(newdecl));

   VarData *data=n_slice_new(VarData);
   data->node = node;
   data->decl = hostDecl;
   n_ptr_array_add(self->hostArray,data);
   symbol_table *save=symtab;
   symtab=mtcsTarget->symtab;
   node->register_symbol ();
   n_debug("mtcsvar.c mtcs_var_clone 11 node:%p definition:%d alias:%d aux:%d no_reorder:%d decl sysmtabnode:%p %p force_output:%d\n",
         node,node->definition,node->alias,node->aux,node->no_reorder,
         node->decl->decl_with_vis.symtab_node,mtcsTarget->symtab,node->force_output);
   symtab=save;
   return node;
}

void  mtcs_var_add_mtcs_node(MtcsVar *self,MtcsVarNode *node)
{
    int len=self->varArray->len;
    int i;
    for(i=0;i<len;i++){
       MtcsVarNode *item=(MtcsVarNode *)n_ptr_array_index(self->varArray,i);
       if(item==node)
          return ;
    }
    n_ptr_array_add(self->varArray,node);
}

/* Add NODE to queue starting at FIRST.
   The queue is linked via AUX pointers and terminated by pointer to 1.  */
//原型 enqueue_node varpool.cc
static void enqueue_node (varpool_node *node, varpool_node **first)
{
   if (node->aux)
      return;
   gcc_checking_assert (*first);
   node->aux = *first;
   *first = node;
}


//if (DECL_EXTERNAL (decl))
//  return true;
//return (!force_output && !used_from_other_partition
//   && ((DECL_COMDAT (decl)
//        && !forced_by_abi
//        && !used_from_object_file_p ())
//       || !externally_visible
//       || DECL_HAS_VALUE_EXPR_P (decl)));
/* Optimization of function bodies might've rendered some variables as
   unnecessary so we want to avoid these from being compiled.  Re-do
   reachability starting from variables that are either externally visible
   or was referred from the asm output routines.  */
//原型 symbol_table::remove_unreferenced_decls cgraph.h varpool.cc
void mtcs_var_remove_unreferenced_decls (MtcsVar *self)
{
   varpool_node *next, *node;
   varpool_node *first = (varpool_node *)(void *)1;
   int i;
   ipa_ref *ref = NULL;
   hash_set<varpool_node *> referenced;

   if (seen_error ())
      return;

   if (dump_file)
      fprintf (dump_file, "Trivially needed variables:");
   FOR_EACH_DEFINED_VARIABLE (node){
      n_debug("mtcsvar.c mtcs_var_remove_unreferenced_decls 00aa node:%p analyzed:%d can:%d %p\n",
            node,node->analyzed,node->can_remove_if_no_refs_p (),DECL_RTL_SET_P (node->decl));
      n_debug("mtcsvar.c mtcs_var_remove_unreferenced_decls 00bb %d %d %d %d %d %d %d %d\n",DECL_EXTERNAL (node->decl),
            node->force_output,node->used_from_other_partition,DECL_COMDAT (node->decl),node->forced_by_abi,
            node->used_from_object_file_p (),node->externally_visible,DECL_HAS_VALUE_EXPR_P (node->decl));
      if (node->analyzed  && (!node->can_remove_if_no_refs_p ()
      /* We just expanded all function bodies.  See if any of
      them needed the variable.  */
      || DECL_RTL_SET_P (node->decl))){
         enqueue_node (node, &first);
         if (dump_file)
            fprintf (dump_file, " %s", node->dump_asm_name ());
      }
   }
   while (first != (varpool_node *)(void *)1){
      node = first;
      first = (varpool_node *)first->aux;
      n_debug("mtcsvar.c mtcs_var_remove_unreferenced_decls 11 node:%p definition:%d alias:%d aux:%d no_reorder:%d decl:%p %p\n",
             node,node->definition,node->alias,node->aux,node->no_reorder,
             node->decl->decl_with_vis.symtab_node,symtab);
      if (node->same_comdat_group){
         symtab_node *next;
         for (next = node->same_comdat_group; next != node; next = next->same_comdat_group){
            varpool_node *vnext = dyn_cast <varpool_node *> (next);
            if (vnext && vnext->analyzed && !next->comdat_local_p ())
               enqueue_node (vnext, &first);
         }
      }
      for (i = 0; node->iterate_reference (i, ref); i++){
         varpool_node *vnode = dyn_cast <varpool_node *> (ref->referred);
         if (vnode  && !vnode->in_other_partition  && (!DECL_EXTERNAL (ref->referred->decl) || vnode->alias) && vnode->analyzed)
            enqueue_node (vnode, &first);
         else{
            if (vnode)
               referenced.add (vnode);
            while (vnode && vnode->alias && vnode->definition){
               vnode = vnode->get_alias_target ();
               gcc_checking_assert (vnode);
               referenced.add (vnode);
            }
         }
      }
   }
   if (dump_file)
      fprintf (dump_file, "\nRemoving variables:");
   for (node = symtab->first_defined_variable (); node; node = next){
      next = symtab->next_defined_variable (node);
      n_debug("mtcsvar.c mtcs_var_remove_unreferenced_decls 66 node:%p next:%p\n",node,next);
      n_debug("mtcsvar.c mtcs_var_remove_unreferenced_decls 77 node:%p definition:%d alias:%d aux:%d no_reorder:%d decl:%p %p\n",
            node,node->definition,node->alias,node->aux,node->no_reorder,
            node->decl->decl_with_vis.symtab_node,symtab);
      if (!node->aux && !node->no_reorder){
         if (dump_file)
            fprintf (dump_file, " %s", node->dump_asm_name ());
         if (referenced.contains(node))
            node->remove_initializer ();
         else
            node->remove ();
      }
   }

   if (dump_file)
      fprintf (dump_file, "\n");
}

/* For variables in named sections make sure get_variable_section
   is called before we switch to those sections.  Then section
   conflicts between read-only and read-only requiring relocations
   sections can be resolved.  */
//原型 varpool_node::finalize_named_section_flags  cgraph.h varpool.cc
void mtcs_var_finalize_named_section_flags (MtcsVar *self, struct varpool_node *node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   tree decl = node->decl;
   bool alias = node->alias;
   bool in_other_partition = node->in_other_partition;

   if (!TREE_ASM_WRITTEN (decl)
   && !alias
   && !in_other_partition
   && !DECL_EXTERNAL (decl)
   && VAR_P (decl)
   && !DECL_HAS_VALUE_EXPR_P (decl)
   && node->get_section ())
      mtcs_asm_get_variable_section/*get_variable_section*/(mtcsAsm,decl, false);
}

/* When doing LTO, read variable's constructor from disk if
   it is not already present.  */
//原型 varpool_node::get_constructor cgraph.h varpool.cc
tree mtcs_var_get_constructor (MtcsVar *self,varpool_node *node)
{
  /*
  lto_file_decl_data *file_data;
  const char *data, *name;
  size_t len;
  */
  tree decl=node->decl;
  if (DECL_INITIAL (decl) != error_mark_node || !in_lto_p  || !node->lto_file_data)
    return DECL_INITIAL (decl);
  n_error("不应该进入到这里--get_constructor\n");

  /*
  timevar_push (TV_IPA_LTO_CTORS_IN);

  file_data = lto_file_data;
  name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));

  // We may have renamed the declaration, e.g., a static function.
  name = lto_get_decl_name_mapping (file_data, name);
  struct lto_in_decl_state *decl_state = lto_get_function_in_decl_state (file_data, decl);

  data = lto_get_section_data (file_data, LTO_section_function_body, name, order - file_data->order_base, &len, decl_state->compressed);
  if (!data)
    fatal_error (input_location, "%s: section %s.%d is missing",file_data->file_name,name, order - file_data->order_base);

  if (!quiet_flag)
    fprintf (stderr, " in:%s", IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)));
  lto_input_variable_constructor (file_data, this, data);
  gcc_assert (DECL_INITIAL (decl) != error_mark_node);
  lto_stats.num_function_bodies++;
  lto_free_section_data (file_data, LTO_section_function_body, name,data, len, decl_state->compressed);
  lto_free_function_in_decl_state_for_node (this);
  timevar_pop (TV_IPA_LTO_CTORS_IN);
  */
  return DECL_INITIAL (decl);
}

/* This function ensures that vtable_map variables are not only
   in the comdat section, but that each variable has its own unique
   comdat name.  Without this the variables end up in the same section
   with a single comdat name.  */

static void handle_vtv_comdat_section (MtcsVar *self,section *sect, const_tree decl ATTRIBUTE_UNUSED)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

    mtcs_asm_switch_to_comdat_section(mtcsAsm,sect, DECL_NAME (decl));
}

static void globalize_decl (MtcsVar *self,tree decl)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    target_asm_out_globalize_decl_name/*!targetm.asm_out.globalize_decl_name*/(mtcsMachine->asmOut, decl);
}

/* Assemble DECL given that it belongs in SECTION_NOSWITCH section SECT.
   NAME is the name of DECL's SYMBOL_REF.  */
static void assemble_noswitch_variable (MtcsVar *self,tree decl, const char *name, section *sect,unsigned int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);

   unsigned HOST_WIDE_INT size, rounded;
   size = tree_to_uhwi (DECL_SIZE_UNIT (decl));
   rounded = size;
   fprintf(stderr,"assemble_noswitch_variable 00\n");

   if ((flag_sanitize & SANITIZE_ADDRESS) && asan_protect_global (decl))
      size += asan_red_zone_size (size);

   fprintf(stderr,"assemble_noswitch_variable 11 size:%d\n",size);

   /* Don't allocate zero bytes of common,
   since that means "undefined external" in the linker.  */
   if (size == 0)
      rounded = 1;
   /* Round size up to multiple of BIGGEST_ALIGNMENT bits
   so that each uninitialized object starts on such a boundary.  */
   rounded += (mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign) / BITS_PER_UNIT) - 1;
   rounded = (rounded / (mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign) / BITS_PER_UNIT)*
         (mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign) / BITS_PER_UNIT));
   fprintf(stderr,"assemble_noswitch_variable 22 size:%d\n",size);

   if (!sect->noswitch.callback (decl, name, size, rounded)  && (unsigned HOST_WIDE_INT) (align / BITS_PER_UNIT) > rounded)
      error ("requested alignment for %q+D is greater than implemented alignment of %wu", decl, rounded);
   fprintf(stderr,"assemble_noswitch_variable 33 size:%d\n",size);

}

/* A subroutine of assemble_variable.  Output the label and contents of
   DECL, whose address is a SYMBOL_REF with name NAME.  DONT_OUTPUT_DATA
   is as for assemble_variable.  */

static void assemble_variable_contents (MtcsVar *self,tree decl, const char *name,bool dont_output_data, bool merge_strings)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   /* Do any machine/system dependent processing of the object.  */
   self->last_assemble_variable_decl = decl;
   n_debug("mtcsvar.c assemble_variable_contents 00 %s\n",name,dont_output_data,merge_strings);
   FILE *back=mtcsAsm->asmFile;
   mtcsAsm->asmFile=mtcsAsm->asmVarDeclFile;
   target_asm_out_declare_object_name/*!ASM_DECLARE_OBJECT_NAME*/(mtcsMachine->asmOut, name, decl);
   if (!dont_output_data){
      /* Caller is supposed to use varpool_get_constructor when it wants
      to output the body.  */
      gcc_assert (!in_lto_p || DECL_INITIAL (decl) != error_mark_node);
      n_debug("mtcsvar.c assemble_variable_contents 22 %s\n",name,dont_output_data,merge_strings);

      if (DECL_INITIAL (decl)  && DECL_INITIAL (decl) != error_mark_node && !initializer_zerop (DECL_INITIAL (decl))){
         /* Output the actual data.  */
         n_debug("mtcsvar.c assemble_variable_contents 33 %s\n",name,dont_output_data,merge_strings);
         mtcs_asm_output_constant (mtcsAsm,DECL_INITIAL (decl),tree_to_uhwi (DECL_SIZE_UNIT (decl)),
               mtcs_asm_get_variable_align(mtcsAsm,decl),false, merge_strings);
      }else
         /* Leave space for it.  */
         mtcs_asm_assemble_zeros (mtcsAsm,tree_to_uhwi (DECL_SIZE_UNIT (decl)));
      target_asm_out_decl_end/*!targetm.asm_out.decl_end */(mtcsMachine->asmOut);
   }
   mtcsAsm->asmFile=back;
}

/* Assemble everything that is needed for a variable or function declaration.
   Not used for automatic variables, and not used for function definitions.
   Should not be called for variables of incomplete structure type.

   TOP_LEVEL is nonzero if this variable has file scope.
   AT_END is nonzero if this is the special handling, at end of compilation,
   to define things that have had only tentative definitions.
   DONT_OUTPUT_DATA if nonzero means don't actually output the
   initial value (that will be done by the caller).  */
//原型 assemble_variable output.h varasm.cc
void mtcs_var_assemble_variable (MtcsVar *self,tree decl, int top_level ATTRIBUTE_UNUSED,
      int at_end ATTRIBUTE_UNUSED, int dont_output_data)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   const char *name;
   rtx decl_rtl, symbol;
   section *sect;
   unsigned int align;
   bool asan_protected = false;
   /* This function is supposed to handle VARIABLES.  Ensure we have one.  */
   gcc_assert (VAR_P (decl));
   /* Emulated TLS had better not get this far.  */
   gcc_checking_assert (mtcsTarget->have_tls || !DECL_THREAD_LOCAL_P (decl));
   self->last_assemble_variable_decl = 0;
   n_debug("mtcsvar.c assemble_variable 00 decl:%p align:%d\n",decl,DECL_ALIGN (decl));

   /* Normally no need to say anything here for external references,
   since assemble_external is called by the language-specific code
   when a declaration is first seen.  */
   if (DECL_EXTERNAL (decl))
      return;

   /* Do nothing for global register variables.  */
   if (DECL_RTL_SET_P (decl) && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl))){
      TREE_ASM_WRITTEN (decl) = 1;
      return;
   }
   n_debug("mtcsvar.c assemble_variable 11 DECL_SIZE (decl):%d\n",DECL_SIZE (decl));

   /* If type was incomplete when the variable was declared,
   see if it is complete now.  */
   if (DECL_SIZE (decl) == 0){
      n_debug("mtcsvar.c assemble_variable 11aa layout_decl\n");
      mtcs_stor_layout_layout_decl/*!layout_decl*/(mtcsStorLayout,decl, 0);
   }
   /* Still incomplete => don't allocate it; treat the tentative defn
   (which is what it must have been) as an `extern' reference.  */
   if (!dont_output_data && DECL_SIZE (decl) == 0){
      error ("storage size of %q+D isn%'t known", decl);
      TREE_ASM_WRITTEN (decl) = 1;
      return;
   }

   /* The first declaration of a variable that comes through this function
   decides whether it is global (in C, has external linkage)
   or local (in C, has internal linkage).  So do nothing more
   if this function has already run.  */

   if (TREE_ASM_WRITTEN (decl))
      return;
   /* Make sure targetm.encode_section_info is invoked before we set
   ASM_WRITTEN.  */
   //重要方法 生成变量的 rtl 并且调用 targetm.encode_section_info 会给变量设 shared constant global区域。

   decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl);
   n_debug("mtcsvar.c assemble_variable 22 decl_rtl:\n");
   mtcs_print_rtl(stderr,decl_rtl);
   TREE_ASM_WRITTEN (decl) = 1;

   /* Do no output if -fsyntax-only.  */
   if (flag_syntax_only)
      return;

   if (! dont_output_data  && ! valid_constant_size_p (DECL_SIZE_UNIT (decl))){
      error ("size of variable %q+D is too large", decl);
      return;
   }

   gcc_assert (MEM_P (decl_rtl));
   gcc_assert (GET_CODE (XEXP (decl_rtl, 0)) == SYMBOL_REF);
   symbol = XEXP (decl_rtl, 0);

   /* If this symbol belongs to the tree constant pool, output the constant
   if it hasn't already been written.  */
   if (TREE_CONSTANT_POOL_ADDRESS_P (symbol)){
      tree decl = SYMBOL_REF_DECL (symbol);
      if (!TREE_ASM_WRITTEN (DECL_INITIAL (decl)))
         mtcs_asm_output_constant_def_contents (mtcsAsm,symbol);
      return;
   }
   n_debug("mtcsvar.c assemble_variable 33 DECL_SIZE (decl):%d decl:%p ALIGN:%d\n",DECL_SIZE (decl),decl,DECL_ALIGN (decl));
   /*!app_disable*/
   if(mtcsAsm->appDisableCallback)
      mtcsAsm->appDisableCallback(mtcsAsm,mtcsAsm->userData);

   name = XSTR (symbol, 0);
   if (TREE_PUBLIC (decl) && DECL_NAME (decl)){
      n_debug("mtcsvar.c assemble_variable 33aa 调notice_global_symbol\n");
      mtcs_asm_notice_global_symbol (mtcsAsm,decl);
   }

   /* Compute the alignment of this data.  */
   mtcs_asm_align_variable (mtcsAsm,decl, dont_output_data);
   if ((flag_sanitize & SANITIZE_ADDRESS)  && asan_protect_global (decl)){
      n_debug("mtcsvar.c assemble_variable 33bb 调SET_DECL_ALIGN\n");
      asan_protected = true;
      n_error("重新设计，mtcs没有asan功能！");
      SET_DECL_ALIGN (decl, MAX (DECL_ALIGN (decl),ASAN_RED_ZONE_SIZE * BITS_PER_UNIT));
   }

   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,decl_rtl, DECL_ALIGN (decl));

   align = mtcs_asm_get_variable_align (mtcsAsm,decl);
   n_debug("mtcsvar.c assemble_variable 44 align:%d TREE_PUBLIC (decl):%p\n",align,TREE_PUBLIC (decl));
   if (TREE_PUBLIC (decl))
      mtcs_asm_maybe_assemble_visibility (mtcsAsm,decl);

   if (DECL_PRESERVE_P (decl)){
      n_debug("mtcsvar.c assemble_variable 44aa DECL_PRESERVE_P (decl):%p\n",DECL_PRESERVE_P (decl));
      target_asm_out_mark_decl_preserved/*!TARGET_ASM_MARK_DECL_PRESERVED*/(mtcsMachine->asmOut,name);
   }

   /* First make the assembler name(s) global if appropriate.  */
   //重要方法，获取变量对应的区域, lcomm_section 对应的回调是 emit_local 在 emit_local调用 mtcsTarget->output_aligned_decl_local
   sect = mtcs_asm_get_variable_section (mtcsAsm,decl, false);
   if (TREE_PUBLIC (decl) && (sect->common.flags & SECTION_COMMON) == 0){
      n_debug("mtcsvar.c assemble_variable 44bb  call globalize_decl (decl)\n");
      globalize_decl (self,decl);
   }

   /* Output any data that we will need to use the address of.  */
   if (DECL_INITIAL (decl) && DECL_INITIAL (decl) != error_mark_node){
      n_debug("mtcsvar.c assemble_variable 44cc  call output_addressed_constants (decl)\n");
      mtcs_asm_output_addressed_constants (mtcsAsm,DECL_INITIAL (decl), 0);
   }
   n_debug("mtcsvar.c assemble_variable 55 \n");

   /* dbxout.cc needs to know this.  */
   if (sect && (sect->common.flags & SECTION_CODE) != 0){
      n_debug("mtcsvar.c assemble_variable 55aa sect && (sect->common.flags & SECTION_CODE) != 0\n");
      DECL_IN_TEXT_SECTION (decl) = 1;
   }
   /* If the decl is part of an object_block, make sure that the decl
   has been positioned within its block, but do not write out its
   definition yet.  output_object_blocks will do that later.  */
   if (SYMBOL_REF_HAS_BLOCK_INFO_P (symbol) && SYMBOL_REF_BLOCK (symbol)){
      n_debug("mtcsvar.c assemble_variable 66 \n");
      gcc_assert (!dont_output_data);
      mtcs_asm_place_block_symbol (mtcsAsm,symbol);
   }else if (SECTION_STYLE (sect) == SECTION_NOSWITCH){
      n_debug("mtcsvar.c assemble_variable 77 name:%s\n",name);
      //调用 assemble_noswitch_variable 会写入汇编文件
      assemble_noswitch_variable(self,decl, name, sect, align);
   }else{
      /* Special-case handling of vtv comdat sections.  */
      n_debug("mtcsvar.c assemble_variable 88 \n");

      if (SECTION_STYLE (sect) == SECTION_NAMED  && (strcmp (sect->named.name, ".vtable_map_vars") == 0)){
         n_debug("mtcsvar.c assemble_variable 99 \n");
         handle_vtv_comdat_section (self,sect, decl);
      }else{
         n_debug("mtcsvar.c assemble_variable 100 \n");
         mtcs_asm_switch_to_section (mtcsAsm,sect, decl);
      }
      if (align > BITS_PER_UNIT){
         n_debug("mtcsvar.c assemble_variable 101 align > BITS_PER_UNIT %d %d \n",align,BITS_PER_UNIT);
         mtcsAsm->output_align(mtcsAsm, floor_log2 (align / BITS_PER_UNIT));
      }
      assemble_variable_contents (self,
            decl, name, dont_output_data,(sect->common.flags & SECTION_MERGE) && (sect->common.flags & SECTION_STRINGS));
      if (asan_protected){
         n_debug("mtcsvar.c assemble_variable 102 asan_protected\n");
         unsigned HOST_WIDE_INT int size = tree_to_uhwi (DECL_SIZE_UNIT (decl));
         mtcs_asm_assemble_zeros (mtcsAsm,asan_red_zone_size (size));
      }
   }
}

/**
 * 原型 ultimate_transparent_alias_target varasm.cc
 * mtcsasm mtcsvarasm 都定义有
 */
static inline tree ultimate_transparent_alias_target (tree *alias)
{
   tree target = *alias;
   if (IDENTIFIER_TRANSPARENT_ALIAS (target)){
      gcc_assert (TREE_CHAIN (target));
      target = ultimate_transparent_alias_target (&TREE_CHAIN (target));
      gcc_assert (! IDENTIFIER_TRANSPARENT_ALIAS (target) && ! TREE_CHAIN (target));
      *alias = target;
   }
   return target;
}

/* Output the assembler code for a define (equate) using ASM_OUTPUT_DEF
   or ASM_OUTPUT_DEF_FROM_DECLS.  The function defines the symbol whose
   tree node is DECL to have the value of the tree node TARGET.  */
//原型 do_assemble_alias output.h varasm.cc
void mtcs_var_do_assemble_alias (MtcsVar *self,tree decl, tree target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   tree id;
   /* Emulated TLS had better not get this var.  */
   gcc_assert (!(!mtcsTarget/*!targetm.have_tls*/->have_tls  && VAR_P (decl)   && DECL_THREAD_LOCAL_P (decl)));
   if (TREE_ASM_WRITTEN (decl))
      return;

   id = DECL_ASSEMBLER_NAME (decl);
   ultimate_transparent_alias_target (&id);
   ultimate_transparent_alias_target (&target);
   /* We must force creation of DECL_RTL for debug info generation, even though
   we don't use it here.  */
   n_debug("mtcsvar.c  mtcs_var_do_assemble_alias 00 准备 make_decl_rtl");
   if(DECL_NAME(decl))
      n_debug("mtcsvar.c  mtcs_var_do_assemble_alias11 准备 make_decl_rtl %s",IDENTIFIER_POINTER(DECL_NAME(decl)));

   mtcs_asm_make_decl_rtl/*!make_decl_rtl*/(mtcsAsm,decl);

   TREE_ASM_WRITTEN (decl) = 1;
   TREE_ASM_WRITTEN (DECL_ASSEMBLER_NAME (decl)) = 1;
   TREE_ASM_WRITTEN (id) = 1;

   if (lookup_attribute ("weakref", DECL_ATTRIBUTES (decl))){
      if (!TREE_SYMBOL_REFERENCED (target))
         mtcsAsm->weakref_targets = tree_cons (decl, target, mtcsAsm->weakref_targets);

      //#ifdef ASM_OUTPUT_WEAKREF
      //    ASM_OUTPUT_WEAKREF (asm_out_file, decl,IDENTIFIER_POINTER (id),IDENTIFIER_POINTER (target));
      //#else
      if(mtcsAsm->asm_output_weakref)
         mtcsAsm->asm_output_weakref(mtcsAsm,decl,IDENTIFIER_POINTER (id), IDENTIFIER_POINTER (target));
      else{
         if (!TARGET_SUPPORTS_WEAK){ //host=1 nvptx=1
            error_at (DECL_SOURCE_LOCATION (decl), "%qs is not supported in this configuration", "weakref ");
            return;
         }
      }
      //#endif
      return;
   }

   //#ifdef ASM_OUTPUT_DEF
   tree orig_decl = decl;

   /* Make name accessible from other files, if appropriate.  */

   if (TREE_PUBLIC (decl) || TREE_PUBLIC (orig_decl)){
      globalize_decl(self,decl);
      maybe_assemble_visibility (decl);
   }
   if (TREE_CODE (decl) == FUNCTION_DECL  && cgraph_node::get (decl)->ifunc_resolver){
      if(mtcsAsm->output_type_directive){
         if(mtcsTarget->has_ifunc_p(mtcsTarget))
            mtcsAsm->output_type_directive(mtcsAsm,IDENTIFIER_POINTER (id),IFUNC_ASM_TYPE);
         else
            error_at (DECL_SOURCE_LOCATION (decl), "%qs is not supported on this target", "ifunc");
      }else{
         error_at (DECL_SOURCE_LOCATION (decl), "%qs is not supported on this target", "ifunc");
      }
      /*!
      #if defined (ASM_OUTPUT_TYPE_DIRECTIVE)
      if (targetm.has_ifunc_p ())
      ASM_OUTPUT_TYPE_DIRECTIVE  (asm_out_file, IDENTIFIER_POINTER (id), IFUNC_ASM_TYPE);
      else
      #endif
      error_at (DECL_SOURCE_LOCATION (decl), "%qs is not supported on this target", "ifunc");
      */
   }
   if(mtcsAsm->asm_output_def_from_decls)
      //# ifdef ASM_OUTPUT_DEF_FROM_DECLS
      //ASM_OUTPUT_DEF_FROM_DECLS (asm_out_file, decl, target);
      mtcsAsm->asm_output_def_from_decls(mtcsAsm,decl,target);
      //# else
   else
      target_asm_out_output_def/*!ASM_OUTPUT_DEF*/(mtcsMachine->asmOut,IDENTIFIER_POINTER (id),IDENTIFIER_POINTER (target));
   //# endif
   /* If symbol aliases aren't actually supported...  */
   if (!TARGET_SUPPORTS_ALIASES
   //# ifdef ACCEL_COMPILER
   /* ..., and unless special-cased...  */
   && !lookup_attribute ("symbol alias handled", DECL_ATTRIBUTES (decl))
   //# endif
   )
      /* ..., 'ASM_OUTPUT_DEF{,_FROM_DECLS}' better have raised an error.  */
      gcc_checking_assert (seen_error ());
   //#elif defined (ASM_OUTPUT_WEAK_ALIAS) || defined (ASM_WEAKEN_DECL)
   //  {
   //    const char *name;
   //    tree *p, t;
   //
   //    name = IDENTIFIER_POINTER (id);
   //# ifdef ASM_WEAKEN_DECL
   //    ASM_WEAKEN_DECL (asm_out_file, decl, name, IDENTIFIER_POINTER (target));
   //# else
   //    ASM_OUTPUT_WEAK_ALIAS (asm_out_file, name, IDENTIFIER_POINTER (target));
   //# endif
   //    /* Remove this function from the pending weak list so that
   //       we do not emit multiple .weak directives for it.  */
   //    for (p = &weak_decls; (t = *p) ; )
   //      if (DECL_ASSEMBLER_NAME (decl) == DECL_ASSEMBLER_NAME (TREE_VALUE (t))
   //     || id == DECL_ASSEMBLER_NAME (TREE_VALUE (t)))
   //   *p = TREE_CHAIN (t);
   //      else
   //   p = &TREE_CHAIN (t);
   //
   //    /* Remove weakrefs to the same target from the pending weakref
   //       list, for the same reason.  */
   //    for (p = &weakref_targets; (t = *p) ; )
   //      {
   //   if (id == ultimate_transparent_alias_target (&TREE_VALUE (t)))
   //     *p = TREE_CHAIN (t);
   //   else
   //     p = &TREE_CHAIN (t);
   //      }
   //  }
   //#endif
}

/* Output .symver directive.  */
//原型 do_assemble_symver output.h varasm.cc
void mtcs_var_do_assemble_symver (MtcsVar *self,tree decl, tree target)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   tree id = DECL_ASSEMBLER_NAME (decl);
   ultimate_transparent_alias_target (&id);
   ultimate_transparent_alias_target (&target);
   //原型 ASM_OUTPUT_SYMVER_DIRECTIVE 平台实现
   if(mtcsAsm->asm_output_symver_directive)
      mtcsAsm->asm_output_symver_directive(mtcsAsm,IDENTIFIER_POINTER (target), IDENTIFIER_POINTER (id));
   /*!
   #ifdef ASM_OUTPUT_SYMVER_DIRECTIVE
   ASM_OUTPUT_SYMVER_DIRECTIVE (asm_out_file,
   IDENTIFIER_POINTER (target),
   IDENTIFIER_POINTER (id));
   */
   //#else
   else
      error ("symver is only supported on ELF platforms");
   //#endif
}

/* Assemble thunks and aliases associated to varpool node.  */
//原型 varpool_node::assemble_aliases cgraph.h varpool.cc
void mtcs_var_assemble_aliases (MtcsVar *self,varpool_node *node)
{
   ipa_ref *ref;

   FOR_EACH_ALIAS (node, ref){
      varpool_node *alias = dyn_cast <varpool_node *> (ref->referring);
      if (alias->symver)
         mtcs_var_do_assemble_symver/*!do_assemble_symver*/(self,alias->decl, DECL_ASSEMBLER_NAME (node->decl));
      else if (!alias->transparent_alias)
         mtcs_var_do_assemble_alias/*!do_assemble_alias*/(self,alias->decl, DECL_ASSEMBLER_NAME (node->decl));
      mtcs_var_assemble_aliases/*!alias->assemble_aliases*/(self,alias);
   }
}


/* Output one variable, if necessary.  Return whether we output it.  */
//原型 node->assemble_decl (); cgraph.h varpool.cc
bool mtcs_var_assemble_decl (MtcsVar *self,varpool_node *node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);

   /* Aliases are output when their target is produced or by
   output_weakrefs.  */
   tree decl=node->decl;
   bool in_other_partition=node->in_other_partition;
   if (node->alias)
      return false;

   /* Constant pool is output from RTL land when the reference
   survive till this level.  */
   if (DECL_IN_CONSTANT_POOL (decl) && TREE_ASM_WRITTEN (decl))
      return false;

   /* Decls with VALUE_EXPR should not be in the varpool at all.  They
   are not real variables, but just info for debugging and codegen.
   Unfortunately at the moment emutls is not updating varpool correctly
   after turning real vars into value_expr vars.  */
   if (DECL_HAS_VALUE_EXPR_P (decl)  && !mtcsTarget/*!targetm.have_tls*/->have_tls)
      return false;

   /* Hard register vars do not need to be output.  */
   if (DECL_HARD_REGISTER (decl))
      return false;

   gcc_checking_assert (!TREE_ASM_WRITTEN (decl) && VAR_P (decl) && !DECL_HAS_VALUE_EXPR_P (decl));

   if (!in_other_partition && !DECL_EXTERNAL (decl)){
      n_debug("mtcsvar.c mtcs_var_asm_assemble_decl 00 !in_other_partition && !DECL_EXTERNAL (decl)\n");
      tree decl=node->decl;;
      mtcs_var_get_constructor/*!get_constructor*/(self,node);
      mtcs_var_assemble_variable/*!assemble_variable*/(self,decl, 0, 1, 0);
      gcc_assert (TREE_ASM_WRITTEN (decl));
      gcc_assert (node->definition);
      n_debug("mtcsvar.c mtcs_var_asm_assemble_decl 11 !in_other_partition && !DECL_EXTERNAL (decl)\n");

      mtcs_var_assemble_aliases/*!assemble_aliases*/(self,node);
      /* After the parser has generated debugging information, augment
      this information with any new location/etc information that may
      have become available after the compilation proper.  */
      mtcs_debug_late_global_decl/*!debug_hooks->late_global_decl*/(mtcsDebug,decl);
      n_debug("mtcsvar.c mtcs_var_asm_assemble_decl 22 !in_other_partition && !DECL_EXTERNAL (decl)\n");

      return true;
   }
   return false;
}

/* Write out assembly for the variable DECL, which is not defined in
   the current translation unit.  */
//原型 assemble_undefined_decl output.h varasm.cc
void mtcs_var_assemble_undefined_decl (MtcsVar *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   const char *name = XSTR (XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl), 0), 0);
   fprintf(stderr,"mtcs_var_assemble_undefined_decl 00 %p %s\n",decl,name);
   target_asm_out_assemble_undefined_decl/*!targetm.asm_out.assemble_undefined_decl*/(mtcsMachine->asmOut, name, decl);
}

static nboolean isLocalSharedVar(MtcsVar *self,varpool_node *node)
{
   int len=self->varArray->len;
   int i;
   for(i=0;i<len;i++){
      MtcsVarNode *item=(MtcsVarNode *)n_ptr_array_index(self->varArray,i);
      if(item->node==node && item->promoteId>=0)
         return TRUE;
   }
   return FALSE;
}

/* Output all variables enqueued to be assembled.  */
//原型 symbol_table::output_variables cgraph.h varpool.cc
bool mtcs_var_output_variables (MtcsVar *self)
{
   bool changed = false;
   varpool_node *node;
   if (seen_error ())
      return false;
   mtcs_var_remove_unreferenced_decls/*!remove_unreferenced_decls*/(self);
   FOR_EACH_DEFINED_VARIABLE (node){
      /* Handled in output_in_order.  */
      n_debug("mtcsvar.c mtcs_var_output_variables 00 %s no_reorder:%d\n",IDENTIFIER_POINTER(DECL_NAME(node->decl)),node->no_reorder);
      if (node->no_reorder)
         continue;
      if(isLocalSharedVar(self,node))
         continue;
      n_debug("mtcsvar.c mtcs_var_output_variables 11 %s\n",IDENTIFIER_POINTER(DECL_NAME(node->decl)));
      mtcs_var_finalize_named_section_flags/*!node->finalize_named_section_flags*/(self,node);
   }

   /* There is a similar loop in output_in_order.  Please keep them in sync.  */
   FOR_EACH_VARIABLE (node){
      /* Handled in output_in_order.  */
      n_debug("mtcsvar.c mtcs_var_output_variables 22 %s\n",IDENTIFIER_POINTER(DECL_NAME(node->decl)));
      if (node->no_reorder)
         continue;
      if (DECL_HARD_REGISTER (node->decl) || DECL_HAS_VALUE_EXPR_P (node->decl))
         continue;
      if(isLocalSharedVar(self,node))
         continue;
      n_debug("mtcsvar.c mtcs_var_output_variables 33 %s definition:%d\n",IDENTIFIER_POINTER(DECL_NAME(node->decl)),node->definition);

      if (node->definition)
         changed |= mtcs_var_assemble_decl/*!node->assemble_decl*/(self,node);
      else
         mtcs_var_assemble_undefined_decl/*!assemble_undefined_decl*/(self,node->decl);
   }
   return changed;
}

/*************输出本地shared变量---------------------------*/

/**
 * 向 MtcsParser 查询 decl对应提升的id号
 */
static int getPromoteDeclId(tree decl)
{
   AetMediatorUser *mediatorUser =(AetMediatorUser *)mtcs_compile_get();
   AetMediator *mediator = mediatorUser->mediator;
   int  id = aet_mediator_get_promote_decl_id(mediator,decl,mediatorUser);
   return id;
}


static char *assembleSharedVariable (MtcsVar *self,tree decl,int dont_output_data)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   const char *name;
   rtx decl_rtl, symbol;
   section *sect;
   unsigned int align;
   bool asan_protected = false;
   /* This function is supposed to handle VARIABLES.  Ensure we have one.  */
   gcc_assert (VAR_P (decl));
   /* Emulated TLS had better not get this far.  */
   gcc_checking_assert (mtcsTarget->have_tls || !DECL_THREAD_LOCAL_P (decl));
   self->last_assemble_variable_decl = 0;
   n_debug("mtcsvar.c assemble_variable 00\n");

   /* Do nothing for global register variables.  */
   if (DECL_RTL_SET_P (decl) && REG_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl))){
      TREE_ASM_WRITTEN (decl) = 1;
      return NULL;
   }
   n_debug("mtcsvar.c assemble_variable 11 DECL_SIZE (decl):%d\n",DECL_SIZE (decl));

   /* The first declaration of a variable that comes through this function
   decides whether it is global (in C, has external linkage)
   or local (in C, has internal linkage).  So do nothing more
   if this function has already run.  */
   if (TREE_ASM_WRITTEN (decl))
      return NULL;
   /* Make sure targetm.encode_section_info is invoked before we set
   ASM_WRITTEN.  */
   //重要方法 生成变量的 rtl 并且调用 targetm.encode_section_info 会给变量设 shared constant global区域。
   decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl);
   n_debug("mtcsvar.c assemble_variable 22 decl_rtl:\n");
   mtcs_print_rtl(stderr,decl_rtl);
   TREE_ASM_WRITTEN (decl) = 1;
   gcc_assert (MEM_P (decl_rtl));
   gcc_assert (GET_CODE (XEXP (decl_rtl, 0)) == SYMBOL_REF);
   symbol = XEXP (decl_rtl, 0);
   n_debug("mtcsvar.c assemble_variable 33 DECL_SIZE (decl):%d\n",DECL_SIZE (decl));
   name = XSTR (symbol, 0);
   /* Compute the alignment of this data.  */
   mtcs_asm_align_variable (mtcsAsm,decl, dont_output_data);
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,decl_rtl, DECL_ALIGN (decl));
   align = mtcs_asm_get_variable_align (mtcsAsm,decl);
   n_debug("mtcsvar.c assemble_variable 44 align:%d TREE_PUBLIC (decl):%p\n",align,TREE_PUBLIC (decl));
   unsigned HOST_WIDE_INT size, rounded;
   size = tree_to_uhwi (DECL_SIZE_UNIT (decl));

   if ((flag_sanitize & SANITIZE_ADDRESS) && asan_protect_global (decl))
      size += asan_red_zone_size (size);
   unsigned int newAlign = symtab_node::get (decl)->definition_alignment ();
   //调用的原型是: mtcsTarget->output_aligned_decl_local(mtcsTarget, decl, name, size, newAlign);
   char *ret= mtcs_asm_output_promote_decl(mtcsAsm,decl,name,size,newAlign);
   //n_debug("mtcsvar.c assemble_variable 55 %s\n",ret);

   return ret;
}

//原型 node->assemble_decl (); cgraph.h varpool.cc
static char *assembleSharedNode (MtcsVar *self,varpool_node *node)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);

   /* Aliases are output when their target is produced or by
   output_weakrefs.  */
   tree decl=node->decl;
   bool in_other_partition=node->in_other_partition;
   if (node->alias)
      return NULL;

   /* Constant pool is output from RTL land when the reference
   survive till this level.  */
   if (DECL_IN_CONSTANT_POOL (decl) && TREE_ASM_WRITTEN (decl))
      return NULL;

   /* Decls with VALUE_EXPR should not be in the varpool at all.  They
   are not real variables, but just info for debugging and codegen.
   Unfortunately at the moment emutls is not updating varpool correctly
   after turning real vars into value_expr vars.  */
   if (DECL_HAS_VALUE_EXPR_P (decl)  && !mtcsTarget/*!targetm.have_tls*/->have_tls)
      return NULL;

   /* Hard register vars do not need to be output.  */
   if (DECL_HARD_REGISTER (decl))
      return NULL;

   gcc_checking_assert (!TREE_ASM_WRITTEN (decl) && VAR_P (decl) && !DECL_HAS_VALUE_EXPR_P (decl));

   if (!in_other_partition && !DECL_EXTERNAL (decl)){
      tree decl=node->decl;;
      mtcs_var_get_constructor/*!get_constructor*/(self,node);
      //关键 具体输出汇编
      char *ret =assembleSharedVariable/*!assemble_variable*/(self,decl, 0);
      gcc_assert (TREE_ASM_WRITTEN (decl));
      gcc_assert (node->definition);
      mtcs_debug_late_global_decl/*!debug_hooks->late_global_decl*/(mtcsDebug,decl);
      return ret;
   }
   return NULL;
}


static char * assembleSharedDecl(MtcsVar *self,varpool_node *promoteNode)
{
   varpool_node *node;
   FOR_EACH_VARIABLE (node){
      /* Handled in output_in_order.  */
      n_debug("mtcsvar.c mtcs_var_output_variables 22 %s\n",IDENTIFIER_POINTER(DECL_NAME(node->decl)));
      if (node->no_reorder)
         continue;
      if (DECL_HARD_REGISTER (node->decl) || DECL_HAS_VALUE_EXPR_P (node->decl))
         continue;
      n_debug("mtcsvar.c mtcs_var_output_variables 33 %s definition:%d\n",IDENTIFIER_POINTER(DECL_NAME(node->decl)),node->definition);
      if(node==promoteNode){
         if (!node->definition)
            n_error("mtcsvar.c 局部shared变量 definition应该为1 %s",IDENTIFIER_POINTER(DECL_NAME(node->decl)));
         return  assembleSharedNode/*!node->assemble_decl*/(self,node);
      }
   }
   return NULL;
}

/**
 * 在函数中的源代码
 * int __shared__ evtx;
 * 转为汇编代码 .shared .align 4 .u32 evtx.0[1];
 */
char  *mtcs_var_assemble_local_shared(MtcsVar *self,int promoteId)
{
   int len=self->varArray->len;
   n_debug("mtcs_var_assemble_local_shared 00 prmoteId:%d len:%d\n",promoteId,len);

   int i;
   for(i=0;i<len;i++){
      MtcsVarNode *item=(MtcsVarNode *)n_ptr_array_index(self->varArray,i);
      varpool_node *node= item->node;
      int id =getPromoteDeclId(item->hostDecl);
      n_debug("mtcs_var_assemble_local_shared 11 prmoteId:%d len:%d\n",promoteId,len);
      aet_print_tree(item->hostDecl);

      if(id==promoteId){
         n_debug("mtcs_var_assemble_local_shared 11 找到了 promoteId 对应的全局变量 name:%s decl:%p\n",node->name(),node->decl);
         item->promoteId = id;
         return assembleSharedDecl(self,node);
      }
   }
   return "shared 还未实现";
}


/**
 * 把变量名的中.换成下划线_
 */
static char *replaceDot (const char *name)
{
  if (strchr (name, '.') == NULL)
    return NULL;
  char *p = xstrdup (name);
  for (size_t i = 0; i < strlen (p); ++i)
    if (p[i] == '.')
      p[i] = '_';
  return p;
}

/**
 * 由于优化或其它原因由内部创建的变量，一般在pass创建，比如在 tree-switch-conversion.cc 中的 pass "switchconv"
 * 创建全局变量CSWTCH.4。
 */
char *mtcs_var_replace_dot(MtcsVar *self,char *name)
{
   if(!strstr(name,"."))
      return NULL;
   int len=self->varArray->len;
   int i;
   for(i=0;i<len;i++){
      MtcsVarNode *item=(MtcsVarNode *)n_ptr_array_index(self->varArray,i);
      if(item->innerCreate){
         varpool_node *node= item->node;
         if(strcmp(name,IDENTIFIER_POINTER(DECL_NAME(node->decl)))==0){
            return replaceDot(name);
         }
      }
   }
   return NULL;
}

MtcsVar *mtcs_var_new(MtcsMode *mtcsMode)
{
   MtcsVar *self = n_slice_alloc0 (sizeof(MtcsVar));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsVarInit(self);
   return self;
}
