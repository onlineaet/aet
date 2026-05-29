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

#include "mtcsptxasm.h"
#include "ptx-common.h"
#include "mtcsptx.h"
#include "ptxtool.h"
#include "mtcsptxfunc.h"
#include "mtcsptxoptions.h"

#define PTX_ASM_APP_ON "\t// #APP \n"
#define PTX_ASM_APP_OFF "\t// #NO_APP \n"

//原型 #define ASM_OUTPUT_ALIGN(FILE, POWER)  各平台自定义 nvptx.h  有定义,
//具体内容如下:
//#define ASM_OUTPUT_ALIGN(FILE, POWER)     \
//  do                 \
//    {                \
//      (void) (FILE);          \
//      (void) (POWER);            \
//    }                \
//  while (0)
static void outputAlign_cb(MtcsAsm *mtcsAsm,int power)
{

}

//原型 #define ASM_APP_ON "\t// #APP \n"
static void appOn_cb(MtcsAsm *mtcsAsm,int power)
{
   fputs (PTX_ASM_APP_ON, mtcsAsm->asmFile);
}

//原型 #define JUMP_TABLES_IN_TEXT_SECTION 0
static int  jumpTableInTextSection_cb (MtcsAsm *mtcsAsm)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsAsm);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   //原型 #define JUMP_TABLES_IN_TEXT_SECTION flag_pic
   return mtcsOptionsItem->x_flag_pic;
}

//原型 #define ASM_GENERATE_INTERNAL_LABEL 无缺省实现
static void generateInternalLabel_cb (MtcsAsm *self,char *buffer,char *prefix,int num)
{
    char *__p;
    __p = stpcpy (buffer+1, prefix);
    buffer[0] = '$';
    sprint_ul (__p, (unsigned long) (num));
}

//原型 #define ASM_DECLARE_FUNCTION_SIZE(STREAM, NAME, DECL)  nvptx_function_end (STREAM)
static  void declareFunctionSize_cb (MtcsAsm *mtcsAsm,const char *name, tree decl)
{
   n_debug("mtcsptxasm.c -----nvptx.cc -----34-- void nvptx_function_end (FILE *file)\n");
   fprintf (mtcsAsm->asmFile, "}\n");
}

/* Define locally, for use in NVPTX_ASM_OUTPUT_DEF.  */
#define SET_ASM_OP ".alias "

/* Define locally, for use in nvptx_asm_output_def_from_decls.  Add NVPTX_
   prefix to avoid clash with ASM_OUTPUT_DEF from nvptx.h.
   Copy of ASM_OUTPUT_DEF from defaults.h, with added terminating
   semicolon.  */
#define NVPTX_ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)   \
  do                    \
    {                   \
      fprintf ((FILE), "%s", SET_ASM_OP);    \
      mtcs_asm_assemble_name (mtcsAsm, LABEL1);       \
      fprintf (FILE, ",");          \
      mtcs_asm_assemble_name (mtcsAsm, LABEL2);       \
      fprintf (FILE, ";\n");           \
    }                   \
  while (0)

//原型  #define ASM_OUTPUT_DEF_FROM_DECLS(STREAM, NAME, VALUE)  nvptx_asm_output_def_from_decls (STREAM, NAME, VALUE)
static void asmOutputDefFromDecls_cb (MtcsAsm *mtcsAsm,tree name,tree value ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsAsm);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsPtxOptions *mtcsPtxOptions=(MtcsPtxOptions *)mtcsOptions;
   MtcsPtxFunc *mtcsPtxFunc=(MtcsPtxFunc *)mtcsFunc;
   n_debug("-----nvptx.cc -----89-- nvptx_asm_output_def_from_decls\n");

   if (mtcsPtxOptions->x_nvptx_alias == 0 || !TARGET_PTX_6_3) {
      /* Symbol aliases are not supported here.  */

      //#ifdef ACCEL_COMPILER
      if (DECL_CXX_CONSTRUCTOR_P (name)  || DECL_CXX_DESTRUCTOR_P (name)){
         /* ..., but symbol aliases are supported and used in the host system,
         via 'gcc/cp/optimize.cc:can_alias_cdtor'.  */

         gcc_assert (!lookup_attribute ("weak", DECL_ATTRIBUTES (name)));
         gcc_assert (TREE_CODE (name) == FUNCTION_DECL);

         /* In this specific case, use PTX '.alias', if available, even for
         (default) '-mno-alias'.  */
         if (TARGET_PTX_6_3){
            DECL_ATTRIBUTES (name) = tree_cons (get_identifier ("symbol alias handled"), NULL_TREE, DECL_ATTRIBUTES (name));
            goto emit_ptx_alias;
         }
      }
      //#endif

      /* Copied from assemble_alias.  */
      error_at (DECL_SOURCE_LOCATION (name), "alias definitions not supported in this configuration");
      TREE_ASM_WRITTEN (name) = 1;
      return;
   }

   if (lookup_attribute ("weak", DECL_ATTRIBUTES (name))){
      /* Prevent execution FAILs for gcc.dg/globalalias.c and
      gcc.dg/pr77587.c.  */
      error_at (DECL_SOURCE_LOCATION (name),"weak alias definitions not supported in this configuration");
      TREE_ASM_WRITTEN (name) = 1;
      return;
   }

   /* Ptx also doesn't support value having weak linkage, but we can't detect
   that here, so we'll end up with:
   "error: Function test with .weak scope cannot be aliased".
   See gcc.dg/localalias.c.  */

   if (TREE_CODE (name) != FUNCTION_DECL){
      error_at (DECL_SOURCE_LOCATION (name),"non-function alias definitions not supported in this configuration");
      TREE_ASM_WRITTEN (name) = 1;
      return;
   }

   //#ifdef ACCEL_COMPILER
emit_ptx_alias:
   //#endif

   cgraph_node *cnode = cgraph_node::get (name);
   //#ifdef ACCEL_COMPILER
   /* For nvptx offloading, make sure to emit C++ constructor, destructor aliases [PR97106]

   For some reason (yet to be analyzed), they're not 'cnode->referred_to_p ()'.
   (..., or that's not the right approach at all;
   <https://inbox.sourceware.org/87v7rx8lbx.fsf@euler.schwinge.ddns.net>
   "Re: [committed][nvptx] Use .alias directive for mptx >= 6.3").  */
   if (DECL_CXX_CONSTRUCTOR_P (name) || DECL_CXX_DESTRUCTOR_P (name))
      ;
   else
      //#endif
      if (!cnode->referred_to_p ())
         /* Prevent "Internal error: reference to deleted section".  */
         return;

   mtcs_ptx_func_write_fn_proto/*!write_fn_proto*/(mtcsPtxFunc,get_fnname_from_decl (name), name);

   tree id = DECL_ASSEMBLER_NAME (name);

   /* Walk alias chain to get reference callgraph node.
   The rationale of using ultimate_alias_target here is that
   PTX's .alias directive only supports 1-level aliasing where
   aliasee is function defined in same module.

   So for the following case:
   int foo() { return 42; }
   int bar () __attribute__((alias ("foo")));
   int baz () __attribute__((alias ("bar")));

   should resolve baz to foo:
   .visible .func (.param.u32 %value_out) baz;
   .alias baz,foo;  */
   symtab_node *alias_target_node = cnode->ultimate_alias_target ();
   tree alias_target_id = DECL_ASSEMBLER_NAME (alias_target_node->decl);
 //  NString *s_def=n_string_new("");//std::stringstream s_def;
  // mtcs_ptx_func_write_fn_marker/*!write_fn_marker*/(mtcsPtxFunc,s_def, true, TREE_PUBLIC (name), IDENTIFIER_POINTER (id));
   //fputs/*!fputs (s_def.str ().c_str (), stream)*/(str->str,mtcsAsm->asmFile);
  // n_string_free(str,TRUE);

   NVPTX_ASM_OUTPUT_DEF (mtcsAsm->asmFile, IDENTIFIER_POINTER (id), IDENTIFIER_POINTER (alias_target_id));
}

//把 nvptx_assemble_decl_begin mtcsptx.c 代码搬运这里
static char *outputPromoteDecl_cb (MtcsAsm *mtcsAsm,const_tree decl,const char *name, HOST_WIDE_INT size, unsigned align)
{
   MtcsPtxAsm *self=(MtcsPtxAsm *)mtcsAsm;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOutput *mtcsOutput = mtcs_target_get_output(mtcsTarget);

   char *section=".shared";
   tree type = TREE_TYPE (decl);
   bool atype = (TREE_CODE (type) == ARRAY_TYPE) && (TYPE_DOMAIN (type) == NULL_TREE);
   n_debug("mtcsasmptx.c 输出从局部到全局的变量使用的是 nvptx_assemble_decl_begin ----nvptx.cc -----90--  name:%s section:%s size:%d align:%d\n",
         name,section,size,align);

   while (TREE_CODE (type) == ARRAY_TYPE)
      type = TREE_TYPE (type);
   if (TREE_CODE (type) == VECTOR_TYPE || TREE_CODE (type) == COMPLEX_TYPE)
      /* Neither vector nor complex types can contain the other.  */
      type = TREE_TYPE (type);

   unsigned HOST_WIDE_INT elt_size = int_size_in_bytes (type);

   mtcs_mode elt_mode =TYPE_MODE (type) == mtcsMode->modes.M_BLKmode ?mtcs_mode_get_Pmode(mtcsMode) :mtcsMode->modes.M_DImode;
   const char *ptx_type = mtcs_mode_get_type/*!nvptx_ptx_type_from_mode*/ (mtcsMode,TYPE_MODE (type), true);

   elt_size |= mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,elt_mode);
   elt_size &= -elt_size; /* Extract LSB set.  */
   n_debug("mtcsptx.c-----nvptx.cc -----90bb-- name:%s size:%d elt_size:%d elt_mode:%d\n",name,size,elt_size,elt_mode);

   /* Size might not be a multiple of elt size, if there's an
   initialized trailing struct array with smaller type than
   elt_size. */
   int remaining = (size + elt_size - 1) / elt_size;
   NString *asmStr=n_string_new("");
   n_string_append_printf(asmStr, "%s .align %d .u" HOST_WIDE_INT_PRINT_UNSIGNED " ",section, align / BITS_PER_UNIT,elt_size * BITS_PER_UNIT);
   //n_string_append_printf(asmStr, "%s .align %d %s ",section, align / BITS_PER_UNIT,ptx_type);

   const char *resolveName = mtcs_asm_assemble_name_resolve(mtcsAsm,name);
   char asmName[255];
   if (resolveName[0] == '*')
      sprintf(asmName,"%s",&resolveName[1]);
   else
      sprintf(asmName,"%s%s",mtcsOutput->user_label_prefix,resolveName);
   // mtcs_asm_assemble_name (mtcsAsm, name);
   n_string_append(asmStr,asmName);

   if (size)
      /* We make everything an array, to simplify any initialization
      emission.  */
      n_string_append_printf(asmStr,  "[" HOST_WIDE_INT_PRINT_UNSIGNED "]", remaining);
   else if (atype)
      n_string_append_printf(asmStr,  "[]");
   n_string_append(asmStr,";");

   n_debug("mtcsptx.c-----nvptx.cc -----90xx-- name:%s size:%d elt_size:%d elt_mode:%d %s\n",
         name,size,elt_size,elt_mode,asmStr->str);
   return n_string_free(asmStr,FALSE);
}

//原型 #ifdef ASM_OUTPUT_DWARF_DELTA nvptx.h
static void asmOutputDwarfDelta_cb(MtcsAsm *mtcsAsm,int size, const char *lab1, const char *lab2)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsAsm);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   fprintf (mtcsAsm->asmFile, "%s[%d]: ", mtcsMachine->asmOut->byte_op/*!targetm.asm_out.byte_op*/, size);
   const char *label1 = lab1;
   mtcs_asm_assemble_name_raw/*!assemble_name_raw*/(mtcsAsm, label1 ? label1 : "*nil");
   fprintf (mtcsAsm->asmFile, " - ");
   const char *label2 = lab2;
   mtcs_asm_assemble_name_raw/*!assemble_name_raw*/(mtcsAsm, label2 ? label2 : "*nil");

}

static void mtcsPtxAsmInit(MtcsPtxAsm *self)
{
   MtcsAsm *mtcsAsm=(MtcsAsm *)self;
   //原型 #define ASM_OUTPUT_ALIGN(FILE, POWER)  各平台自定义 nvptx有定义
   mtcsAsm->output_align = outputAlign_cb;
   //原型 #define ASM_APP_ON "\t// #APP \n"
   mtcsAsm->app_on = appOn_cb;
   //原型 #define JUMP_TABLES_IN_TEXT_SECTION 0
   mtcsAsm->jump_tables_in_text_section = jumpTableInTextSection_cb;
   //原型 #define ASM_GENERATE_INTERNAL_LABEL 无缺省实现
   mtcsAsm->generate_internal_label = generateInternalLabel_cb;
   //原型 #define ASM_DECLARE_FUNCTION_SIZE(STREAM, NAME, DECL)  nvptx_function_end (STREAM)
   mtcsAsm->declare_function_size = declareFunctionSize_cb;
   //原型  #define ASM_OUTPUT_DEF_FROM_DECLS(STREAM, NAME, VALUE)  nvptx_asm_output_def_from_decls (STREAM, NAME, VALUE)
   mtcsAsm->asm_output_def_from_decls = asmOutputDefFromDecls_cb;
   //提升到全局的变量，输出汇编字符串
   mtcsAsm->output_promote_decl = outputPromoteDecl_cb;
   //原型 #ifdef ASM_OUTPUT_DWARF_DELTA
   mtcsAsm->asm_output_dwarf_delta = asmOutputDwarfDelta_cb;


   mtcsAsm->sectionAsmOp.text=n_strdup("");//原型 TEXT_SECTION_ASM_OP
   mtcsAsm->sectionAsmOp.data=n_strdup("");//原型 DATA_SECTION_ASM_OP
   mtcs_asm_set_asm_output_aligned_bss(mtcsAsm,FALSE);
   //原型 #define ASM_COMMENT_START "//"
   mtcs_asm_set_comment_start(mtcsAsm,PTX_ASM_COMMENT_START);

   self->funcDeclRegion=n_ptr_array_new();//保存所有函数声明的字符串和名字。
}

typedef struct _FuncDeclData{
   char *name;
   NString *decl;
   nboolean isExtern;
}FuncDeclData;

char *mtcs_ptx_asm_get_func_decl_asm(MtcsPtxAsm *self,char *name)
{
    int len=self->funcDeclRegion->len;
    int i;
    for(i=0;i<len;i++){
       FuncDeclData *item=n_ptr_array_index(self->funcDeclRegion,i);
       if(strcmp(item->name,name)==0)
          return item->decl->str;
    }
    return NULL;
}

void mtcs_ptx_asm_add_func_decl_asm(MtcsPtxAsm *self,char *name,NString *str,nboolean isExtern)
{
   FuncDeclData *data=n_slice_new(FuncDeclData);
   data->name=xstrdup(name);
   data->decl=str;
   data->isExtern=isExtern;
   n_ptr_array_add(self->funcDeclRegion,data);
}

char *mtcs_ptx_asm_get_all_fun_decl_asm(MtcsPtxAsm *self)
{
   int len=self->funcDeclRegion->len;
   int i;
   NString *externfn=n_string_new("");
   NString *declfn=n_string_new("");

   for(i=0;i<len;i++){
      FuncDeclData *item=n_ptr_array_index(self->funcDeclRegion,i);
      if(item->isExtern){
         n_string_append(externfn,item->decl->str);
         n_string_append(externfn,";\n");
      }else{
         n_string_append(declfn,item->decl->str);
         n_string_append(declfn,";\n");
      }
   }
   NString *result=n_string_new("");
   if(externfn->len>0){
      n_string_append(result,"//External function declarations referenced\n");
      n_string_append(result,externfn->str);
      n_string_append(result,"\n");
   }
   if(declfn->len>0){
      n_string_append(result,"//Function declaration\n");
      n_string_append(result,declfn->str);
      n_string_append(result,"\n");
   }
   if(result->len==0){
      n_string_free(result,TRUE);
      return NULL;
   }
   return   n_string_free(result,FALSE);
}


MtcsPtxAsm *mtcs_ptx_asm_new(MtcsMode *mtcsMode)
{
     MtcsPtxAsm *self = n_slice_alloc0 (sizeof(MtcsPtxAsm));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcs_asm_init((MtcsAsm *)self);
     mtcsPtxAsmInit(self);
     return self;
}
