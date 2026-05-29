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
 * base on varasm.cc
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
#include "tree-inline.h"

#include "mtcsasm.h"
#include "ptx/mtcsptx.h"
#include "mtcsfunc.h"
#include "aet/aetprinttree.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcstool.h"
#include "mtcsprintrtl.h"
#include "../aetmediator.h"

/* Return a nonzero value if DECL has a section attribute.  */
#define IN_NAMED_SECTION(DECL) \
  (VAR_OR_FUNCTION_DECL_P (DECL) && DECL_SECTION_NAME (DECL) != NULL)

/* The (assembler) name of the first globally-visible object output.  */
//extern GTY(()) const char *first_global_object_name; varasm.cc 已定义
//extern GTY(()) const char *weak_global_object_name;varasm.cc 已定义

struct rtx_constant_pool;
class addr_const;
class constant_descriptor_rtx;

#define n_deferred_constants (mtcsRtlData->varasm.deferred_constants)

static void      decode_addr_const(MtcsAsm *self,tree t, class addr_const *a);
static hashval_t const_hash_1 (MtcsAsm *self,const tree);
static bool      compare_constant(const tree, const tree);
static unsigned HOST_WIDE_INT output_constant (MtcsAsm *self,tree, unsigned HOST_WIDE_INT,unsigned int, bool, bool);
static section  *createNoSwitchSection (unsigned int flags, noswitch_section_callback callback);
static bool      emit_common (tree decl ATTRIBUTE_UNUSED,const char *name ATTRIBUTE_UNUSED,
                    unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED, unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED);
static bool      emit_local (tree decl ATTRIBUTE_UNUSED,  const char *name ATTRIBUTE_UNUSED,
                    unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED, unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED);
static bool      emit_tls_common (tree decl ATTRIBUTE_UNUSED, const char *name ATTRIBUTE_UNUSED,
                    unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED, unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED);
static void      outputInternalLabel_cb(MtcsAsm *self,const char *name);
static void      outputLabel_cb(MtcsAsm *self,const char *name);
//原型 #define ASM_OUTPUT_TYPE_DIRECTIVE(STREAM, NAME, TYPE)
static void      outputTypeDirective_cb(MtcsAsm *self,const char *name,const char *type);

/* Return a hash value for section SECT.  */
static hashval_t hash_section (section *sect)
{
   if (sect->common.flags & SECTION_NAMED)
      return htab_hash_string (sect->named.name);
   return sect->common.flags & ~SECTION_DECLARED;
}

class GTY((chain_next ("%h.next"), for_user)) constant_descriptor_rtx {
public:
  class constant_descriptor_rtx *next;
  rtx mem;
  rtx sym;
  rtx constant;
  HOST_WIDE_INT offset;
  hashval_t hash;
  fixed_size_mode mode;
  unsigned int align;
  int labelno;
  int mark;
};



/*-----------------hash功能-------------*/
//原型 hashval_t section_hasher::hash (section *old) varasm.cc
static nuint sectionHash_cb(nconstpointer v)
{
    section *old=(section*)v;
    return htab_hash_string (old->named.name);
}

//原型 bool section_hasher::equal (section *old, const char *new_name) varasm.cc
static nboolean sectionHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    section         *old=(section *)v1;
    const char *new_name=(const char *)v2;
    return strcmp (old->named.name, new_name) == 0;
}

//原型 hashval_t object_block_hasher::hash (object_block *old) varasm.cc
static nuint objectBlockHash_cb(nconstpointer v)
{
    object_block *old=(object_block *)v;
    return hash_section (old->sect);
}

//原型 inline bool object_block_hasher::equal (object_block *old, const section *new_section) varasm.cc
static nboolean objectBlockHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    object_block            *old=(object_block *)v1;
    const section *new_section  =(const section *)v2;
    return old->sect == new_section;
}

//原型 hashval_t tree_descriptor_hasher::hash (constant_descriptor_tree *ptr) varasm.cc
static nuint treeDescriptorHash_cb(nconstpointer v)
{
    constant_descriptor_tree *ptr=(constant_descriptor_tree *)v;
    return ptr->hash;
}


//原型 bool tree_descriptor_hasher::equal (constant_descriptor_tree *c1, constant_descriptor_tree *c2) varasm.cc
static nboolean treeDescriptorHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    constant_descriptor_tree *c1=(constant_descriptor_tree *)v1;
    constant_descriptor_tree *c2=(constant_descriptor_tree *)v2;
    if (c1->hash != c2->hash)
      return false;
    return compare_constant (c1->value, c2->value);
}

/* Hash and compare functions for const_rtx_htab.  */
//原型  hashval_t mtcs_const_rtx_desc_hasher::hash (constant_descriptor_rtx *desc)
static nuint constRtxDescHash_cb(nconstpointer v)
{
    constant_descriptor_rtx *desc=(constant_descriptor_rtx *)v;
    return desc->hash;
}

//原型 bool mtcs_const_rtx_desc_hasher::equal (constant_descriptor_rtx *x, constant_descriptor_rtx *y)
static nboolean constRtxDescHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
  constant_descriptor_rtx *x=(constant_descriptor_rtx *)v1;
  constant_descriptor_rtx *y=(constant_descriptor_rtx *)v2;
  if (x->mode != y->mode)
    return false;
  return rtx_equal_p (x->constant, y->constant);
}

/*---------------------结束hash功能-----------------------------*/

typedef struct _CreateUnamedSectionCallBackData{
    MtcsAsm *mtcsAsm;
    int flag;
    const char *sectionAsmOp;
}CreateUnamedSectionCallBackData;

static CreateUnamedSectionCallBackData *createUNnamdSection(MtcsAsm *self,int flag,const char *sectionAsmOp)
{
     CreateUnamedSectionCallBackData *data=(CreateUnamedSectionCallBackData *)xmalloc(sizeof(CreateUnamedSectionCallBackData));
     data->mtcsAsm=self;
     data->flag=flag;
     data->sectionAsmOp=sectionAsmOp;
     return data;
}

/* Return a new unnamed section with the given fields.  */
static section * createUnnamedSection (MtcsAsm *self,unsigned int flags, void (*callback) (const char *),const char *data)
{
  section *sect;
  sect = ggc_alloc<section> ();
  sect->unnamed.common.flags = flags | SECTION_UNNAMED;
  sect->unnamed.callback = callback;
  sect->unnamed.data = data;
  sect->unnamed.next = self->unnamed_sections;
  //fprintf(stderr,"createUnnamedSection --- sect:%p old unnamed_sections:%p flags:%d data:%p\n",sect,unnamed_sections,flags,data);
  self->unnamed_sections = sect;
  return sect;
}

static void createUnnamedSectionCallBack_cb (const char *directive)
{
    CreateUnamedSectionCallBackData *data=(CreateUnamedSectionCallBackData *)directive;
    //n_debug("mtcsasm.c createUnnamedSectionCallBack_cb %p\n",directive);
    MtcsAsm *self=data->mtcsAsm;
    fprintf (self->asmFile, "%s\n", data->sectionAsmOp);
    free(data);
}

/* Used in the hash tables to avoid outputting the same constant
   twice.  Unlike 'struct constant_descriptor_tree', RTX constants
   are output once per function, not once per file.  */
/* ??? Only a few targets need per-function constant pools.  Most
   can use one per-file pool.  Should add a targetm bit to tell the
   difference.  */
struct GTY(()) rtx_constant_pool {
  /* Pointers to first and last constant in pool, as ordered by offset.  */
  class constant_descriptor_rtx *first;
  class constant_descriptor_rtx *last;
  /* Hash facility for making memory-constants from constant rtl-expressions.
     It is used on RISC machines where immediate integer arguments and
     constant addresses are restricted so that such constants must be stored
     in memory.  */
  //原型 hash_table<mtcs_const_rtx_desc_hasher> *const_rtx_htab;
  NHashTable *const_rtx_htab;
  /* Current offset in constant pool (does not include any
     machine-specific header).  */
  HOST_WIDE_INT offset;
};


/* Create and return a new rtx constant pool.  */
//原型 create_constant_pool varasm.cc
static struct rtx_constant_pool *create_constant_pool (void)
{
  struct rtx_constant_pool *pool;
  pool = ggc_alloc<rtx_constant_pool> ();
  //原型 pool->const_rtx_htab = hash_table<mtcs_const_rtx_desc_hasher>::create_ggc (31);
  pool->const_rtx_htab = n_hash_table_new_full(constRtxDescHash_cb, constRtxDescHashEqual_cb,NULL, NULL);
  pool->first = NULL;
  pool->last = NULL;
  pool->offset = 0;
  return pool;
}


void     mtcs_asm_init(MtcsAsm *self)
{
      self->asmFileName=n_strdup_printf("%s_mtcs_asm.o",asm_file_name);
      self->asmFile=fopen(self->asmFileName,"w");
      self->asmVarDeclFileName= n_strdup_printf("%s_mtcs_var_decl_asm.o",asm_file_name);
      self->asmVarDeclFile=fopen(self->asmVarDeclFileName,"w");
      n_debug("mtcsasm.c mtcs_asm_init 00 asmFileName:%s %s asmFile:%p\n",self->asmFileName,asm_file_name,self->asmFile);

      //原型 ASM_OUTPUT_LABEL ptx没有自已的实现 用的是default.h的缺省实现
      self->output_label=outputLabel_cb;//来自宏ASM_OUTPUT_LABEL
      //来自宏ASM_OUTPUT_INTERNAL_LABEL ptx没有自已的实现 用的是default.h的缺省实现
      self->output_internal_label=outputInternalLabel_cb;
      //原型 ASM_OUTPUT_EXTERNAL (asm_out_file, decl, XSTR (XEXP (rtl, 0), 0));
      self->output_external =NULL;
      //原型 ASM_OUTPUT_REG_PUSH 各平台自定义 nvptx没定义
      self->output_reg_push = NULL;
      //原型 #define ASM_OUTPUT_REG_POP(STREAM, REGNO)  各平台自定义 nvptx没定义
      self->output_reg_pop = NULL;
      //原型 #define ASM_WEAKEN_LABEL(FILE,NAME)
      self->weaken_label = NULL;
      //原型 #define ASM_OUTPUT_TYPE_DIRECTIVE(STREAM, NAME, TYPE)
      self->output_type_directive = outputTypeDirective_cb;
      //原型  #define ASM_OUTPUT_ALIGN_WITH_NOP 各平台自定义 nvptx没定义
      self->output_align_with_nop = NULL;
      // 原型  #define  ASM_OUTPUT_MAX_SKIP_ALIGN 各平台自定义 nvptx没定义
      self->output_max_skip_align = NULL;
      //原型 #define ASM_OUTPUT_ALIGN(FILE, POWER)  各平台自定义 nvptx有定义
      self->output_align = NULL;
      //原型 #define ASM_OUTPUT_ADDR_VEC
      self->output_addr_vec = NULL;
      //原型 #define ASM_OUTPUT_ADDR_DIFF_VEC
      self->output_addr_diff_vec = NULL;
      //原型 #define ASM_OUTPUT_ADDR_VEC_ELT host=1 nvptx=0 gcn=1
      self->output_addr_vec_elt = NULL;
      //原型 #define ASM_OUTPUT_ADDR_DIFF_ELT host=1 nvptx=0
      self->output_addr_vec_diff_elt = NULL;
      //原型 #define ASM_OUTPUT_CASE_LABEL //host=1 nvptx=gcn=0
      self->output_case_label = NULL;
      //原型 #define ASM_DECLARE_FUNCTION_SIZE(STREAM, NAME, DECL)  nvptx_function_end (STREAM)
      self->declare_function_size = NULL;
      //原型 #ifdef ASM_DECLARE_COLD_FUNCTION_NAME
      self->declare_cold_function_name = NULL;
      //原型 #ifdef ASM_DECLARE_COLD_FUNCTION_SIZE
      self->declare_cold_function_size = NULL;

      self->sectionAsmOp.text=NULL;
      self->sectionAsmOp.data=NULL;
      self->sectionAsmOp.sdata=NULL;
      self->sectionAsmOp.readonly_data=NULL;
      self->sectionAsmOp.ctors=NULL;
      self->sectionAsmOp.dtors=NULL;
      self->sectionAsmOp.bss=NULL;
      self->sectionAsmOp.sbss=NULL;


      self->shared_constant_pool =NULL;
      self->text_section = NULL;
      self->data_section = NULL;
      self->readonly_data_section = NULL;
      self->sdata_section = NULL;
      self->ctors_section = NULL;
      self->ctors_section = NULL;
      self->ctors_section = NULL;
      self->sbss_section = NULL;
      self->tls_comm_section = NULL;
      self->lcomm_section = NULL;
      self->comm_section = NULL;
      self->bss_noswitch_section=NULL;
      //原型 ASM_OUTPUT_ALIGNED_BSS
      self->asmOutputAlignedBss=FALSE;

      self->funcHashTable = n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);

      //原型 static GTY(()) hash_table<section_hasher> *section_htab; varasm.cc
      self->section_htab = n_hash_table_new_full(sectionHash_cb, sectionHashEqual_cb,NULL, NULL);
      //原型 static GTY(()) hash_table<mtcs_object_block_hasher> *object_block_htab; varasm.cc
      self->object_block_htab = n_hash_table_new_full(objectBlockHash_cb, objectBlockHashEqual_cb,NULL, NULL);
      //原型 static GTY(()) hash_table<tree_descriptor_hasher> *const_desc_htab; varasm.cc
      self->const_desc_htab = n_hash_table_new_full(treeDescriptorHash_cb, treeDescriptorHashEqual_cb,NULL, NULL);

}

/* Return true if the current compilation mode benefits from having
   objects grouped into blocks.  */
static bool use_object_blocks_p (MtcsAsm *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  return mtcsOptionsItem->x_flag_section_anchors;
}

/* Return the object_block structure for section SECT.  Create a new
   structure if we haven't created one already.  Return null if SECT
   itself is null.  Return also null for mergeable sections since
   section anchors can't be used in mergeable sections anyway,
   because the linker might move objects around, and using the
   object blocks infrastructure in that case is both a waste and a
   maintenance burden.  */
static struct object_block *get_block_for_section (MtcsAsm *self,section *sect)
{
  struct object_block *block;
  if (sect == NULL)
    return NULL;
  if (sect->common.flags & SECTION_MERGE)
    return NULL;
  //原型 object_block **slot= object_block_htab->find_slot_with_hash (sect, hash_section (sect),INSERT);
  object_block *slot = n_hash_table_lookup_by_hash(self->object_block_htab,sect,hash_section (sect));
 /*
  block = *slot;
  if (block == NULL){
      block = ggc_cleared_alloc<object_block> ();
      block->sect = sect;
      *slot = block;
  }
  */
  block=slot;
  if (block == NULL){
      block = ggc_cleared_alloc<object_block> ();
      block->sect = sect;
      n_hash_table_insert(self->object_block_htab,block,block);
      slot = block;
  }
  return block;
}

/* Create a symbol with label LABEL and place it at byte offset
   OFFSET in BLOCK.  OFFSET can be negative if the symbol's offset
   is not yet known.  LABEL must be a garbage-collected string.  */
static rtx create_block_symbol (MtcsAsm *self,const char *label, struct object_block *block,HOST_WIDE_INT offset)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);//self->mtcsMode;
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
  mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,symbol, Pmode);
  XSTR (symbol, 0) = label;
  SYMBOL_REF_FLAGS (symbol) = SYMBOL_FLAG_HAS_BLOCK_INFO;
  /* Initialize the block_symbol stuff.  */
  SYMBOL_REF_BLOCK (symbol) = block;
  SYMBOL_REF_BLOCK_OFFSET (symbol) = offset;
  return symbol;
}

/* Follow the IDENTIFIER_TRANSPARENT_ALIAS chain starting at *ALIAS
   until we find an identifier that is not itself a transparent alias.
   Modify the alias passed to it by reference (and all aliases on the
   way to the ultimate target), such that they do not have to be
   followed again, and return the ultimate target of the alias
   chain.  */
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

/* Look up EXP in the table of constant descriptors.  Return the rtl
   if it has been emitted, else null.  */
//原型 rtl.h

static rtx mtcs_lookup_constant_def(MtcsAsm *self,tree exp)
{
  struct constant_descriptor_tree key;
  key.value = exp;
  key.hash = const_hash_1(self,exp);
  //原型 constant_descriptor_tree *desc= const_desc_htab->find_with_hash (&key, key.hash);
  constant_descriptor_tree *desc = n_hash_table_lookup_by_hash(self->const_desc_htab,&key,key.hash);
  return (desc ? desc->rtl : NULL_RTX);
}

/* CONSTANT_POOL_BEFORE_FUNCTION may be defined as an expression with
   a nonzero value if the constant pool should be output before the
   start of the function, or a zero value if the pool should output
   after the end of the function.  The default is to put it before the
   start.  */

#ifndef CONSTANT_POOL_BEFORE_FUNCTION
#define CONSTANT_POOL_BEFORE_FUNCTION 1
#endif

/* Set the symbol_referenced flag for ID.  */
void mtcs_mark_referenced (tree id)
{
   TREE_SYMBOL_REFERENCED (id) = 1;
}

/* A and B are either alignments or offsets.  Return the minimum alignment
   that may be assumed after adding the two together.  */
static inline unsigned min_align (unsigned int a, unsigned int b)
{
   return least_bit_hwi (a | b);
}


/* Given an expression EXP with a constant value,
   reduce it to the sum of an assembler symbol and an integer.
   Store them both in the structure *VALUE.
   EXP must be reducible.  */

class addr_const {
public:
  rtx base;
  poly_int64 offset;
};

static void decode_addr_const (MtcsAsm *self,tree exp, class addr_const *value)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget =(MtcsTarget *)mtcsMode->target;
   MtcsStmt *mtcsStmt=mtcs_target_get_stmt(mtcsTarget);

  tree target = TREE_OPERAND (exp, 0);
  poly_int64 offset = 0;
  rtx x;

  while (1){
      poly_int64 bytepos;
      if (TREE_CODE (target) == COMPONENT_REF && poly_int_tree_p (byte_position (TREE_OPERAND (target, 1)), &bytepos)) {
          offset += bytepos;
          target = TREE_OPERAND (target, 0);
      }else if (TREE_CODE (target) == ARRAY_REF || TREE_CODE (target) == ARRAY_RANGE_REF){
          /* Truncate big offset.  */
          offset += (TREE_INT_CST_LOW (TYPE_SIZE_UNIT (TREE_TYPE (target))) *
                wi::to_poly_widest (TREE_OPERAND (target, 1)).force_shwi ());
          target = TREE_OPERAND (target, 0);
      }else if (TREE_CODE (target) == MEM_REF && TREE_CODE (TREE_OPERAND (target, 0)) == ADDR_EXPR){
          offset += mem_ref_offset (target).force_shwi ();
          target = TREE_OPERAND (TREE_OPERAND (target, 0), 0);
      }else if (INDIRECT_REF_P (target) && TREE_CODE (TREE_OPERAND (target, 0)) == NOP_EXPR
           && TREE_CODE (TREE_OPERAND (TREE_OPERAND (target, 0), 0))== ADDR_EXPR)
          target = TREE_OPERAND (TREE_OPERAND (TREE_OPERAND (target, 0), 0), 0);
      else
          break;
  }

  switch (TREE_CODE (target)){
    case VAR_DECL:
    case FUNCTION_DECL:
      x = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,target);
      break;

    case LABEL_DECL:
      x = gen_rtx_MEM (FUNCTION_MODE, gen_rtx_LABEL_REF (mtcs_mode_get_Pmode(mtcsMode),
            mtcs_stmt_force_label_rtx/*!force_label_rtx*/(mtcsStmt,target)));
      break;

    case REAL_CST:
    case FIXED_CST:
    case STRING_CST:
    case COMPLEX_CST:
    case CONSTRUCTOR:
    case INTEGER_CST:
      x = mtcs_lookup_constant_def(self,target);
      /* Should have been added by output_addressed_constants.  */
      gcc_assert (x);
      break;

    case INDIRECT_REF:
      /* This deals with absolute addresses.  */
      offset += tree_to_shwi (TREE_OPERAND (target, 0));
      x = gen_rtx_MEM (QImode,gen_rtx_SYMBOL_REF (Pmode, "origin of addresses"));
      break;

    case COMPOUND_LITERAL_EXPR:
      gcc_assert (COMPOUND_LITERAL_EXPR_DECL (target));
      x = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,COMPOUND_LITERAL_EXPR_DECL (target));
      break;

    default:
      gcc_unreachable ();
 }

  gcc_assert (MEM_P (x));
  x = XEXP (x, 0);

  value->base = x;
  value->offset = offset;
}

static hashval_t const_hash_1(MtcsAsm *self,const tree exp)
{
  const char *p;
  hashval_t hi;
  int len, i;
  enum tree_code code = TREE_CODE (exp);

  /* Either set P and LEN to the address and len of something to hash and
     exit the switch or return a value.  */

  switch (code){
    case INTEGER_CST:
      p = (char *) &TREE_INT_CST_ELT (exp, 0);
      len = TREE_INT_CST_NUNITS (exp) * sizeof (HOST_WIDE_INT);
      break;

    case REAL_CST:
      return real_hash (TREE_REAL_CST_PTR (exp));

    case FIXED_CST:
      return fixed_hash (TREE_FIXED_CST_PTR (exp));

    case STRING_CST:
      p = TREE_STRING_POINTER (exp);
      len = TREE_STRING_LENGTH (exp);
      break;

    case COMPLEX_CST:
      return (const_hash_1(self,TREE_REALPART (exp)) * 5
          + const_hash_1(self,TREE_IMAGPART (exp)));

    case VECTOR_CST:
      {
        hi = 7 + VECTOR_CST_NPATTERNS (exp);
        hi = hi * 563 + VECTOR_CST_NELTS_PER_PATTERN (exp);
        unsigned int count = vector_cst_encoded_nelts (exp);
        for (unsigned int i = 0; i < count; ++i)
          hi = hi * 563 + const_hash_1(self,VECTOR_CST_ENCODED_ELT (exp, i));
        return hi;
      }

    case CONSTRUCTOR:
      {
        unsigned HOST_WIDE_INT idx;
        tree value;

        hi = 5 + int_size_in_bytes (TREE_TYPE (exp));

        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (exp), idx, value)
          if (value)
            hi = hi * 603 + const_hash_1(self,value);

        return hi;
      }

    case ADDR_EXPR:
      if (CONSTANT_CLASS_P (TREE_OPERAND (exp, 0)))
       return const_hash_1(self,TREE_OPERAND (exp, 0));

      /* Fallthru.  */
    case FDESC_EXPR:
      {
        class addr_const value;

        decode_addr_const(self,exp, &value);
        switch (GET_CODE (value.base)){
          case SYMBOL_REF:
            /* Don't hash the address of the SYMBOL_REF;
               only use the offset and the symbol name.  */
            hi = value.offset.coeffs[0];
            p = XSTR (value.base, 0);
            for (i = 0; p[i] != 0; i++)
              hi = ((hi * 613) + (unsigned) (p[i]));
            break;

          case LABEL_REF:
            hi = (value.offset.coeffs[0]
              + CODE_LABEL_NUMBER (label_ref_label (value.base)) * 13);
            break;

          default:
            gcc_unreachable ();
        }
      }
      return hi;

    case PLUS_EXPR:
    case POINTER_PLUS_EXPR:
    case MINUS_EXPR:
      return (const_hash_1(self,TREE_OPERAND (exp, 0)) * 9 + const_hash_1(self,TREE_OPERAND (exp, 1)));

    CASE_CONVERT:
      return const_hash_1(self,TREE_OPERAND (exp, 0)) * 7 + 2;

    default:
      /* A language specific constant. Just hash the code.  */
      return code;
    }

  /* Compute hashing function.  */
  hi = len;
  for (i = 0; i < len; i++)
    hi = ((hi * 613) + (unsigned) (p[i]));

  return hi;
}

/* Compare t1 and t2, and return true only if they are known to result in
   the same bit pattern on output.  */

static bool compare_constant (const tree t1, const tree t2)
{
  MtcsCompile *mtcsCompile=mtcs_compile_get();
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcsCompile);
  MtcsAsm *self=mtcs_target_get_asm(mtcsTarget);
  enum tree_code typecode;
  if (t1 == NULL_TREE)
    return t2 == NULL_TREE;
  if (t2 == NULL_TREE)
    return false;
  if (TREE_CODE (t1) != TREE_CODE (t2))
    return false;
  switch (TREE_CODE (t1)){
    case INTEGER_CST:
      /* Integer constants are the same only if the same width of type.  */
      if (TYPE_PRECISION (TREE_TYPE (t1)) != TYPE_PRECISION (TREE_TYPE (t2)))
          return false;
      if (TYPE_MODE (TREE_TYPE (t1)) != TYPE_MODE (TREE_TYPE (t2)))
          return false;
      return tree_int_cst_equal (t1, t2);
    case REAL_CST:
      /* Real constants are the same only if the same width of type.  In
     addition to the same width, we need to check whether the modes are the
     same.  There might be two floating point modes that are the same size
     but have different representations, such as the PowerPC that has 2
     different 128-bit floating point types (IBM extended double and IEEE
     128-bit floating point).  */
      if (TYPE_PRECISION (TREE_TYPE (t1)) != TYPE_PRECISION (TREE_TYPE (t2)))
          return false;
      if (TYPE_MODE (TREE_TYPE (t1)) != TYPE_MODE (TREE_TYPE (t2)))
          return false;
      return real_identical (&TREE_REAL_CST (t1), &TREE_REAL_CST (t2));
    case FIXED_CST:
      /* Fixed constants are the same only if the same width of type.  */
      if (TYPE_PRECISION (TREE_TYPE (t1)) != TYPE_PRECISION (TREE_TYPE (t2)))
          return false;
      return FIXED_VALUES_IDENTICAL (TREE_FIXED_CST (t1), TREE_FIXED_CST (t2));
    case STRING_CST:
      if (TYPE_MODE (TREE_TYPE (t1)) != TYPE_MODE (TREE_TYPE (t2)) || int_size_in_bytes (TREE_TYPE (t1))!= int_size_in_bytes (TREE_TYPE (t2)))
          return false;
      return (TREE_STRING_LENGTH (t1) == TREE_STRING_LENGTH (t2)
          && ! memcmp (TREE_STRING_POINTER (t1), TREE_STRING_POINTER (t2),TREE_STRING_LENGTH (t1)));
    case COMPLEX_CST:
      return (compare_constant (TREE_REALPART (t1), TREE_REALPART (t2)) && compare_constant (TREE_IMAGPART (t1), TREE_IMAGPART (t2)));
    case VECTOR_CST:
      {
        if (VECTOR_CST_NPATTERNS (t1) != VECTOR_CST_NPATTERNS (t2))
          return false;
        if (VECTOR_CST_NELTS_PER_PATTERN (t1)!= VECTOR_CST_NELTS_PER_PATTERN (t2))
          return false;
        unsigned int count = vector_cst_encoded_nelts (t1);
        for (unsigned int i = 0; i < count; ++i)
          if (!compare_constant (VECTOR_CST_ENCODED_ELT (t1, i),VECTOR_CST_ENCODED_ELT (t2, i)))
            return false;
        return true;
      }
    case CONSTRUCTOR:
      {
        vec<constructor_elt, va_gc> *v1, *v2;
        unsigned HOST_WIDE_INT idx;
        typecode = TREE_CODE (TREE_TYPE (t1));
        if (typecode != TREE_CODE (TREE_TYPE (t2)))
          return false;
        if (typecode == ARRAY_TYPE){
            HOST_WIDE_INT size_1 = int_size_in_bytes (TREE_TYPE (t1));
            /* For arrays, check that mode, size and storage order match.  */
            if (TYPE_MODE (TREE_TYPE (t1)) != TYPE_MODE (TREE_TYPE (t2))
            || size_1 == -1 || size_1 != int_size_in_bytes (TREE_TYPE (t2))
            || TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (t1))!= TYPE_REVERSE_STORAGE_ORDER (TREE_TYPE (t2)))
              return false;
        }else{
            /* For record and union constructors, require exact type
                   equality.  */
            if (TREE_TYPE (t1) != TREE_TYPE (t2))
              return false;
        }
        v1 = CONSTRUCTOR_ELTS (t1);
        v2 = CONSTRUCTOR_ELTS (t2);
        if (vec_safe_length (v1) != vec_safe_length (v2))
          return false;

        for (idx = 0; idx < vec_safe_length (v1); ++idx){
            constructor_elt *c1 = &(*v1)[idx];
            constructor_elt *c2 = &(*v2)[idx];
            /* Check that each value is the same...  */
            if (!compare_constant (c1->value, c2->value))
              return false;
            /* ... and that they apply to the same fields!  */
            if (typecode == ARRAY_TYPE){
                if (!compare_constant (c1->index, c2->index))
                  return false;
            }else{
                if (c1->index != c2->index)
                  return false;
            }
        }
        return true;
      }
    case ADDR_EXPR:
    case FDESC_EXPR:
      {
        class addr_const value1, value2;
        enum rtx_code code;
        bool ret;
        decode_addr_const(self,t1, &value1);
        decode_addr_const(self,t2, &value2);
        if (maybe_ne (value1.offset, value2.offset))
          return false;
        code = GET_CODE (value1.base);
        if (code != GET_CODE (value2.base))
          return false;
        switch (code){
          case SYMBOL_REF:
            ret = (strcmp (XSTR (value1.base, 0), XSTR (value2.base, 0)) == 0);
            break;
          case LABEL_REF:
            ret = (CODE_LABEL_NUMBER (label_ref_label (value1.base))== CODE_LABEL_NUMBER (label_ref_label (value2.base)));
            break;
          default:
            gcc_unreachable ();
        }
        return ret;
      }
    case PLUS_EXPR:
    case POINTER_PLUS_EXPR:
    case MINUS_EXPR:
    case RANGE_EXPR:
      return (compare_constant(TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0)) && compare_constant (TREE_OPERAND (t1, 1), TREE_OPERAND (t2, 1)));

    CASE_CONVERT:
    case VIEW_CONVERT_EXPR:
      return compare_constant(TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));

    default:
      return false;
  }
}



/* Recompute the offsets of entries in POOL, and the overall size of
   POOL.  Do this after calling mark_constant_pool to ensure that we
   are computing the offset values for the pool which we will actually
   emit.  */

static void recompute_pool_offsets (MtcsAsm *self,struct rtx_constant_pool *pool)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  class constant_descriptor_rtx *desc;
  pool->offset = 0;
  for (desc = pool->first; desc ; desc = desc->next)
    if (desc->mark){
          /* Recalculate offset.  */
        unsigned int align = desc->align;
        pool->offset += (align / BITS_PER_UNIT) - 1;
        pool->offset &= ~ ((align / BITS_PER_UNIT) - 1);
        desc->offset = pool->offset;
        pool->offset += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,desc->mode);
    }
}

struct constant_descriptor_rtx_data {
  constant_descriptor_rtx *desc;
  target_unit *bytes;
  unsigned short size;
  unsigned short offset;
  unsigned int hash;
};

struct mtcs_const_rtx_data_hasher : nofree_ptr_hash<constant_descriptor_rtx_data>
{
  static hashval_t hash (constant_descriptor_rtx_data *);
  static bool equal (constant_descriptor_rtx_data *,constant_descriptor_rtx_data *);
};

/* Hash and compare functions for const_rtx_data_htab.  */
hashval_t mtcs_const_rtx_data_hasher::hash (constant_descriptor_rtx_data *data)
{
  return data->hash;
}

bool mtcs_const_rtx_data_hasher::equal (constant_descriptor_rtx_data *x,constant_descriptor_rtx_data *y)
{
  if (x->hash != y->hash || x->size != y->size)
    return false;
  unsigned int align1 = x->desc->align;
  unsigned int align2 = y->desc->align;
  unsigned int offset1 = (x->offset * BITS_PER_UNIT) & (align1 - 1);
  unsigned int offset2 = (y->offset * BITS_PER_UNIT) & (align2 - 1);
  if (offset1)
    align1 = least_bit_hwi (offset1);
  if (offset2)
    align2 = least_bit_hwi (offset2);
  if (align2 > align1)
    return false;
  if (memcmp (x->bytes, y->bytes, x->size * sizeof (target_unit)) != 0)
    return false;
  return true;
}

/* Whether a constructor CTOR is a valid static constant initializer if all
   its elements are.  This used to be internal to mtcs_initializer_constant_valid_p
   and has been exposed to let other functions like categorize_ctor_elements
   evaluate the property while walking a constructor for other purposes.  */

bool mtcs_constructor_static_from_elts_p(const_tree ctor)
{
  return (TREE_CONSTANT (ctor)  && (TREE_CODE (TREE_TYPE (ctor)) == UNION_TYPE
          || TREE_CODE (TREE_TYPE (ctor)) == RECORD_TYPE  || TREE_CODE (TREE_TYPE (ctor)) == ARRAY_TYPE));
}



/* Check if a STRING_CST fits into the field.
   Tolerate only the case when the NUL termination
   does not fit into the field.   */
static bool check_string_literal (tree string, unsigned HOST_WIDE_INT size)
{
  tree type = TREE_TYPE (string);
  tree eltype = TREE_TYPE (type);
  unsigned HOST_WIDE_INT elts = tree_to_uhwi (TYPE_SIZE_UNIT (eltype));
  unsigned HOST_WIDE_INT mem_size = tree_to_uhwi (TYPE_SIZE_UNIT (type));
  int len = TREE_STRING_LENGTH (string);

  if (elts != 1 && elts != 2 && elts != 4)
    return false;
  if (len < 0 || len % elts != 0)
    return false;
  if (size < (unsigned)len)
    return false;
  if (mem_size != size)
    return false;
  return true;
}

/* output_constructor outer state of relevance in recursive calls, typically
   for nested aggregate bitfields.  */

struct oc_outer_state {
  unsigned int bit_offset;  /* current position in ...  */
  int byte;                 /* ... the outer byte buffer.  */
};

static unsigned HOST_WIDE_INT output_constructor (MtcsAsm *self,tree, unsigned HOST_WIDE_INT, unsigned int, bool, oc_outer_state *);

/* Output assembler code for constant EXP, with no label.
   This includes the pseudo-op such as ".int" or ".byte", and a newline.
   Assumes output_addressed_constants has been done on EXP already.

   Generate at least SIZE bytes of assembler data, padding at the end
   with zeros if necessary.  SIZE must always be specified.  The returned
   value is the actual number of bytes of assembler data generated, which
   may be bigger than SIZE if the object contains a variable length field.

   SIZE is important for structure constructors,
   since trailing members may have been omitted from the constructor.
   It is also important for initialization of arrays from string constants
   since the full length of the string constant might not be wanted.
   It is also needed for initialization of unions, where the initializer's
   type is just one member, and that may not be as long as the union.

   There a case in which we would fail to output exactly SIZE bytes:
   for a structure constructor that wants to produce more than SIZE bytes.
   But such constructors will never be generated for any possible input.

   ALIGN is the alignment of the data in bits.

   If REVERSE is true, EXP is output in reverse storage order.  */
//原型 output_constant
static unsigned HOST_WIDE_INT output_constant (MtcsAsm *self,tree exp,
      unsigned HOST_WIDE_INT size, unsigned int align,bool reverse, bool merge_strings)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);
  MtcsMachine  *mtcsMachine =mtcs_target_get_machine(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  enum tree_code code;
  unsigned HOST_WIDE_INT thissize;
  rtx cst;
  machine_mode typeMode;
  n_debug("mtcsasm.c output_constant 00 size:%d align:%d reverse:%d merge_strings:%d exp name:%s flag_syntax_only:%d typemode:%d\n",
        size,align,reverse,merge_strings,exp?get_tree_code_name(TREE_CODE(exp)):"null",
              mtcsOptionsItem->x_flag_syntax_only,TYPE_MODE (TREE_TYPE (exp)));

  if (size == 0 || mtcsOptionsItem->x_flag_syntax_only)
    return size;

  /* See if we're trying to initialize a pointer in a non-default mode
     to the address of some declaration somewhere.  If the target says
     the mode is valid for pointers, assume the target has a way of
     resolving it.  */
  if (TREE_CODE (exp) == NOP_EXPR  && POINTER_TYPE_P (TREE_TYPE (exp))
      && target_addr_space_valid_pointer_mode/*!targetm.addr_space..valid_pointer_mode*/(mtcsMachine->addrSpace,
            mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (exp)),
              TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (exp))))){
      n_debug("mtcsasm.c output_constant 11 \n");
      tree saved_type = TREE_TYPE (exp);

      /* Peel off any intermediate conversions-to-pointer for valid
     pointer modes.  */
      while (TREE_CODE (exp) == NOP_EXPR  && POINTER_TYPE_P (TREE_TYPE (exp))
         && target_addr_space_valid_pointer_mode/*!targetm.addr_space..valid_pointer_mode*/(mtcsMachine->addrSpace,
               mtcs_mode_scalar_int_type_mode/*!SCALAR_INT_TYPE_MODE*/(mtcsMode,TREE_TYPE (exp)),
                 TYPE_ADDR_SPACE (TREE_TYPE (TREE_TYPE (exp)))))
          exp = TREE_OPERAND (exp, 0);
      n_debug("mtcsasm.c output_constant 22 \n");

      /* If what we're left with is the address of something, we can
     convert the address to the final type and output it that
     way.  */
      if (TREE_CODE (exp) == ADDR_EXPR)
          exp = build1 (ADDR_EXPR, saved_type, TREE_OPERAND (exp, 0));
      /* Likewise for constant ints.  */
      else if (TREE_CODE (exp) == INTEGER_CST)
          exp = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,saved_type, exp);

  }

  /* Eliminate any conversions since we'll be outputting the underlying
     constant.  */
  while (CONVERT_EXPR_P (exp) || TREE_CODE (exp) == NON_LVALUE_EXPR || TREE_CODE (exp) == VIEW_CONVERT_EXPR){

      HOST_WIDE_INT type_size = int_size_in_bytes (TREE_TYPE (exp));
      HOST_WIDE_INT op_size = int_size_in_bytes (TREE_TYPE (TREE_OPERAND (exp, 0)));
      n_debug("mtcsasm.c output_constant 33 type_size:%d op_size:%d\n",type_size,op_size);

      /* Make sure eliminating the conversion is really a no-op, except with
     VIEW_CONVERT_EXPRs to allow for wild Ada unchecked conversions and
     union types to allow for Ada unchecked unions.  */
      if (type_size > op_size  && TREE_CODE (exp) != VIEW_CONVERT_EXPR  && TREE_CODE (TREE_TYPE (exp)) != UNION_TYPE)
    /* Keep the conversion. */
          break;
      else
          exp = TREE_OPERAND (exp, 0);
  }

  n_debug("mtcsasm.c output_constant 33-- %p %p\n",exp,TREE_TYPE (exp));

  code = TREE_CODE (TREE_TYPE (exp));
  thissize = int_size_in_bytes (TREE_TYPE (exp));

  /* Allow a constructor with no elements for any data type.
     This means to fill the space with zeros.  */
  if (TREE_CODE (exp) == CONSTRUCTOR  && vec_safe_is_empty (CONSTRUCTOR_ELTS (exp))){
     n_debug("mtcsasm.c output_constant 44 \n");

      mtcs_asm_assemble_zeros (self,size);
      return size;
  }

  if (TREE_CODE (exp) == FDESC_EXPR){
      gcc_unreachable ();
      return size;
  }

  /* Now output the underlying data.  If we've handling the padding, return.
     Otherwise, break and ensure SIZE is the size written.  */
  switch (code) {
    case BOOLEAN_TYPE:
    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
    case POINTER_TYPE:
    case REFERENCE_TYPE:
    case OFFSET_TYPE:
    case FIXED_POINT_TYPE:
    case NULLPTR_TYPE:
      //typeMode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));
      typeMode=TYPE_MODE (TREE_TYPE (exp));
      n_debug("mtcsasm.c output_constant 55 typeMode:%d reverse:%d\n",typeMode,reverse);

      cst = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,exp, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);
      if (reverse)
          cst = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,
                typeMode/*!TYPE_MODE (TREE_TYPE (exp))*/, cst);
      if (!mtcs_asm_assemble_integer/*!assemble_integer*/(self,cst, MIN (size, thissize), align, 0))
          error ("initializer for integer/fixed-point value is too complicated");
      break;

    case REAL_TYPE:
      n_debug("mtcsasm.c output_constant 66 REAL_TYPE typeMode:%d\n",mtcs_mode_host2device_scalar_float(mtcsMode,TREE_TYPE (exp)));
      gcc_assert (size == thissize);
      if (TREE_CODE (exp) != REAL_CST)
          error ("initializer for floating value is not a floating constant");
      else
         mtcs_asm_assemble_real (self,TREE_REAL_CST (exp),
               mtcs_mode_host2device_scalar_float(mtcsMode,TREE_TYPE (exp))/*!SCALAR_FLOAT_TYPE_MODE (TREE_TYPE (exp))*/
               ,align, reverse);
      break;

    case COMPLEX_TYPE:
       n_debug("mtcsasm.c output_constant 77  COMPLEX_TYPE\n");

      output_constant (self,TREE_REALPART (exp), thissize / 2, align,reverse, false);
      output_constant (self,TREE_IMAGPART (exp), thissize / 2, min_align (align, BITS_PER_UNIT * (thissize / 2)),reverse, false);
      break;

    case BITINT_TYPE:
      if (TREE_CODE (exp) != INTEGER_CST)
          error ("initializer for %<_BitInt(%d)%> value is not an integer constant", TYPE_PRECISION (TREE_TYPE (exp)));
      else{
         typeMode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));
         n_debug("mtcsasm.c output_constant 88  BITINT_TYPE typeMode:%d\n",typeMode);

          struct bitint_info info;
          tree type = TREE_TYPE (exp);
          bool ok = target_c_bitint_type_info/*!targetm.c.bitint_type_info*/(mtcsMachine->c,TYPE_PRECISION (type), &info);
          gcc_assert (ok);
          scalar_int_mode limb_mode = mtcs_mode_as_a/*!as_a*/<scalar_int_mode>(mtcsMode,info.abi_limb_mode);
          if (TYPE_PRECISION (type) <= mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode)){
              cst = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,exp, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);
              n_debug("mtcsasm.c output_constant 99  BITINT_TYPE typeMode:%d reverse:%d\n",typeMode,reverse);

              if (reverse)
                  cst = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,
                        typeMode/*!TYPE_MODE (TREE_TYPE (exp))*/, cst);
              if (!mtcs_asm_assemble_integer (self,cst, MIN (size, thissize), align, 0))
                  error ("initializer for integer/fixed-point value is too complicated");
              break;
          }
          int prec = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,limb_mode);
          int cnt = CEIL (TYPE_PRECISION (type), prec);
          tree limb_type = mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,prec, 1);
          int elt_size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,limb_mode);
          unsigned int nalign = MIN (align, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,limb_mode));
          thissize = 0;
          if (prec == HOST_BITS_PER_WIDE_INT)
            for (int i = 0; i < cnt; i++){
                int idx = (info.big_endian ^ reverse) ? cnt - 1 - i : i;
                tree c;
                if (idx >= TREE_INT_CST_EXT_NUNITS (exp))
                  c = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,limb_type,tree_int_cst_sgn (exp) < 0 ? -1 : 0);
                else
                  c = mtcs_tree_build_int_cst/*!build_int_cst*/(mtcsTree,limb_type,TREE_INT_CST_ELT (exp, idx));
                output_constant (self,c, elt_size, nalign, reverse, false);
                thissize += elt_size;
            }
          else
            for (int i = 0; i < cnt; i++){
                int idx = (info.big_endian ^ reverse) ? cnt - 1 - i : i;
                wide_int w = wi::rshift (wi::to_wide (exp), idx * prec,TYPE_SIGN (TREE_TYPE (exp)));
                tree c = mtcs_tree_wide_int_to_tree/*!wide_int_to_tree*/(mtcsTree,limb_type,wide_int::from (w, prec, UNSIGNED));
                output_constant (self,c, elt_size, nalign, reverse, false);
                thissize += elt_size;
            }
      }
      break;

    case ARRAY_TYPE:
    case VECTOR_TYPE:
      switch (TREE_CODE (exp)){
            case CONSTRUCTOR:
               n_debug("mtcsasm.c output_constant 变量是数组或向量并初始化 size:%d align:%d reverse:%d\n",size,align,reverse);
              aet_print_tree(exp);
              return output_constructor(self,exp, size, align, reverse, NULL);
            case STRING_CST:

              thissize = (unsigned HOST_WIDE_INT)TREE_STRING_LENGTH (exp);
              if (merge_strings && (thissize == 0 || TREE_STRING_POINTER (exp) [thissize - 1] != '\0'))
                thissize++;
              gcc_checking_assert (check_string_literal (exp, size));
              n_debug("mtcsasm.c output_constant 100  STRING_CST thissize:%d %s\n",thissize,TREE_STRING_POINTER (exp));

              mtcs_asm_assemble_string (self,TREE_STRING_POINTER (exp), thissize);
              break;
            case VECTOR_CST:
              {

                scalar_mode inner = mtcs_mode_scalar_type_mode/*!SCALAR_TYPE_MODE*/(mtcsMode,TREE_TYPE (TREE_TYPE (exp)));
                unsigned int nalign = MIN (align, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,inner));
                int elt_size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,inner);
                output_constant (self,VECTOR_CST_ELT (exp, 0), elt_size, align,reverse, false);
                thissize = elt_size;
                n_debug("mtcsasm.c output_constant 101  VECTOR_CST thissize:%d\n",thissize);

                /* Static constants must have a fixed size.  */
                unsigned int nunits = VECTOR_CST_NELTS (exp).to_constant ();
                for (unsigned int i = 1; i < nunits; i++){
                    output_constant (self,VECTOR_CST_ELT (exp, i), elt_size, nalign,reverse, false);
                    thissize += elt_size;
                }
                break;
              }
            default:
              gcc_unreachable ();
      }
      break;

    case RECORD_TYPE:
    case UNION_TYPE:
       n_debug("mtcsasm.c output_constant 102  RECORD_TYPE  UNION_TYPE\n");

      gcc_assert (TREE_CODE (exp) == CONSTRUCTOR);
      return output_constructor(self,exp, size, align, reverse, NULL);

    case ERROR_MARK:
      return 0;

    default:
      gcc_unreachable ();
  }
  n_debug("mtcsasm.c output_constant 103  size > thissize size:%d thissize:%d\n",size,thissize);

  if (size > thissize)
    mtcs_asm_assemble_zeros (self,size - thissize);

  return size;
}

/* Subroutine of output_constructor, used for computing the size of
   arrays of unspecified length.  VAL must be a CONSTRUCTOR of an array
   type with an unspecified upper bound.  */

static unsigned HOST_WIDE_INT array_size_for_constructor (tree val)
{
  tree max_index;
  unsigned HOST_WIDE_INT cnt;
  tree index, value, tmp;
  offset_int i;

  /* This code used to attempt to handle string constants that are not
     arrays of single-bytes, but nothing else does, so there's no point in
     doing it here.  */
  if (TREE_CODE (val) == STRING_CST)
    return TREE_STRING_LENGTH (val);

  max_index = NULL_TREE;
  FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (val), cnt, index, value){
      if (TREE_CODE (index) == RANGE_EXPR)
          index = TREE_OPERAND (index, 1);
      if (max_index == NULL_TREE || tree_int_cst_lt (max_index, index))
          max_index = index;
  }

  if (max_index == NULL_TREE)
    return 0;

  /* Compute the total number of array elements.  */
  tmp = TYPE_MIN_VALUE (TYPE_DOMAIN (TREE_TYPE (val)));
  i = wi::to_offset (max_index) - wi::to_offset (tmp) + 1;

  /* Multiply by the array element unit size to find number of bytes.  */
  i *= wi::to_offset (TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (val))));

  gcc_assert (wi::fits_uhwi_p (i));
  return i.to_uhwi ();
}

/* Other datastructures + helpers for output_constructor.  */

/* output_constructor local state to support interaction with helpers.  */

struct oc_local_state {

  /* Received arguments.  */
  tree exp;                     /* Constructor expression.  */
  tree type;                    /* Type of constructor expression.  */
  unsigned HOST_WIDE_INT size;  /* # bytes to output - pad if necessary.  */
  unsigned int align;           /* Known initial alignment.  */
  tree min_index;               /* Lower bound if specified for an array.  */

  /* Output processing state.  */
  HOST_WIDE_INT total_bytes;  /* # bytes output so far / current position.  */
  int byte;                   /* Part of a bitfield byte yet to be output.  */
  int last_relative_index;    /* Implicit or explicit index of the last
                 array element output within a bitfield.  */
  bool byte_buffer_in_use;    /* Whether BYTE is in use.  */
  bool reverse;               /* Whether reverse storage order is in use.  */

  /* Current element.  */
  tree field;      /* Current field decl in a record.  */
  tree val;        /* Current element value.  */
  tree index;      /* Current element index.  */

};

/* Helper for output_constructor.  From the current LOCAL state, output a
   RANGE_EXPR element.  */

static void output_constructor_array_range (MtcsAsm *self,oc_local_state *local)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  /* Perform the index calculation in modulo arithmetic but
     sign-extend the result because Ada has negative DECL_FIELD_OFFSETs
     but we are using an unsigned sizetype.  */
  unsigned prec = TYPE_PRECISION (sizetype);
  offset_int idx = wi::sext (wi::to_offset (TREE_OPERAND (local->index, 0)) - wi::to_offset (local->min_index), prec);
  tree valtype = TREE_TYPE (local->val);
  HOST_WIDE_INT fieldpos= (idx * wi::to_offset (TYPE_SIZE_UNIT (valtype))).to_short_addr ();

  /* Advance to offset of this element.  */
  if (fieldpos > local->total_bytes){
      mtcs_asm_assemble_zeros (self,fieldpos - local->total_bytes);
      local->total_bytes = fieldpos;
  }else
    /* Must not go backwards.  */
    gcc_assert (fieldpos == local->total_bytes);

  unsigned HOST_WIDE_INT fieldsize  = int_size_in_bytes (TREE_TYPE (local->type));
  HOST_WIDE_INT lo_index = tree_to_shwi (TREE_OPERAND (local->index, 0));
  HOST_WIDE_INT hi_index = tree_to_shwi (TREE_OPERAND (local->index, 1));
  HOST_WIDE_INT index;
  unsigned int align2 = min_align (local->align, fieldsize * BITS_PER_UNIT);
  for (index = lo_index; index <= hi_index; index++){
      /* Output the element's initial value.  */
      if (local->val == NULL_TREE)
          mtcs_asm_assemble_zeros (self,fieldsize);
      else
          fieldsize = output_constant (self,local->val, fieldsize, align2,local->reverse, false);

      /* Count its size.  */
      local->total_bytes += fieldsize;
  }
}

/* Helper for output_constructor.  From the current LOCAL state, output a
   field element that is not true bitfield or part of an outer one.  */

static void output_constructor_regular_field (MtcsAsm *self,oc_local_state *local)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  /* Field size and position.  Since this structure is static, we know the
     positions are constant.  */
  unsigned HOST_WIDE_INT fieldsize;
  HOST_WIDE_INT fieldpos;
  unsigned int align2;
  /* Output any buffered-up bit-fields preceding this element.  */
  if (local->byte_buffer_in_use){
      mtcs_asm_assemble_integer (self,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,local->byte), 1, BITS_PER_UNIT, 1);
      local->total_bytes++;
      local->byte_buffer_in_use = false;
  }

  if (local->index != NULL_TREE){
      /* Perform the index calculation in modulo arithmetic but
     sign-extend the result because Ada has negative DECL_FIELD_OFFSETs
     but we are using an unsigned sizetype.  */
      unsigned prec = TYPE_PRECISION (sizetype);
      offset_int idx = wi::sext (wi::to_offset (local->index) - wi::to_offset (local->min_index), prec);
      fieldpos = (idx * wi::to_offset (TYPE_SIZE_UNIT (TREE_TYPE (local->val)))).to_short_addr ();
  }else if (local->field != NULL_TREE)
    fieldpos = int_byte_position (local->field);
  else
    fieldpos = 0;

  /* Advance to offset of this element.
     Note no alignment needed in an array, since that is guaranteed
     if each element has the proper size.  */
  if (local->field != NULL_TREE || local->index != NULL_TREE){
      if (fieldpos > local->total_bytes){
          mtcs_asm_assemble_zeros (self,fieldpos - local->total_bytes);
          local->total_bytes = fieldpos;
      }else
        /* Must not go backwards.  */
        gcc_assert (fieldpos == local->total_bytes);
  }

  /* Find the alignment of this element.  */
  align2 = min_align (local->align, BITS_PER_UNIT * fieldpos);

  /* Determine size this element should occupy.  */
  if (local->field){
      fieldsize = 0;

      /* If this is an array with an unspecified upper bound,
     the initializer determines the size.  */
      /* ??? This ought to only checked if DECL_SIZE_UNIT is NULL,
     but we cannot do this until the deprecated support for
     initializing zero-length array members is removed.  */
      if (TREE_CODE (TREE_TYPE (local->field)) == ARRAY_TYPE  && (!TYPE_DOMAIN (TREE_TYPE (local->field))
          || !TYPE_MAX_VALUE (TYPE_DOMAIN (TREE_TYPE (local->field))))){
          unsigned HOST_WIDE_INT fldsize = array_size_for_constructor (local->val);
          fieldsize = int_size_in_bytes (TREE_TYPE (local->val));
          /* In most cases fieldsize == fldsize as the size of the initializer
             determines how many elements the flexible array member has.  For
             C++ fldsize can be smaller though, if the last or several last or
             all initializers of the flexible array member have side-effects
             and the FE splits them into dynamic initialization.  */
          gcc_checking_assert (fieldsize >= fldsize);
          /* Given a non-empty initialization, this field had better
             be last.  Given a flexible array member, the next field
             on the chain is a TYPE_DECL of the enclosing struct.  */
          const_tree next = DECL_CHAIN (local->field);
          gcc_assert (!fieldsize || !next || TREE_CODE (next) != FIELD_DECL);
      }else
          fieldsize = tree_to_uhwi (DECL_SIZE_UNIT (local->field));
  }else
    fieldsize = int_size_in_bytes (TREE_TYPE (local->type));

  /* Output the element's initial value.  */
  if (local->val == NULL_TREE)
    mtcs_asm_assemble_zeros(self,fieldsize);
  else
    fieldsize = output_constant(self,local->val, fieldsize, align2,local->reverse, false);

  /* Count its size.  */
  local->total_bytes += fieldsize;
}

/* Helper for output_constructor.  From the LOCAL state, output an element
   that is a true bitfield or part of an outer one.  BIT_OFFSET is the offset
   from the start of a possibly ongoing outer byte buffer.  */

static void output_constructor_bitfield (MtcsAsm *self,oc_local_state *local, unsigned int bit_offset)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  /* Bit size of this element.  */
  HOST_WIDE_INT ebitsize = (local->field? tree_to_uhwi (DECL_SIZE (local->field)): tree_to_uhwi (TYPE_SIZE (TREE_TYPE (local->type))));

  /* Relative index of this element if this is an array component.  */
  HOST_WIDE_INT relative_index = (local->field ? 0 :
          (local->index? tree_to_uhwi (local->index) - tree_to_uhwi (local->min_index): local->last_relative_index + 1));

  /* Bit position of this element from the start of the containing
     constructor.  */
  HOST_WIDE_INT constructor_relative_ebitpos= (local->field ? int_bit_position (local->field) : ebitsize * relative_index);

  /* Bit position of this element from the start of a possibly ongoing
     outer byte buffer.  */
  HOST_WIDE_INT byte_relative_ebitpos= bit_offset + constructor_relative_ebitpos;

  /* From the start of a possibly ongoing outer byte buffer, offsets to
     the first bit of this element and to the first bit past the end of
     this element.  */
  HOST_WIDE_INT next_offset = byte_relative_ebitpos;
  HOST_WIDE_INT end_offset = byte_relative_ebitpos + ebitsize;

  local->last_relative_index = relative_index;

  if (local->val == NULL_TREE)
    local->val = integer_zero_node;

  while (TREE_CODE (local->val) == VIEW_CONVERT_EXPR  || TREE_CODE (local->val) == NON_LVALUE_EXPR)
    local->val = TREE_OPERAND (local->val, 0);

  if (TREE_CODE (local->val) != INTEGER_CST && TREE_CODE (local->val) != CONSTRUCTOR){
      error ("invalid initial value for member %qE", DECL_NAME (local->field));
      return;
  }

  /* If this field does not start in this (or next) byte, skip some bytes.  */
  if (next_offset / BITS_PER_UNIT != local->total_bytes){
      /* Output remnant of any bit field in previous bytes.  */
      if (local->byte_buffer_in_use){
          mtcs_asm_assemble_integer (self,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,local->byte), 1, BITS_PER_UNIT, 1);
          local->total_bytes++;
          local->byte_buffer_in_use = false;
      }

      /* If still not at proper byte, advance to there.  */
      if (next_offset / BITS_PER_UNIT != local->total_bytes){
          gcc_assert (next_offset / BITS_PER_UNIT >= local->total_bytes);
          mtcs_asm_assemble_zeros (self,next_offset / BITS_PER_UNIT - local->total_bytes);
          local->total_bytes = next_offset / BITS_PER_UNIT;
      }
  }

  /* Set up the buffer if necessary.  */
  if (!local->byte_buffer_in_use){
      local->byte = 0;
      if (ebitsize > 0)
          local->byte_buffer_in_use = true;
  }

  /* If this is nested constructor, recurse passing the bit offset and the
     pending data, then retrieve the new pending data afterwards.  */
  if (TREE_CODE (local->val) == CONSTRUCTOR){
      oc_outer_state temp_state;
      temp_state.bit_offset = next_offset % BITS_PER_UNIT;
      temp_state.byte = local->byte;
      local->total_bytes += output_constructor (self,local->val, 0, 0, local->reverse, &temp_state);
      local->byte = temp_state.byte;
      return;
  }

  /* Otherwise, we must split the element into pieces that fall within
     separate bytes, and combine each byte with previous or following
     bit-fields.  */
  while (next_offset < end_offset){
      int this_time;
      int shift;
      unsigned HOST_WIDE_INT value;
      HOST_WIDE_INT next_byte = next_offset / BITS_PER_UNIT;
      HOST_WIDE_INT next_bit = next_offset % BITS_PER_UNIT;

      /* Advance from byte to byte within this element when necessary.  */
      while (next_byte != local->total_bytes){
          mtcs_asm_assemble_integer (self,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,local->byte), 1, BITS_PER_UNIT, 1);
          local->total_bytes++;
          local->byte = 0;
      }

      /* Number of bits we can process at once (all part of the same byte).  */
      this_time = MIN (end_offset - next_offset, BITS_PER_UNIT - next_bit);
      if (local->reverse ? !BYTES_BIG_ENDIAN : BYTES_BIG_ENDIAN){
          /* For big-endian data, take the most significant bits (of the
             bits that are significant) first and put them into bytes from
             the most significant end.  */
          shift = end_offset - next_offset - this_time;

          /* Don't try to take a bunch of bits that cross
             the word boundary in the INTEGER_CST.  We can
             only select bits from one element.  */
          if ((shift / HOST_BITS_PER_WIDE_INT) != ((shift + this_time - 1) / HOST_BITS_PER_WIDE_INT)){
              const int end = shift + this_time - 1;
              shift = end & -HOST_BITS_PER_WIDE_INT;
              this_time = end - shift + 1;
          }

          /* Now get the bits we want to insert.  */
          value = wi::extract_uhwi (wi::to_widest (local->val),shift, this_time);

          /* Get the result.  This works only when:
             1 <= this_time <= HOST_BITS_PER_WIDE_INT.  */
          local->byte |= value << (BITS_PER_UNIT - this_time - next_bit);
      }else{
          /* On little-endian machines, take the least significant bits of
             the value first and pack them starting at the least significant
             bits of the bytes.  */
          shift = next_offset - byte_relative_ebitpos;

          /* Don't try to take a bunch of bits that cross
             the word boundary in the INTEGER_CST.  We can
             only select bits from one element.  */
          if ((shift / HOST_BITS_PER_WIDE_INT) != ((shift + this_time - 1) / HOST_BITS_PER_WIDE_INT))
            this_time= HOST_BITS_PER_WIDE_INT - (shift & (HOST_BITS_PER_WIDE_INT - 1));

          /* Now get the bits we want to insert.  */
          value = wi::extract_uhwi (wi::to_widest (local->val),shift, this_time);

          /* Get the result.  This works only when:
             1 <= this_time <= HOST_BITS_PER_WIDE_INT.  */
          local->byte |= value << next_bit;
      }

      next_offset += this_time;
      local->byte_buffer_in_use = true;
  }
}

/* Subroutine of output_constant, used for CONSTRUCTORs (aggregate constants).
   Generate at least SIZE bytes, padding if necessary.  OUTER designates the
   caller output state of relevance in recursive invocations.  */

static unsigned HOST_WIDE_INT output_constructor (MtcsAsm *self,tree exp, unsigned HOST_WIDE_INT size,
      unsigned int align,bool reverse, oc_outer_state *outer)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsTree  *mtcsTree =mtcs_target_get_tree(mtcsTarget);

  unsigned HOST_WIDE_INT cnt;
  constructor_elt *ce;
  oc_local_state local;
  n_debug("mtcsasm.c output_constructor 00 size:%d align:%d reverse:%d\n",size,align,reverse);
  aet_print_tree(exp);
  aet_print_tree(TREE_TYPE (exp));

  /* Setup our local state to communicate with helpers.  */
  local.exp = exp;
  local.type = TREE_TYPE (exp);
  local.size = size;
  local.align = align;
  if (TREE_CODE (local.type) == ARRAY_TYPE && TYPE_DOMAIN (local.type))
    local.min_index = TYPE_MIN_VALUE (TYPE_DOMAIN (local.type));
  else
    local.min_index = integer_zero_node;

  local.total_bytes = 0;
  local.byte_buffer_in_use = outer != NULL;
  local.byte = outer ? outer->byte : 0;
  local.last_relative_index = -1;
  /* The storage order is specified for every aggregate type.  */
  if (AGGREGATE_TYPE_P (local.type))
    local.reverse = TYPE_REVERSE_STORAGE_ORDER (local.type);
  else
    local.reverse = reverse;

  gcc_assert (HOST_BITS_PER_WIDE_INT >= BITS_PER_UNIT);

  /* As CE goes through the elements of the constant, FIELD goes through the
     structure fields if the constant is a structure.  If the constant is a
     union, we override this by getting the field from the TREE_LIST element.
     But the constant could also be an array.  Then FIELD is zero.

     There is always a maximum of one element in the chain LINK for unions
     (even if the initializer in a source program incorrectly contains
     more one).  */

  if (TREE_CODE (local.type) == RECORD_TYPE)
    local.field = TYPE_FIELDS (local.type);
  else
    local.field = NULL_TREE;

  for (cnt = 0;vec_safe_iterate (CONSTRUCTOR_ELTS (exp), cnt, &ce);cnt++, local.field = local.field ? DECL_CHAIN (local.field) : 0){
      local.val = ce->value;
      n_debug("mtcsasm.c output_constructor 11 val:\n");
      aet_print_tree(local.val);
      local.index = NULL_TREE;

      /* The element in a union constructor specifies the proper field
     or index.  */
      if (RECORD_OR_UNION_TYPE_P (local.type) && ce->index != NULL_TREE)
          local.field = ce->index;
      else if (TREE_CODE (local.type) == ARRAY_TYPE)
          local.index = ce->index;

      if (local.field && flag_verbose_asm)
          fprintf (self->asmFile, "%s %s:\n",self->asmCommentStart/*!ASM_COMMENT_START*/,
                DECL_NAME (local.field)? IDENTIFIER_POINTER (DECL_NAME (local.field)): "<anonymous>");

      /* Eliminate the marker that makes a cast not be an lvalue.  */
      if (local.val != NULL_TREE)
          STRIP_NOPS (local.val);

      /* Output the current element, using the appropriate helper ...  */

      /* For an array slice not part of an outer bitfield.  */
      if (!outer && local.index != NULL_TREE && TREE_CODE (local.index) == RANGE_EXPR)
          output_constructor_array_range(self,&local);
      /* For a field that is neither a true bitfield nor part of an outer one,
     known to be at least byte aligned and multiple-of-bytes long.  */
      else if (!outer && (local.field == NULL_TREE || !CONSTRUCTOR_BITFIELD_P (local.field)))
          output_constructor_regular_field(self,&local);
      /* For a true bitfield or part of an outer one.  Only INTEGER_CSTs are
     supported for scalar fields, so we may need to convert first.  */
      else{
          if (TREE_CODE (local.val) == REAL_CST)
            local.val = fold_unary (VIEW_CONVERT_EXPR,
                  mtcs_tree_build_nonstandard_integer_type/*!build_nonstandard_integer_type*/(mtcsTree,
                        TYPE_PRECISION (TREE_TYPE (local.val)), 0),local.val);
          output_constructor_bitfield(self,&local, outer ? outer->bit_offset : 0);
      }
  }

  /* If we are not at toplevel, save the pending data for our caller.
     Otherwise output the pending data and padding zeros as needed. */
  if (outer)
    outer->byte = local.byte;
  else{
      if (local.byte_buffer_in_use){
          mtcs_asm_assemble_integer (self,mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,local.byte), 1, BITS_PER_UNIT, 1);
          local.total_bytes++;
      }

      if ((unsigned HOST_WIDE_INT)local.total_bytes < local.size){
          mtcs_asm_assemble_zeros (self,local.size - local.total_bytes);
          local.total_bytes = local.size;
      }
  }
  return local.total_bytes;
}

/* This TREE_LIST contains weakref targets.  */

static vec<alias_pair, va_gc> *mtcs_alias_pairs;

/* Record and output a table of translations from original function
   to its transaction aware clone.  Note that tm_pure functions are
   considered to be their own clone.  */

struct tm_clone_hasher : ggc_cache_ptr_hash<tree_map>
{
  static hashval_t hash (tree_map *m) { return tree_map_hash (m); }
  static bool equal (tree_map *a, tree_map *b) { return tree_map_eq (a, b); }

  static int
  keep_cache_entry (tree_map *&e)
  {
    return ggc_marked_p (e->base.from);
  }
};

static GTY((cache)) hash_table<tm_clone_hasher> *tm_clone_hash;

#ifndef TLS_SECTION_ASM_FLAG
#define TLS_SECTION_ASM_FLAG 'T'
#endif


/////////////////////************************MtcsAsm------------------------------
//原型 output.h switch_to_section
static void switchToSection(MtcsAsm *self,section *, tree = nullptr);
//原型 output.h assemble_integer
static void outputConstantDefContents (MtcsAsm *self,rtx symbol);
static section * getConstantSection (MtcsAsm *self,tree exp, unsigned int align);

/* Return DECL_ALIGN (decl), possibly increased for optimization purposes
   beyond what mtcs_align_variable returned.  */

static unsigned int get_variable_align (MtcsAsm *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   unsigned int align = DECL_ALIGN (decl);
   /* For user aligned vars or static vars mtcs_align_variable already did
   everything.  */
   if (DECL_USER_ALIGN (decl) || !TREE_PUBLIC (decl))
      return align;

   /* For decls that bind to the current definition, mtcs_align_variable
   did also everything, except for not assuming ABI required alignment
   of TLS variables.  For other vars, increase the alignment here
   as an optimization.  */
   if (!mtcs_asm_decl_binds_to_current_def_p (self,decl)){
      /* On some machines, it is good to increase alignment sometimes.  */
      unsigned int data_align = mtcsTarget->data_alignment(mtcsTarget,TREE_TYPE (decl), align);
      /* Don't increase alignment too much for TLS variables - TLS space
      is too precious.  */
      if (! DECL_THREAD_LOCAL_P (decl) || data_align <= BITS_PER_WORD)
         align = data_align;
      if (DECL_INITIAL (decl) != 0
      /* In LTO we have no errors in program; error_mark_node is used
      to mark offlined constructors.  */
      && (in_lto_p || DECL_INITIAL (decl) != error_mark_node)){
         unsigned int const_align = mtcsTarget->constant_alignment (mtcsTarget,DECL_INITIAL (decl), align);
         /* Don't increase alignment too much for TLS variables - TLS space
         is too precious.  */
         if (! DECL_THREAD_LOCAL_P (decl) || const_align <= BITS_PER_WORD)
            align = const_align;
      }
   }

   return align;
}

/* Return true when RESOLUTION indicate that symbol will be bound to the
   definition provided by current .o file.  */
static bool resolution_to_local_definition_p (enum ld_plugin_symbol_resolution resolution)
{
  return (resolution == LDPR_PREVAILING_DEF || resolution == LDPR_PREVAILING_DEF_IRONLY_EXP || resolution == LDPR_PREVAILING_DEF_IRONLY);
}

/* Emit the assembly bits to indicate that DECL is globally visible.  */

static void globalize_decl (MtcsAsm *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   target_asm_out_globalize_decl_name/*!targetm.asm_out.globalize_decl_name*/(mtcsMachine->asmOut, decl);
}

/* Return the size of constant EXP in bytes.  */
static HOST_WIDE_INT getConstantSize (tree exp)
{
   HOST_WIDE_INT size;
   size = int_size_in_bytes (TREE_TYPE (exp));
   gcc_checking_assert (size >= 0);
   gcc_checking_assert (TREE_CODE (exp) != STRING_CST || size >= TREE_STRING_LENGTH (exp));
   return size;
}

/* Return DECL_ALIGN (decl), possibly increased for optimization purposes
   beyond what mtcs_align_variable returned.  */

static unsigned int getVariableAlign (MtcsAsm *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  unsigned int align = DECL_ALIGN (decl);
  /* For user aligned vars or static vars mtcs_align_variable already did
     everything.  */
  if (DECL_USER_ALIGN (decl) || !TREE_PUBLIC (decl))
    return align;

  /* For decls that bind to the current definition, mtcs_align_variable
     did also everything, except for not assuming ABI required alignment
     of TLS variables.  For other vars, increase the alignment here
     as an optimization.  */
  if (!mtcs_asm_decl_binds_to_current_def_p (self,decl)){
      /* On some machines, it is good to increase alignment sometimes.  */
      unsigned int data_align = mtcsTarget->data_alignment(mtcsTarget,TREE_TYPE (decl), align);
      /* Don't increase alignment too much for TLS variables - TLS space
         is too precious.  */
      if (! DECL_THREAD_LOCAL_P (decl) || data_align <= BITS_PER_WORD)
              align = data_align;
      if (DECL_INITIAL (decl) != 0
      /* In LTO we have no errors in program; error_mark_node is used
         to mark offlined constructors.  */
      && (in_lto_p || DECL_INITIAL (decl) != error_mark_node)){
          unsigned int const_align = mtcsTarget->constant_alignment (mtcsTarget,DECL_INITIAL (decl), align);
          /* Don't increase alignment too much for TLS variables - TLS space
             is too precious.  */
          if (! DECL_THREAD_LOCAL_P (decl) || const_align <= BITS_PER_WORD)
            align = const_align;
      }
  }

  return align;
}

/* Subroutine of mtcs_output_constant_def:
   No constant equal to EXP is known to have been output.
   Make a constant descriptor to enter EXP in the hash table.
   Assign the label number and construct RTL to refer to the
   constant's location in memory.
   Caller is responsible for updating the hash table.  */
//原型 build_constant_desc varasm.cc
static struct constant_descriptor_tree *build_constant_desc (MtcsAsm *self,tree exp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

   struct constant_descriptor_tree *desc;
   rtx symbol, rtl;
   char label[256];
   int labelno;
   tree decl;

   desc = ggc_alloc<constant_descriptor_tree> ();
   desc->value = exp;
   n_debug("mtcsasm.c build_constant_desc 00 创建常数 LC 开头 \n");

   /* Create a string containing the label name, in LABEL.  */
   labelno = self->const_labelno++;
   mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,label, "LC", labelno);

   /* Construct the VAR_DECL associated with the constant.  */
   decl =mtcs_tree_build_decl/*!build_decl*/(mtcsTree,UNKNOWN_LOCATION, VAR_DECL, get_identifier (label),TREE_TYPE (exp));
   DECL_ARTIFICIAL (decl) = 1;
   DECL_IGNORED_P (decl) = 1;
   TREE_READONLY (decl) = 1;
   TREE_STATIC (decl) = 1;
   TREE_ADDRESSABLE (decl) = 1;
   /* We don't set the RTL yet as this would cause varpool to assume that the
   variable is referenced.  Moreover, it would just be dropped in LTO mode.
   Instead we set the flag that will be recognized in mtcs_make_decl_rtl.  */
   DECL_IN_CONSTANT_POOL (decl) = 1;
   DECL_INITIAL (decl) = desc->value;
   /* ??? targetm.constant_alignment hasn't been updated for vector types on
   most architectures so use DATA_ALIGNMENT as well, except for strings.  */
   if (TREE_CODE (exp) == STRING_CST){
      n_debug("mtcsasm.c build_constant_desc 11 常数是字符串 \n");
      SET_DECL_ALIGN (decl, mtcsTarget->constant_alignment (mtcsTarget,exp, DECL_ALIGN (decl)));
   }else{
      n_debug("mtcsasm.c build_constant_desc 22 非字符常数\n");

      mtcs_asm_align_variable (self,decl, 0);
      if (DECL_ALIGN (decl) < mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,DECL_MODE (decl))
      && ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab, DECL_MODE (decl))!= CODE_FOR_nothing)
      || mtcsTarget->slow_unaligned_access (mtcsTarget,DECL_MODE (decl),DECL_ALIGN (decl))))
         SET_DECL_ALIGN (decl, mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,DECL_MODE (decl)));
   }

   /* Now construct the SYMBOL_REF and the MEM.  */
   if (use_object_blocks_p(self)){
      n_debug("mtcsasm.c build_constant_desc 33\n");
      int align = (TREE_CODE (decl) == CONST_DECL|| (VAR_P (decl) && DECL_IN_CONSTANT_POOL (decl))
      ? DECL_ALIGN (decl) : symtab_node::get (decl)->definition_alignment ());
      section *sect = getConstantSection(self,exp, align);
      symbol = create_block_symbol (self,ggc_strdup (label),get_block_for_section(self,sect), -1);
   }else{
      n_debug("mtcsasm.c build_constant_desc 44 DECL_ALIGN (decl):%d\n",DECL_ALIGN (decl));
      symbol = gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), ggc_strdup (label));
   }
   SYMBOL_REF_FLAGS (symbol) |= SYMBOL_FLAG_LOCAL;
   SET_SYMBOL_REF_DECL (symbol, decl);
   TREE_CONSTANT_POOL_ADDRESS_P (symbol) = 1;
   n_debug("mtcsasm.c build_constant_desc 55 host Pmode:%d mtcs Pmode:%d\n",Pmode,mtcs_mode_get_Pmode(mtcsMode));
   machine_mode typeMode=mtcs_mode_host2device_by_tree(mtcsMode,TREE_TYPE (exp),TYPE_MODE (TREE_TYPE (exp)));
   rtl = gen_const_mem (typeMode/*!TYPE_MODE (TREE_TYPE (exp))*/, symbol);
   mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,rtl, 0);
   /* Putting EXP into the literal pool might have imposed a different
   alignment which should be visible in the RTX as well.  */
   mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,rtl, DECL_ALIGN (decl));
   /* We cannot share RTX'es in pool entries.
   Mark this piece of RTL as required for unsharing.  */
   RTX_FLAG (rtl, used) = 1;
   n_debug("mtcsasm.c build_constant_desc 66 typeMode:%d\n",typeMode);

   /* Set flags or add text to the name to record information, such as
   that it is a local symbol.  If the name is changed, the macro
   ASM_OUTPUT_LABELREF will have to know how to strip this
   information.  This call might invalidate our local variable
   SYMBOL; we can't use it afterward.  */
   mtcs_output_encode_section_info/*!targetm.encode_section_info*/(mtcsOutput,exp, rtl, true);
   desc->rtl = rtl;
   n_debug("mtcsasm.c build_constant_desc 77 完成\n");
   return desc;
}

/* Subroutine of output_constant_def_contents.  Output the definition
   of constant EXP, which is pointed to by label LABEL.  ALIGN is the
   constant's alignment in bits.  */

static void assembleConstantContents (MtcsAsm *self,tree exp, const char *label, unsigned int align,bool merge_strings)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   n_debug("mtcsasm.c  00 %s label:%s align:%d merge_strings:%d\n",
            get_tree_code_name(TREE_CODE(exp)),label,align,merge_strings);
   HOST_WIDE_INT size;
   size = getConstantSize (exp);
   n_debug("mtcsasm.c assembleConstantContents 11 size:%d\n",size);
   /* Do any machine/system dependent processing of the constant.  */
   //写入常数到汇编文件
   FILE *back=self->asmFile;
   self->asmFile=self->asmVarDeclFile;
   target_asm_out_declare_constant_name/*!targetm.asm_out.declare_constant_name*/(mtcsMachine->asmOut, label, exp, size);
   n_debug("mtcsasm.c assembleConstantContents 22 size:%d\n",size);

   /* Output the value of EXP.  */
   output_constant (self,exp, size, align, false, merge_strings);
   n_debug("mtcsasm.c assembleConstantContents 33 size:%d\n",size);

   target_asm_out_decl_end/*!targetm.asm_out.decl_end */(mtcsMachine->asmOut);
   self->asmFile=back;

   n_debug("mtcsasm.c assembleConstantContents 44 结束 size:%d\n",size);

}

/* Subroutine of mtcs_output_constant_def: Decide whether or not we need to
   output the constant DESC now, and if so, do it.  */
//原型 maybe_output_constant_def_contents varasm.cc
static void maybe_output_constant_def_contents (MtcsAsm *self,struct constant_descriptor_tree *desc,int defer)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  rtx symbol = XEXP (desc->rtl, 0);
  tree exp = desc->value;
  if (mtcsOptionsItem->x_flag_syntax_only)
    return;
  if (TREE_ASM_WRITTEN (exp))
    /* Already output; don't do it again.  */
    return;
  /* We can always defer constants as long as the context allows
     doing so.  */
  if (defer){
      /* Increment n_deferred_constants if it exists.  It needs to be at
     least as large as the number of constants actually referred to
     by the function.  If it's too small we'll stop looking too early
     and fail to emit constants; if it's too large we'll only look
     through the entire function when we could have stopped earlier.  */
      if (cfun)
          n_deferred_constants++;
      return;
  }
  n_debug("mtcsasm.c maybe_output_constant_def_contents 00 symbol mode:%d\n",GET_MODE(symbol));
  outputConstantDefContents(self,symbol);
}

/* Find all the constants whose addresses are referenced inside of EXP,
   and make sure assembler code with a label has been output for each one.
   Indicate whether an ADDR_EXPR has been encountered.  */
/**
 * output_constant_def 在varasm中定义，在output.h中声明
 * 因为output_constant_def 调 maybe_output_constant_def_contents -->output_constant_def_contents
 * output_constant_def_contents是和target有关，所以需要在这里实现该函数
 */
static void outputAddressedConstants (MtcsAsm *self,tree exp, int defer)
{
   tree tem;

   n_debug("mtcsasm.c outputAddressedConstants 00 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
   switch (TREE_CODE (exp)){
      case ADDR_EXPR:
      case FDESC_EXPR:
         n_debug("mtcsasm.c outputAddressedConstants 11 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);

         /* Go inside any operations that get_inner_reference can handle and see
         if what's inside is a constant: no need to do anything here for
         addresses of variables or functions.  */
         for (tem = TREE_OPERAND (exp, 0); handled_component_p (tem); tem = TREE_OPERAND (tem, 0))
            ;
         n_debug("mtcsasm.c outputAddressedConstants 22 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);

         /* If we have an initialized CONST_DECL, retrieve the initializer.  */
         if (TREE_CODE (tem) == CONST_DECL && DECL_INITIAL (tem))
            tem = DECL_INITIAL (tem);
         n_debug("mtcsasm.c outputAddressedConstants 33 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);

         if (CONSTANT_CLASS_P (tem) || TREE_CODE (tem) == CONSTRUCTOR)
            mtcs_asm_output_constant_def(self,tem, defer);
         n_debug("mtcsasm.c outputAddressedConstants 44 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);

         if (TREE_CODE (tem) == MEM_REF)
            outputAddressedConstants (self,TREE_OPERAND (tem, 0), defer);
         break;

      case PLUS_EXPR:
      case POINTER_PLUS_EXPR:
      case MINUS_EXPR:
         n_debug("mtcsasm.c outputAddressedConstants 55 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
         outputAddressedConstants (self,TREE_OPERAND (exp, 1), defer);
         gcc_fallthrough ();

      CASE_CONVERT:
      case VIEW_CONVERT_EXPR:
         n_debug("mtcsasm.c outputAddressedConstants 66 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
         outputAddressedConstants (self,TREE_OPERAND (exp, 0), defer);
         break;

      case CONSTRUCTOR:
      {
         n_debug("mtcsasm.c outputAddressedConstants 77 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
         unsigned HOST_WIDE_INT idx;
         FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (exp), idx, tem)
            if (tem != 0)
               outputAddressedConstants (self,tem, defer);
      }
         break;

      default:
         break;
   }
}

/* Return the section into which constant EXP should be placed.  */
static section * getConstantSection (MtcsAsm *self,tree exp, unsigned int align)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    return target_asm_out_select_section/*!targetm.asm_out.select_section*/(mtcsMachine->asmOut,
          exp,mtcs_asm_compute_reloc_for_constant(self,exp),align);
}

/* We must output the constant data referred to by SYMBOL; do so.  */
static void outputConstantDefContents (MtcsAsm *self,rtx symbol)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  tree decl = SYMBOL_REF_DECL (symbol);
  tree exp = DECL_INITIAL (decl);
  bool asan_protected = false;
  n_debug("mtcsasm.c outputConstantDefContents 00 调用 outputAddressedConstants \n");
  /* Make sure any other constants whose addresses appear in EXP
     are assigned label numbers.  */
  outputAddressedConstants(self,exp, 0);
  /* We are no longer deferring this constant.  */
  TREE_ASM_WRITTEN (decl) = TREE_ASM_WRITTEN (exp) = 1;
  n_debug("mtcsasm.c outputConstantDefContents 11 调用 outputAddressedConstants 结束\n");

  if ((mtcsOptionsItem->x_flag_sanitize & SANITIZE_ADDRESS)
        && TREE_CODE (exp) == STRING_CST && asan_protect_global (exp)){
     n_debug("mtcsasm.c outputConstantDefContents 22\n");
      asan_protected = true;
      SET_DECL_ALIGN (decl, MAX (DECL_ALIGN (decl), ASAN_RED_ZONE_SIZE * BITS_PER_UNIT));
  }
  /* If the constant is part of an object block, make sure that the
     decl has been positioned within its block, but do not write out
     its definition yet.  mtcs_output_object_blocks will do that later.  */
  if (SYMBOL_REF_HAS_BLOCK_INFO_P (symbol) && SYMBOL_REF_BLOCK (symbol)){
     n_debug("mtcsasm.c outputConstantDefContents 33\n");
     mtcs_asm_place_block_symbol (self,symbol);
  }else{
      int align = (TREE_CODE (decl) == CONST_DECL || (VAR_P (decl) && DECL_IN_CONSTANT_POOL (decl))
           ? DECL_ALIGN (decl): symtab_node::get (decl)->definition_alignment ());
      n_debug("mtcsasm.c outputConstantDefContents 44 align:%d\n",align);

      section *sect = getConstantSection(self,exp, align);
      n_debug("mtcsasm.c outputConstantDefContents 55 section:%d\n",sect);
      mtcs_asm_switch_to_section (self,sect);
      n_debug("mtcsasm.c outputConstantDefContents 66 section:%d BITS_PER_UNIT:%d\n",sect,BITS_PER_UNIT);

      if (align > BITS_PER_UNIT)
          self->output_align(self, floor_log2 (align / BITS_PER_UNIT));

      n_debug("mtcsasm.c outputConstantDefContents 77 section:%d BITS_PER_UNIT:%d\n",sect,BITS_PER_UNIT);

      assembleConstantContents (self,exp, XSTR (symbol, 0), align,(sect->common.flags & SECTION_MERGE)
            && (sect->common.flags & SECTION_STRINGS));
      if (asan_protected){
         n_debug("mtcsasm.c outputConstantDefContents 88 section:%d BITS_PER_UNIT:%d\n",sect,BITS_PER_UNIT);

          HOST_WIDE_INT size = getConstantSize (exp);
          mtcs_asm_assemble_zeros (self,asan_red_zone_size (size));
      }
  }
}

/* Mark all constants that are referenced by SYMBOL_REFs in X.
   Emit referenced deferred strings.  */
static void markConstantsInPattern (MtcsAsm *self,rtx insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, PATTERN (insn), ALL){
      const_rtx x = *iter;
      if (GET_CODE (x) == SYMBOL_REF){
         if (CONSTANT_POOL_ADDRESS_P (x)){
            class constant_descriptor_rtx *desc = SYMBOL_REF_CONSTANT (x);
            if (desc->mark == 0){
               desc->mark = 1;
               iter.substitute (desc->constant);
            }
         }else if (TREE_CONSTANT_POOL_ADDRESS_P (x)){
            tree decl = SYMBOL_REF_DECL (x);
            if (!TREE_ASM_WRITTEN (DECL_INITIAL (decl))){
               n_deferred_constants--;
               outputConstantDefContents(self,CONST_CAST_RTX (x));
            }
         }
      }
   }
}

/* Look through appropriate parts of INSN, marking all entries in the
   constant pool which are actually being used.  Entries that are only
   referenced by other constants are also marked as used.  Emit
   deferred strings that are used.  */
static void markConstants (MtcsAsm *self,rtx_insn *insn)
{
  if (!INSN_P (insn))
    return;

  /* Insns may appear inside a SEQUENCE.  Only check the patterns of
     insns, not any notes that may be attached.  We don't want to mark
     a constant just because it happens to appear in a REG_EQUIV note.  */
  if (rtx_sequence *seq = dyn_cast <rtx_sequence *> (PATTERN (insn))){
      int i, n = seq->len ();
      for (i = 0; i < n; ++i){
          rtx subinsn = seq->element (i);
          if (INSN_P (subinsn))
              markConstantsInPattern (self,subinsn);
       }
  }else
      markConstantsInPattern (self,insn);
}

/* Look through the instructions for this function, and mark all the
   entries in POOL which are actually being used.  Emit deferred constants
   which have indeed been used.  */

static void markConstantPool (MtcsAsm *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

  rtx_insn *insn;
  if (!mtcsRtlData/*!crtl*/->uses_const_pool && n_deferred_constants == 0)
    return;
  for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
      markConstants (self,insn);
}

/* Subroutine of mtcs_output_constant_def and mtcs_tree_output_constant_def:
   Add a constant to the hash table that tracks which constants
   already have labels.  */
//原型 add_constant_to_table varasm.cc
static constant_descriptor_tree *add_constant_to_table (MtcsAsm *self,tree exp, int defer)
{
   /* The hash table methods may call mtcs_output_constant_def for addressed
   constants, so handle them first.  */
   n_debug("mtcsasm.c addConstantToTable 00 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
   outputAddressedConstants (self,exp, defer);
   /* Sanity check to catch recursive insertion.  */
   static bool inserting;
   gcc_assert (!inserting);
   inserting = true;
   /* Look up EXP in the table of constant descriptors.  If we didn't
   find it, create a new one.  */
   struct constant_descriptor_tree key;
   key.value = exp;
   key.hash = const_hash_1(self,exp);
   //原型 constant_descriptor_tree **loc= const_desc_htab->find_slot_with_hash (&key, key.hash, INSERT);
   constant_descriptor_tree *loc = n_hash_table_lookup_by_hash(self->const_desc_htab,&key,key.hash);
   /*!
   inserting = false;
   struct constant_descriptor_tree *desc = *loc;
   if (!desc){
   desc = build_constant_desc (self,exp);
   desc->hash = key.hash;
   *loc = desc;
   }*/
   inserting = false;
   struct constant_descriptor_tree *desc = loc;
   if (!desc){
      n_debug("mtcsasm.c addConstantToTable 11 构建常数 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
      desc = build_constant_desc(self,exp);
      desc->hash = key.hash;
      n_hash_table_insert(self->const_desc_htab,desc,desc);
      loc = desc;
   }
   n_debug("mtcsasm.c addConstantToTable 22 结束 %s defer:%d\n",get_tree_code_name(TREE_CODE(exp)),defer);
   return desc;
}

/* Worker function for output_constant_pool_1.  Emit assembly for X
   in MODE with known alignment ALIGN.  */
static void outputConstantPool_2 (MtcsAsm *self,fixed_size_mode mode, rtx x, unsigned int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  switch (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode)){
    case MODE_FLOAT:
    case MODE_DECIMAL_FLOAT:
      {
        gcc_assert (CONST_DOUBLE_AS_FLOAT_P (x));
        mtcs_asm_assemble_real (self,*CONST_DOUBLE_REAL_VALUE (x),
                mtcs_mode_as_a/*!as_a*/<scalar_float_mode>(mtcsMode,mode), align, false);
        break;
      }

    case MODE_INT:
    case MODE_PARTIAL_INT:
    case MODE_FRACT:
    case MODE_UFRACT:
    case MODE_ACCUM:
    case MODE_UACCUM:
      mtcs_asm_assemble_integer (self,x, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), align, 1);
      break;
    case MODE_VECTOR_BOOL:
      {
        gcc_assert (GET_CODE (x) == CONST_VECTOR);
            /* Pick the smallest integer mode that contains at least one
           whole element.  Often this is byte_mode and contains more
           than one element.  */
        unsigned int nelts = mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode).coeffs[0];
        unsigned int elt_bits = mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode) / nelts;
        unsigned int int_bits = MAX (elt_bits, BITS_PER_UNIT);
        scalar_int_mode int_mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,int_bits, 0).require ();
        unsigned int mask = mtcs_mode_get_mask/*!GET_MODE_MASK*/(mtcsMode,
                mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode));
            /* We allow GET_MODE_PRECISION (mode) <= GET_MODE_BITSIZE (mode) but
           only properly handle cases where the difference is less than a
           byte.  */
        gcc_assert (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,mode) -
                mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,mode) < BITS_PER_UNIT);
            /* Build the constant up one integer at a time.  */
        unsigned int elts_per_int = int_bits / elt_bits;
        for (unsigned int i = 0; i < nelts; i += elts_per_int){
            unsigned HOST_WIDE_INT value = 0;
            unsigned int limit = MIN (nelts - i, elts_per_int);
            for (unsigned int j = 0; j < limit; ++j){
              auto elt = INTVAL (CONST_VECTOR_ELT (x, i + j));
              value |= (elt & mask) << (j * elt_bits);
            }
            outputConstantPool_2 (self,int_mode, mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,value, int_mode),
                    i != 0 ? MIN (align, int_bits) : align);
        }
        break;
      }
    case MODE_VECTOR_FLOAT:
    case MODE_VECTOR_INT:
    case MODE_VECTOR_FRACT:
    case MODE_VECTOR_UFRACT:
    case MODE_VECTOR_ACCUM:
    case MODE_VECTOR_UACCUM:
      {
        int i, units;
        scalar_mode submode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,mode);
        unsigned int subalign = MIN (align, mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,submode));

        gcc_assert (GET_CODE (x) == CONST_VECTOR);
        units = mtcs_mode_get_nunits/*!GET_MODE_NUNITS*/(mtcsMode,mode).coeffs[0];

        for (i = 0; i < units; i++){
            rtx elt = CONST_VECTOR_ELT (x, i);
            outputConstantPool_2 (self,submode, elt, i ? subalign : align);
        }
      }
      break;
    default:
      gcc_unreachable ();
  }
}


/* Worker function for output_constant_pool.  Emit constant DESC,
   giving it ALIGN bits of alignment.  */

static void outputConstantPool_1(MtcsAsm *self,class constant_descriptor_rtx *desc, unsigned int align)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   rtx x, tmp;
   x = desc->constant;
   /* See if X is a LABEL_REF (or a CONST referring to a LABEL_REF)
   whose CODE_LABEL has been deleted.  This can occur if a jump table
   is eliminated by optimization.  If so, write a constant of zero
   instead.  Note that this can also happen by turning the
   CODE_LABEL into a NOTE.  */
   /* ??? This seems completely and utterly wrong.  Certainly it's
   not true for NOTE_INSN_DELETED_LABEL, but I disbelieve proper
   functioning even with rtx_insn::deleted and friends.  */
   tmp = x;
   switch (GET_CODE (tmp)){
      case CONST:
         if (GET_CODE (XEXP (tmp, 0)) != PLUS || GET_CODE (XEXP (XEXP (tmp, 0), 0)) != LABEL_REF)
            break;
         tmp = XEXP (XEXP (tmp, 0), 0);
         /* FALLTHRU  */
      case LABEL_REF:
      {
         rtx_insn *insn = label_ref_label (tmp);
         gcc_assert (!insn->deleted ());
         gcc_assert (!NOTE_P (insn) || NOTE_KIND (insn) != NOTE_INSN_DELETED);
         break;
      }
      default:
         break;
   }
   mtcs_asm_assemble_align (self,align);
   /* Output the label.  */
   target_asm_out_internal_label/*!targetm.asm_out.internal_label*/(mtcsMachine->asmOut, "LC", desc->labelno);
   /* Output the data.
   Pass actual alignment value while emitting string constant to asm code
   as function 'output_constant_pool_1' explicitly passes the alignment as 1
   assuming that the data is already aligned which prevents the generation
   of fix-up table entries.  */
   outputConstantPool_2 (self,desc->mode, x, desc->align);
   /* Make sure all constants in SECTION_MERGE and not SECTION_STRINGS
   sections have proper size.  */
   if (align > mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,desc->mode)
   && self->in_section && (self->in_section->common.flags & SECTION_MERGE))
      mtcs_asm_assemble_align (self,align);

   return;
}

/* Write all the constants in POOL.  */
static void outputConstantPoolContents (MtcsAsm *self,struct rtx_constant_pool *pool)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   class constant_descriptor_rtx *desc;
   for (desc = pool->first; desc ; desc = desc->next)
      if (desc->mark < 0){
         gcc_checking_assert (TARGET_SUPPORTS_ALIASES);
         const char *name = XSTR (desc->sym, 0);
         char label[256];
         char buffer[256 + 32];
         const char *p;

         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,label, "LC", ~desc->mark);
         p = label;
         if (desc->offset){
            sprintf (buffer, "%s+" HOST_WIDE_INT_PRINT_DEC, p, desc->offset);
            p = buffer;
         }
         target_asm_out_output_def/*!ASM_OUTPUT_DEF*/(mtcsMachine->asmOut, name, p);
      }else if (desc->mark) {
         /* If the constant is part of an object_block, make sure that
         the constant has been positioned within its block, but do not
         write out its definition yet.  mtcs_output_object_blocks will do
         that later.  */
         if (SYMBOL_REF_HAS_BLOCK_INFO_P (desc->sym)  && SYMBOL_REF_BLOCK (desc->sym))
            mtcs_asm_place_block_symbol (self,desc->sym);
         else{
            section *newSection=target_asm_out_select_rtx_section/*!argetm.asm_out.select_rtx_section*/
                     (mtcsMachine->asmOut,desc->mode, desc->constant, desc->align);
            mtcs_asm_switch_to_section (self,newSection);
            outputConstantPool_1 (self,desc, desc->align);
         }
      }
}

/* Mark all constants that are used in the current function, then write
   out the function's private constant pool.  */
static void outputConstantPool (MtcsAsm *self,const char *fnname ATTRIBUTE_UNUSED, tree fndecl ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   struct rtx_constant_pool *pool = mtcsRtlData/*!crtl*/->varasm.pool;

   /* It is possible for gcc to call mtcs_force_const_mem and then to later
   discard the instructions which refer to the constant.  In such a
   case we do not need to output the constant.  */
   markConstantPool (self);
   /* Having marked the constant pool entries we'll actually emit, we
   now need to rebuild the offset information, which may have become
   stale.  */
   recompute_pool_offsets (self,pool);
   outputConstantPoolContents(self,pool);
}

/* Return NAME that should actually be emitted, looking through
   transparent aliases.  If NAME refers to an entity that is also
   represented as a tree (like a function or variable), mark the entity
   as referenced.  */
static const char *assembleNameResolve(MtcsAsm *self,const char *name)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   const char *real_name = mtcsTarget->strip_name_encoding/*!targetm.strip_name_encoding*/(mtcsTarget,name);
   tree id = maybe_get_identifier (real_name);
   if (id){
      tree id_orig = id;
      mtcs_mark_referenced (id);
      ultimate_transparent_alias_target (&id);
      if (id != id_orig)
         name = IDENTIFIER_POINTER (id);
      gcc_assert (! TREE_CHAIN (id));
   }
   return name;
}
//原型 assemble_name_resolve output.h varasm.cc
const char *mtcs_asm_assemble_name_resolve (MtcsAsm *self,const char *name)
{
   return assembleNameResolve(self,name);
}

/* Like mtcs_assemble_name_raw, but should be used when NAME might refer to
   an entity that is also represented as a tree (like a function or
   variable).  If NAME does refer to such an entity, that entity will
   be marked as referenced.  */
static void assembleNameRaw (MtcsAsm *self, const char *name)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine  *mtcsMachine =mtcs_target_get_machine(mtcsTarget);

   if (name[0] == '*')
      fputs (&name[1], self->asmFile);
   else
      target_asm_out_output_labelref/*!ASM_OUTPUT_LABELREF*/(mtcsMachine->asmOut,name);
}

static void assembleAlign (MtcsAsm *self,unsigned int align)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  if (align > BITS_PER_UNIT){
      //ASM_OUTPUT_ALIGN (asm_out_file, floor_log2 (align / BITS_PER_UNIT));
      self->output_align(self, floor_log2 (align / BITS_PER_UNIT));
  }
}

static void assembleName (MtcsAsm *self,const char *name)
{
   assembleNameRaw (self, assembleNameResolve (self,name));
}

/**
 * 等同ASM_OUTPUT_LABEL
 * gcn的实现不一样,MtcsGcnAsm需要覆盖这个方法
 */
static void outputLabel_cb(MtcsAsm *self,const char *name)
{
   assembleName(self,name);
   fputs (":\n", self->asmFile);
}

//原型 #define ASM_OUTPUT_TYPE_DIRECTIVE(STREAM, NAME, TYPE)
static void outputTypeDirective_cb(MtcsAsm *self,const char *name,const char *type)
{
   fprintf(stderr,"还未确定是否应该定义 ASM_OUTPUT_TYPE_DIRECTIVE\n ");
   abort();
//        fputs (TYPE_ASM_OP, STREAM);       \
//        assemble_name (STREAM, NAME);       \
//        fputs (", ", self->asmFile);            \
//        fprintf (self->asmFile, TYPE_OPERAND_FMT, TYPE);    \
//        putc ('\n', self->asmFile);
}

/**
 *  来自宏ASM_OUTPUT_INTERNAL_LABEL ptx没有自已的实现
 */
static void outputInternalLabel_cb(MtcsAsm *self,const char *name)
{
    assembleNameRaw (self,name);
    fputs (":\n", self->asmFile);
}

/* Return a SECTION_NOSWITCH section with the given fields.  */
static section *createNoSwitchSection (unsigned int flags, noswitch_section_callback callback)
{
  section *sect;
  sect = ggc_alloc<section> ();
  sect->noswitch.common.flags = flags | SECTION_NOSWITCH;
  sect->noswitch.callback = callback;
  return sect;
}



/* A noswitch_section_callback for mtcs_lcomm_section.  */
static bool emit_local (tree decl ATTRIBUTE_UNUSED,  const char *name ATTRIBUTE_UNUSED,
        unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED, unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED)
{
  MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  unsigned int align = symtab_node::get (decl)->definition_alignment ();
  target_asm_out_output_aligned_decl_local/*!ASM_OUTPUT_ALIGNED_DECL_LOCAL*/(mtcsMachine->asmOut, decl, name, size, align);
  return true;

}

/* A noswitch_section_callback for mtcs_comm_section.  */
//原型 emit_common varasm.cc
//没有MtcsAsm *self 由外部destAsm代替
static bool emit_common (tree decl ATTRIBUTE_UNUSED,const char *name ATTRIBUTE_UNUSED,
         unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED, unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsAsm *self=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine  *mtcsMachine =mtcs_target_get_machine(mtcsTarget);

   unsigned int align= getVariableAlign (self,decl);
   target_asm_out_output_aligned_decl_common/*!ASM_OUTPUT_ALIGNED_DECL_COMMON*/(mtcsMachine->asmOut,decl, name, size, align);
   return true;
}

/* A noswitch_section_callback for mtcs_tls_comm_section.  */
static bool emit_tls_common (tree decl ATTRIBUTE_UNUSED, const char *name ATTRIBUTE_UNUSED,
         unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED, unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED)
{
   sorry ("thread-local COMMON data not implemented");
   return true;
}

static bool emit_bss (tree decl ATTRIBUTE_UNUSED,
      const char *name ATTRIBUTE_UNUSED,
      unsigned HOST_WIDE_INT size ATTRIBUTE_UNUSED,
      unsigned HOST_WIDE_INT rounded ATTRIBUTE_UNUSED)
{
   //ASM_OUTPUT_ALIGNED_BSS (asm_out_file, decl, name, size, get_variable_align (decl));
   n_error("ptx ASM_OUTPUT_ALIGNED_BSS =false 不应该进这里!!!");
   return true;
}

//原型 #define ASM_APP_ON "\t// #APP \n"
void   mtcs_asm_app_on(MtcsAsm *self)
{
    self->app_on(self);
}

//原型 #define ASM_APP_OFF "\t// #NO_APP \n"
void     mtcs_asm_app_off(MtcsAsm *self)
{
   self->app_off(self);
}

void     mtcs_asm_start_function(MtcsAsm *self,tree decl, const char *fnname)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  int align;
  char tmp_label[100];
  bool hot_label_written = false;
  n_debug("mtcsasm.c assemble_start_function 00 fnname:%s\n",fnname);
  if(n_hash_table_contains(self->funcHashTable,fnname)){
      n_error("报告此错误 在函数mtcs_asm_start_function 重复的函数名 %s\n",fnname);
      return;
  }
  if (mtcsRtlData/*!crtl*/->has_bb_partition){
         n_debug("mtcsasm.c assemble_start_function 11 crtl->has_bb_partition=true fnname:%s\n",fnname);
         //ASM_GENERATE_INTERNAL_LABEL保留
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,tmp_label, "LHOTB", self->const_labelno);
         mtcsRtlData/*!crtl*/->subsections.hot_section_label = ggc_strdup (tmp_label);
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,tmp_label, "LCOLDB", self->const_labelno);
         mtcsRtlData/*!crtl*/->subsections.cold_section_label = ggc_strdup (tmp_label);
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,tmp_label, "LHOTE", self->const_labelno);
         mtcsRtlData/*!crtl*/->subsections.hot_section_end_label = ggc_strdup (tmp_label);
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,tmp_label, "LCOLDE", self->const_labelno);
         mtcsRtlData/*!crtl*/->subsections.cold_section_end_label = ggc_strdup (tmp_label);
      self->const_labelno++;
      self->cold_function_name = NULL_TREE;
  }else{
      n_debug("mtcsasm.c assemble_start_function 22 fnname:%s\n",fnname);

      mtcsRtlData/*!crtl*/->subsections.hot_section_label = NULL;
      mtcsRtlData/*!crtl*/->subsections.cold_section_label = NULL;
      mtcsRtlData/*!crtl*/->subsections.hot_section_end_label = NULL;
      mtcsRtlData/*!crtl*/->subsections.cold_section_end_label = NULL;
  }

  /* The following code does not need preprocessing in the assembler.  */
  if(self->appDisableCallback)
      self->appDisableCallback(self,self->userData);

  if (CONSTANT_POOL_BEFORE_FUNCTION){
     n_debug("mtcsasm.c assemble_start_function 33 outputConstantPool fnname:%s\n",fnname);
     outputConstantPool (self,fnname, decl);
  }

  align = symtab_node::get (decl)->definition_alignment ();

  /* Make sure the not and cold text (code) sections are properly
     aligned.  This is necessary here in the case where the function
     has both hot and cold sections, because we don't want to re-set
     the alignment when the section switch happens mid-function.  */

  if (mtcsRtlData/*!crtl*/->has_bb_partition){
      self->first_function_block_is_cold = false;
      n_debug("mtcsasm.c assemble_start_function 44  fnname:%s\n",fnname);

      switchToSection (self,mtcs_asm_unlikely_text_section (self));
      assembleAlign (self,align);
      self->output_label(self, mtcsRtlData/*!crtl*/->subsections.cold_section_label);

      /* When the function starts with a cold section, we need to explicitly
     align the hot section and write out the hot section label.
     But if the current function is a thunk, we do not have a CFG.  */
      if (!cfun->is_thunk && BB_PARTITION (ENTRY_BLOCK_PTR_FOR_FN (cfun)->next_bb) == BB_COLD_PARTITION){
          n_debug("mtcsasm.c assemble_start_function 55 outputConstantPool fnname:%s\n",fnname);
          switchToSection (self,self->text_section);
          assembleAlign (self,align);
          self->output_label(self, mtcsRtlData/*!crtl*/->subsections.hot_section_label);
          hot_label_written = true;
          self->first_function_block_is_cold = true;
      }
      self->in_cold_section_p = self->first_function_block_is_cold;
  }


  /* Switch to the correct text section for the start of the function.  */
  n_debug("mtcsasm.c assemble_start_function 66 switchToSection fnname:%s\n",fnname);

  switchToSection(self,mtcs_ams_function_section (self,decl), decl);
  if (mtcsRtlData/*!crtl*/->has_bb_partition && !hot_label_written){
      n_debug("mtcsasm.c assemble_start_function 77 outputConstantPool fnname:%s\n",fnname);
      //ASM_OUTPUT_LABEL (asm_out_file, crtl->subsections.hot_section_label);
      self->output_label(self, mtcsRtlData/*!crtl*/->subsections.hot_section_label);

  }

  /* Tell assembler to move to target machine's alignment for functions.  */
  align = floor_log2 (align / BITS_PER_UNIT);
  /* Handle forced alignment.  This really ought to apply to all functions,
     since it is used by patchable entries.  */
  if (flag_min_function_alignment)
    align = MAX (align, floor_log2 (flag_min_function_alignment));

  if (align > 0){
      //ASM_OUTPUT_ALIGN (asm_out_file, align);
      self->output_align(self,align);
   }

  /* Handle a user-specified function alignment.
     Note that we still need to align to DECL_ALIGN, as above,
     because ASM_OUTPUT_MAX_SKIP_ALIGN might not do any alignment at all.  */
  if (! DECL_USER_ALIGN (decl) && align_functions.levels[0].log > align  && optimize_function_for_speed_p (cfun)){
      //npvtx没有定义宏，所以下面代码取消
      int max_skip = align_functions.levels[0].maxskip;
      if (flag_limit_function_alignment && mtcsRtlData/*!crtl*/->max_insn_address > 0
            && max_skip >= mtcsRtlData/*!crtl*/->max_insn_address)
          max_skip = mtcsRtlData/*!crtl*/->max_insn_address - 1;
      n_debug("mtcsasm.c assemble_start_function 88 ASM_OUTPUT_MAX_SKIP_ALIGN=false fnname:%s\n",fnname);
      self->output_align(self, align_functions.levels[0].log);
  }


  if (!DECL_IGNORED_P (decl)){
      n_debug("mtcsasm.c assemble_start_function 99 (*debug_hooks->begin_function) fnname:%s\n",fnname);
     mtcs_debug_begin_function/*!(*debug_hooks->begin_function)*/(mtcsDebug,decl);
  }

  /* Make function name accessible from other files, if appropriate.  */

  if (TREE_PUBLIC (decl)){
      n_debug("mtcsasm.c assemble_start_function 100 globalize_decl fnname:%s\n",fnname);
      mtcs_asm_notice_global_symbol (self,decl);
      globalize_decl (self,decl);
      mtcs_asm_maybe_assemble_visibility (self,decl);
  }

  if (DECL_PRESERVE_P (decl)){
   n_debug("mtcsasm.c assemble_start_function 101  if (DECL_PRESERVE_P (decl)){ fnname:%s\n",fnname);
   //targetm.asm_out.mark_decl_preserved (fnname);
   //TARGET_ASM_MARK_DECL_PRESERVED gcc中的实现是hook_void_constcharptr 声明在gcc/target.def
   target_asm_out_mark_decl_preserved/*!TARGET_ASM_MARK_DECL_PRESERVED*/(mtcsMachine->asmOut,fnname);
   n_debug("mtcsasm.c assemble_start_function 102  if (DECL_PRESERVE_P (decl)){ fnname:%s\n",fnname);

  }

  unsigned short patch_area_size = mtcsRtlData/*!crtl*/->patch_area_size;
  unsigned short patch_area_entry = mtcsRtlData/*!crtl*/->patch_area_entry;

  /* Emit the patching area before the entry label, if any.  */
  if (patch_area_entry > 0){
      n_debug("mtcsasm.c assemble_start_function 103  if (patch_area_entry > 0){ fnname:%s\n",fnname);
    //TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY 对应的函数在nvptx是default_print_patchable_function_entry
      //来自宏TARGET_ASM_PRINT_PATCHABLE_FUNCTION_ENTRY和targetm.asm_out.print_patchable_function_entry 缺省ptx实现
      //targetm.asm_out.print_patchable_function_entry (asm_out_file,patch_area_entry, true);
      target_asm_out_print_patchable_function_entry/*! targetm.asm_out.print_patchable_function_entry*/
                     (mtcsMachine->asmOut,patch_area_entry,true);
  }

  /* Do any machine/system dependent processing of the function name.  */
  //nvptx 定义了ASM_DECLARE_FUNCTION_NAME
  n_debug("mtcsasm.c assemble_start_function 104  ASM_DECLARE_FUNCTION_NAME=true fnname:%s\n",fnname);
  //ASM_DECLARE_FUNCTION_NAME (asm_out_file, fnname, current_function_decl);
  target_asm_out_declare_function_name/*!ASM_DECLARE_FUNCTION_NAME*/(mtcsMachine->asmOut,fnname, current_function_decl);

  /* And the area after the label.  Record it if we haven't done so yet.  */
  if (patch_area_size > patch_area_entry){
      n_debug("mtcsasm.c assemble_start_function 105   if (patch_area_size > patch_area_entry){ fnname:%s\n",fnname);
      //targetm.asm_out.print_patchable_function_entry (asm_out_file,patch_area_size- patch_area_entry,patch_area_entry == 0);
      target_asm_out_print_patchable_function_entry/*! targetm.asm_out.print_patchable_function_entry*/
            (mtcsMachine->asmOut,patch_area_size- patch_area_entry,patch_area_entry == 0);

  }

  if (lookup_attribute ("no_split_stack", DECL_ATTRIBUTES (decl))){
      n_debug("mtcsasm.c assemble_start_function 106   if (lookup_attribute (no_split_stack, DECL_ATTRIBUTES (decl))) fnname:%s\n",fnname);
      self->saw_no_split_stack = true;
  }
}

/* Emit assembly code to switch to section NEW_SECTION.  Do nothing if
   the current section is NEW_SECTION.  */
//原型 switch_to_section output.h varasm.cc
static void switchToSection (MtcsAsm *self,section *new_section, tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   bool retain_p;
   if ((new_section->common.flags & SECTION_NAMED) && decl != nullptr  && DECL_P (decl)
   && ((retain_p = !!lookup_attribute ("retain",DECL_ATTRIBUTES (decl)))!= !!(new_section->common.flags & SECTION_RETAIN))){
      /* If the SECTION_RETAIN bit doesn't match, switch to a new
      section.  */
      tree used_decl, no_used_decl;
      n_debug("mtcsasm.cc switch_to_section 00\n");

      if (retain_p){
         n_debug("mtcsasm.c switch_to_section 11\n");
         new_section->common.flags |= SECTION_RETAIN;
         used_decl = decl;
         no_used_decl = new_section->named.decl;
      }else{
         n_debug("mtcsasm.c switch_to_section 22\n");
         new_section->common.flags &= ~(SECTION_RETAIN | SECTION_DECLARED);
         used_decl = new_section->named.decl;
         no_used_decl = decl;
      }
      if (no_used_decl != used_decl){
         warning (OPT_Wattributes, "%+qD without %<retain%> attribute and %qD with %<retain%> attribute are placed in a section with "
                  "the same name", no_used_decl, used_decl);
         inform (DECL_SOURCE_LOCATION (used_decl), "%qD was declared here", used_decl);
      }
   }else if (self->in_section == new_section){
      n_debug("mtcsasm.cc switch_to_section 33 new_section:%p text:%p readonly:%p\n",
      new_section,self->text_section,self->readonly_data_section);
      return;
   }
   n_debug("mtcsasm.cc switch_to_section 44 new_section:%p text:%p readonly:%p\n",
         new_section,self->text_section,self->readonly_data_section);
   self->in_section = new_section;
   switch (SECTION_STYLE (new_section)){
      case SECTION_NAMED:
         n_debug("mtcsasm.c switch_to_section 55 SECTION_NAMED\n");
         target_asm_out_named_section/*!targetm.asm_out.named_section*/(mtcsMachine->asmOut,
               new_section->named.name,new_section->named.common.flags,new_section->named.decl);
         break;
      case SECTION_UNNAMED:
         n_debug("mtcsasm.c switch_to_section 66 SECTION_UNNAMED %p %p\n",new_section,new_section->unnamed);
         new_section->unnamed.callback (new_section->unnamed.data);
         break;
      case SECTION_NOSWITCH:
         gcc_unreachable ();
         break;
   }

   new_section->common.flags |= SECTION_DECLARED;
}

/**
 * 原型：mtcs_switch_to_comdat_section
 * 删除 #if defined (OBJECT_FORMAT_ELF) TARGET_PECOFF
 * nvptx没有定义OBJECT_FORMAT_ELF和TARGET_PECOFF
 * 不知gcn是否定义?
 */
static void switchToComdatSection(MtcsAsm *self,section *sect, tree decl)
{
  /* Neither OBJECT_FORMAT_PE, nor OBJECT_FORMAT_COFF is set here.
     Therefore the following check is used.
     In case a the target is PE or COFF a comdat group section
     is created, e.g. .vtable_map_vars$foo. The linker places
     everything in .vtable_map_vars at the end.

     A fix could be made in
     gcc/config/i386/winnt.cc: i386_pe_unique_section.  */
    switchToSection (self,sect,nullptr);
}

/* If OP is a REG or MEM and we can find a MEM_EXPR corresponding to it
   or its address, return that expr .  Set *PADDRESSP to 1 if the expr
   corresponds to the address of the object and 0 if to the object.  */

static tree get_mem_expr_from_op (MtcsAsm *self,rtx op, int *paddressp)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  tree expr;
  int inner_addressp;
  *paddressp = 0;
  if (REG_P (op))
    return REG_EXPR (op);
  else if (!MEM_P (op))
    return 0;

  if (mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,op) != 0)
    return mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,op);

  /* Otherwise we have an address, so indicate it and look at the address.  */
  *paddressp = 1;
  op = XEXP (op, 0);

  /* First check if we have a decl for the address, then look at the right side
     if it is a PLUS.  Otherwise, strip off arithmetic and keep looking.
     But don't allow the address to itself be indirect.  */
  if ((expr = get_mem_expr_from_op(self,op, &inner_addressp)) && ! inner_addressp)
    return expr;
  else if (GET_CODE (op) == PLUS
       && (expr = get_mem_expr_from_op(self,XEXP (op, 1), &inner_addressp)))
    return expr;

  while (UNARY_P (op)
     || GET_RTX_CLASS (GET_CODE (op)) == RTX_BIN_ARITH)
    op = XEXP (op, 0);

  expr = get_mem_expr_from_op(self,op, &inner_addressp);
  return inner_addressp ? 0 : expr;
}


void  mtcs_asm_set_app_callback(MtcsAsm *self,AppEnable enable,AppDisable disable,npointer userData)
{
    self->appEnableCallback=enable;
    self->appDisableCallback=disable;
    self->userData=userData;
}

/* Similar, for calling a library function FUN.  */
//原型 assemble_external_libcall output.h varasm.cc
void mtcs_asm_assemble_external_libcall (MtcsAsm *self,rtx fun)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  /* Declare library function name external when first used, if nec.  */
  if (! SYMBOL_REF_USED (fun)){
      SYMBOL_REF_USED (fun) = 1;
      /* Make sure the libcall symbol is in the symtab so any
         reference to it will mark its tree node as referenced, via
         assemble_name_resolve.  These are eventually emitted, if
         used, in process_pending_assemble_externals. */
      const char *name = mtcsTarget->strip_name_encoding/*!targetm.strip_name_encoding*/(mtcsTarget,XSTR (fun, 0));
      get_identifier (name);
      self->pending_libcall_symbols = gen_rtx_EXPR_LIST (VOIDmode, fun, self->pending_libcall_symbols);
  }
}

section  *mtcs_asm_get_exception_section (MtcsAsm *self)
{
  return self->exception_section;
}

void     mtcs_asm_set_exception_section (MtcsAsm *self,section *section)
{
    self->exception_section=section;
}

/* Return the named section structure associated with NAME.  Create
   a new section with the given fields if no such structure exists.
   When NOT_EXISTING, then fail if the section already exists.  Return
   the existing section if the SECTION_RETAIN bit doesn't match.  Set
   the SECTION_WRITE | SECTION_RELRO bits on the existing section
   if one of the section flags is SECTION_WRITE | SECTION_RELRO and the
   other has none of these flags in named sections and either the section
   hasn't been declared yet or has been declared as writable.  */
section *mtcs_asm_get_section (MtcsAsm *self,const char *name, unsigned int flags, tree decl,bool not_existing)
{
  section *sect;
  section *slot;
  //原型   slot = section_htab->find_slot_with_hash (name, htab_hash_string (name),INSERT);
  slot = n_hash_table_lookup_by_hash(self->section_htab,name,htab_hash_string (name));
  flags |= SECTION_NAMED;
  if (decl != nullptr  && DECL_P (decl) && lookup_attribute ("retain", DECL_ATTRIBUTES (decl)))
    flags |= SECTION_RETAIN;
  /*
  if (*slot == NULL){
      sect = ggc_alloc<section> ();
      sect->named.common.flags = flags;
      sect->named.name = ggc_strdup (name);
      sect->named.decl = decl;
      *slot = sect;
      */
  if (slot == NULL){
      sect = ggc_alloc<section> ();
      sect->named.common.flags = flags;
      sect->named.name = ggc_strdup (name);
      sect->named.decl = decl;
      n_hash_table_insert(self->section_htab,sect,sect);
      slot = sect;
  }else{
      if (not_existing)
          internal_error ("section already exists: %qs", name);

      sect = slot;
      /* It is fine if one of the sections has SECTION_NOTYPE as long as
         the other has none of the contrary flags (see the logic at the end
         of mtcs_default_section_type_flags, below).  */
      if (((sect->common.flags ^ flags) & SECTION_NOTYPE)  && !((sect->common.flags | flags)
               & (SECTION_CODE | SECTION_BSS | SECTION_TLS | SECTION_ENTSIZE | (HAVE_COMDAT_GROUP ? SECTION_LINKONCE : 0)))){
          sect->common.flags |= SECTION_NOTYPE;
          flags |= SECTION_NOTYPE;
      }
      if ((sect->common.flags & ~SECTION_DECLARED) != flags && ((sect->common.flags | flags) & SECTION_OVERRIDE) == 0){
          /* It is fine if one of the section flags is
             SECTION_WRITE | SECTION_RELRO and the other has none of these
             flags (i.e. read-only) in named sections and either the
             section hasn't been declared yet or has been declared as writable.
             In that case just make sure the resulting flags are
             SECTION_WRITE | SECTION_RELRO, ie. writable only because of
             relocations.  */
          if (((sect->common.flags ^ flags) & (SECTION_WRITE | SECTION_RELRO)) == (SECTION_WRITE | SECTION_RELRO)
              && (sect->common.flags & ~(SECTION_DECLARED | SECTION_WRITE | SECTION_RELRO)) == (flags & ~(SECTION_WRITE | SECTION_RELRO))
              && ((sect->common.flags & SECTION_DECLARED) == 0  || (sect->common.flags & SECTION_WRITE))) {
              sect->common.flags |= (SECTION_WRITE | SECTION_RELRO);
              return sect;
          }
          /* If the SECTION_RETAIN bit doesn't match, return and switch
             to a new section later.  */
          if ((sect->common.flags & SECTION_RETAIN) != (flags & SECTION_RETAIN))
            return sect;
          /* Sanity check user variables for flag changes.  */
          if (sect->named.decl != NULL && DECL_P (sect->named.decl)  && decl != sect->named.decl){
              if (decl != NULL && DECL_P (decl))
                  error ("%+qD causes a section type conflict with %qD", decl, sect->named.decl);
              else
                  error ("section type conflict with %qD", sect->named.decl);
              inform (DECL_SOURCE_LOCATION (sect->named.decl), "%qD was declared here", sect->named.decl);
          }else if (decl != NULL && DECL_P (decl))
            error ("%+qD causes a section type conflict", decl);
          else
            error ("section type conflict");
          /* Make sure we don't error about one section multiple times.  */
          sect->common.flags |= SECTION_OVERRIDE;
      }
  }
  return sect;
}

//原型 switch_to_section output.h varasm.cc
void mtcs_asm_switch_to_section(MtcsAsm *self,section *section, tree decl)
{
    switchToSection(self,section,decl);
}

/* Assemble a label named NAME.  */
void mtcs_asm_assemble_label (MtcsAsm *self, const char *name)
{
    self->output_label (self, name);
}

void mtcs_asm_assemble_align (MtcsAsm *self,unsigned int align)
{
   assembleAlign(self,align);
}

void mtcs_asm_assemble_name (MtcsAsm *self,const char *name)
{
    assembleName(self,name);
}

void mtcs_asm_assemble_name_raw (MtcsAsm *self,const char *name)
{
    assembleNameRaw(self,name);
}

//原型 assemble_integer output.h varasm.cc
bool mtcs_asm_assemble_integer (MtcsAsm *self,rtx x, unsigned int size, unsigned int align, int force)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);//self->mtcsMode;
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
    MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    int aligned_p;

    aligned_p = (align >= MIN (size * BITS_PER_UNIT, mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign)));

    /* See if the target hook can handle this kind of object.  */
    if (target_asm_out_integer/*!targetm.asm_out.integer*/(mtcsMachine->asmOut,x, size, aligned_p))
      return true;

    /* If the object is a multi-byte one, try splitting it up.  Split
       it into words it if is multi-word, otherwise split it into bytes.  */
    if (size > 1){
        machine_mode omode, imode;
        unsigned int subalign;
        unsigned int subsize, i;
        enum mode_class mclass;

        subsize = size > UNITS_PER_WORD? UNITS_PER_WORD : 1;
        subalign = MIN (align, subsize * BITS_PER_UNIT);
        if (GET_CODE (x) == CONST_FIXED)
            mclass = mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,GET_MODE (x));
        else
            mclass = MODE_INT;

        omode = mtcs_mode_mode_for_size/*!mode_for_size*/(mtcsMode,subsize * BITS_PER_UNIT, mclass, 0).require ();
        imode = mtcs_mode_mode_for_size/*!mode_for_size*/(mtcsMode,size * BITS_PER_UNIT, mclass, 0).require ();

        for (i = 0; i < size; i += subsize){
            rtx partial = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,omode, x, imode, i);
            if (!partial || !mtcs_asm_assemble_integer/*!assemble_integer*/(self,partial, subsize, subalign, 0))
                break;
        }
        if (i == size)
            return true;

        /* If we've printed some of it, but not all of it, there's no going
       back now.  */
        gcc_assert (!i);
    }
    gcc_assert (!force);
    return false;
}

//依赖宏 ASM_OUTPUT_EXTERNAL
static bool incorporeal_function_p (tree decl)
{
   if (TREE_CODE (decl) == FUNCTION_DECL && fndecl_built_in_p (decl)){
      const char *name;

      if (DECL_BUILT_IN_CLASS (decl) == BUILT_IN_NORMAL && ALLOCA_FUNCTION_CODE_P (DECL_FUNCTION_CODE (decl)))
         return true;

      name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
      /* Atomic or sync builtins which have survived this far will be
      resolved externally and therefore are not incorporeal.  */
      if (startswith (name, "__builtin_"))
         return true;
   }
   return false;
}

/* Actually do the tests to determine if this is necessary, and invoke
   ASM_OUTPUT_EXTERNAL.  */
static void assemble_external_real (MtcsAsm *self,tree decl)
{
  rtx rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl);

  if (MEM_P (rtl) && GET_CODE (XEXP (rtl, 0)) == SYMBOL_REF
      && !SYMBOL_REF_USED (XEXP (rtl, 0))
      && !incorporeal_function_p (decl)){
      /* Some systems do require some output.  */
      SYMBOL_REF_USED (XEXP (rtl, 0)) = 1;
      self->output_external/*!ASM_OUTPUT_EXTERNAL*/(self,self->asmFile/*!asm_out_file*/, decl, XSTR (XEXP (rtl, 0), 0));
    }
}

//原型 assemble_external output.h varasm.cc
void mtcs_asm_assemble_external (MtcsAsm *self,tree decl ATTRIBUTE_UNUSED)
{
   if (!DECL_P (decl) || !DECL_EXTERNAL (decl) || !TREE_PUBLIC (decl))
      return;
   /* We want to output annotation for weak and external symbols at
   very last to check if they are references or not.  */

   if (TARGET_SUPPORTS_WEAK  && DECL_WEAK (decl)
   /* TREE_STATIC is a weird and abused creature which is not
   generally the right test for whether an entity has been
   locally emitted, inlined or otherwise not-really-extern, but
   for declarations that can be weak, it happens to be
   match.  */
   && !TREE_STATIC (decl)  && lookup_attribute ("weak",DECL_ATTRIBUTES (decl))
   && value_member (decl, self->weak_decls) == NULL_TREE)
      self->weak_decls = tree_cons (NULL, decl, self->weak_decls);

   if(self->output_external/*!#ifdef ASM_OUTPUT_EXTERNAL*/){
      if (self->pending_assemble_externals_processed){
         assemble_external_real(self,decl);
         return;
      }

      if (!self->pending_assemble_externals_set->add (decl))
         self->pending_assemble_externals = tree_cons (NULL, decl,self->pending_assemble_externals);
   }
   /*
#ifdef ASM_OUTPUT_EXTERNAL
  if (pending_assemble_externals_processed)
    {
      assemble_external_real (decl);
      return;
    }

  if (! pending_assemble_externals_set->add (decl))
    pending_assemble_externals = tree_cons (NULL, decl,
                        pending_assemble_externals);
#endif
  */
}

//原型 default_function_rodata_section output.h varasm.cc
section *mtcs_asm_default_function_rodata_section (MtcsAsm *self,tree decl, bool relocatable)
{
     const char* sname;
     unsigned int flags;
     flags = 0;
     if (relocatable){
         sname = ".data.rel.ro.local";
         flags = (SECTION_WRITE | SECTION_RELRO);
     }else
       sname = ".rodata";

     if (decl && DECL_SECTION_NAME (decl)){
         const char *name = DECL_SECTION_NAME (decl);

         if (DECL_COMDAT_GROUP (decl) && HAVE_COMDAT_GROUP){
             const char *dot;
             size_t len;
             char* rname;
             dot = strchr (name + 1, '.');
             if (!dot)
               dot = name;
             len = strlen (dot) + strlen (sname) + 1;
             rname = (char *) alloca (len);
             strcpy (rname, sname);
             strcat (rname, dot);
             return mtcs_asm_get_section (self,rname, (SECTION_LINKONCE | flags), decl);
         }
         /* For .gnu.linkonce.t.foo we want to use .gnu.linkonce.r.foo or
        .gnu.linkonce.d.rel.ro.local.foo if the jump table is relocatable.  */
         else if (DECL_COMDAT_GROUP (decl) && startswith (name, ".gnu.linkonce.t.")){
             size_t len;
             char *rname;
             if (relocatable){
                 len = strlen (name) + strlen (".rel.ro.local") + 1;
                 rname = (char *) alloca (len);

                 strcpy (rname, ".gnu.linkonce.d.rel.ro.local");
                 strcat (rname, name + 15);
             }else{
                 len = strlen (name) + 1;
                 rname = (char *) alloca (len);

                 memcpy (rname, name, len);
                 rname[14] = 'r';
             }
             return mtcs_asm_get_section (self,rname, (SECTION_LINKONCE | flags), decl);
         }
         /* For .text.foo we want to use .rodata.foo.  */
         else if (flag_function_sections && flag_data_sections && startswith (name, ".text.")){
             size_t len = strlen (name) + 1;
             char *rname = (char *) alloca (len + strlen (sname) - 5);

             memcpy (rname, sname, strlen (sname));
             memcpy (rname + strlen (sname), name + 5, len - 5);
             return mtcs_asm_get_section (self,rname, flags, decl);
         }
     }

     if (relocatable)
         return mtcs_asm_get_section (self,sname, flags, decl);
     else
         return self->readonly_data_section;
}

static section *hotFunctionSection (MtcsAsm *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  if (decl != NULL_TREE && DECL_SECTION_NAME (decl) != NULL
        && mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/){
     n_debug("mtcsasm.c hotFunctionSection 00 %p\n",decl);
    return mtcs_asm_get_named_section (self,decl, NULL, 0);
  }else{
     n_debug("mtcsasm.c hotFunctionSection 11 %p\n",decl);
     return self->text_section;
  }
}

/* Return the section for function DECL.

   If DECL is NULL_TREE, return the text section.  We can be passed
   NULL_TREE under some circumstances by dbxout.cc at least.

   If FORCE_COLD is true, return cold function section ignoring
   the frequency info of cgraph_node.  */

static section *function_section_1 (MtcsAsm *self,tree decl, bool force_cold)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   section *section = NULL;
   enum node_frequency freq = NODE_FREQUENCY_NORMAL;
   bool startup = false, exit = false;
   if (decl){
      struct cgraph_node *node = cgraph_node::get (decl);
      if (node){
         freq = node->frequency;
         startup = node->only_called_at_startup;
         exit = node->only_called_at_exit;
      }
   }
   if (force_cold)
      freq = NODE_FREQUENCY_UNLIKELY_EXECUTED;
   n_debug("mtcsasm.c function_section_1 00  #ifdef USE_SELECT_SECTION_FOR_FUNCTIONS\n");
   if (mtcsMachine->asmOut->function_section/*!targetm.asm_out.function_section*/){
      n_debug("mtcsasm.c function_section_1 11 targetm.asm_out.function_section\n");
      section = target_asm_out_function_section/*!targetm.asm_out.function_section*/(mtcsMachine->asmOut,decl, freq, startup, exit);
   }
   if (section)
      return section;
   n_debug("mtcsasm.c function_section_1 22 section is null hot_function_section \n");

   return hotFunctionSection (self,decl);
}

//原型 function_section output.h varasm.cc
section *mtcs_ams_function_section (MtcsAsm *self,tree decl)
{
   /* Handle cases where function splitting code decides
   to put function entry point into unlikely executed section
   despite the fact that the function itself is not cold
   (i.e. it is called rarely but contains a hot loop that is
   better to live in hot subsection for the code locality).  */
   return function_section_1 (self,decl,self->first_function_block_is_cold);
}

/* Return the section for the current function, take IN_COLD_SECTION_P
   into account.  */
//原型 current_function_section output.h varasm.cc
section *mtcs_asm_current_function_section (MtcsAsm *self)
{
   return function_section_1 (self,current_function_decl, self->in_cold_section_p);
}

/* Tell assembler to switch to unlikely-to-be-executed text section.  */

section *mtcs_asm_unlikely_text_section (MtcsAsm *self)
{
   return function_section_1 (self,current_function_decl, true);
}

/* Return a section with a particular name and with whatever SECTION_*
   flags section_type_flags deems appropriate.  The name of the section
   is taken from NAME if nonnull, otherwise it is taken from DECL's
   DECL_SECTION_NAME.  DECL is the decl associated with the section
   (see the section comment for details) and RELOC is as for
   section_type_flags.  */
//原型 get_named_section output.h varasm.cc
section *mtcs_asm_get_named_section (MtcsAsm *self,tree decl, const char *name, int reloc)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  unsigned int flags;
  if (name == NULL){
      gcc_assert (decl && DECL_P (decl) && DECL_SECTION_NAME (decl));
      name = DECL_SECTION_NAME (decl);
  }
  flags =mtcs_output_section_type_flags/*!targetm.section_type_flags*/(mtcsOutput,decl, name, reloc);
  return mtcs_asm_get_section (self,name, flags, decl);
}

/* Return section for TEXT_SECTION_NAME if DECL or DECL_SECTION_NAME (DECL)
   is NULL.

   When DECL_SECTION_NAME is non-NULL and it is implicit section and
   NAMED_SECTION_SUFFIX is non-NULL, then produce section called
   concatenate the name with NAMED_SECTION_SUFFIX.
   Otherwise produce "TEXT_SECTION_NAME.IMPLICIT_NAME".  */

section *mtcs_asm_get_named_text_section (MtcsAsm *self,tree decl,const char *text_section_name,const char *named_section_suffix)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  if (decl && DECL_SECTION_NAME (decl)){
      if (named_section_suffix){
          const char *dsn = DECL_SECTION_NAME (decl);
          const char *stripped_name;
          char *name, *buffer;

          name = (char *) alloca (strlen (dsn) + 1);
          memcpy (name, dsn,
              strlen (dsn) + 1);

          stripped_name = mtcsTarget->strip_name_encoding/*!targetm.strip_name_encoding*/(mtcsTarget,name);

          buffer = ACONCAT ((stripped_name, named_section_suffix, NULL));
          return mtcs_asm_get_named_section (self,decl, buffer, 0);
      }else if (symtab_node::get (decl)->implicit_section){
          const char *name;

          /* Do not try to split gnu_linkonce functions.  This gets somewhat
             slipperly.  */
          if (DECL_COMDAT_GROUP (decl) && !HAVE_COMDAT_GROUP)
            return NULL;
          name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
          name = mtcsTarget->strip_name_encoding/*!targetm.strip_name_encoding*/(mtcsTarget,name);
          return mtcs_asm_get_named_section (self,decl, ACONCAT ((text_section_name, ".",name, NULL)), 0);
      }else
          return NULL;
  }
  return mtcs_asm_get_named_section (self,decl, text_section_name, 0);
}

section  *mtcs_asm_get_text_section (MtcsAsm *self)
{
    return self->text_section;
}

section       *mtcs_asm_get_data_section (MtcsAsm *self)
{
    return self->data_section;
}

section       *mtcs_asm_get_readonly_data_section (MtcsAsm *self)
{
    return self->readonly_data_section;
}


/* Output assembler code associated with defining the size of the
   function.  DECL describes the function.  NAME is the function's name.  */
//mtcs nouse
void mtcs_asm_assemble_end_function (MtcsAsm *self,tree decl, const char *fnname ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   if(self->declare_function_size){
      /* We could have switched section in the middle of the function.  */
      if (mtcsRtlData/*!crtl*/->has_bb_partition)
         mtcs_asm_switch_to_section (self,mtcs_ams_function_section (self,decl));
      self->declare_function_size/*!ASM_DECLARE_FUNCTION_SIZE (asm_out_file,*/(self,fnname, decl);
   }
   if (! CONSTANT_POOL_BEFORE_FUNCTION){
      outputConstantPool (self,fnname, decl);
      mtcs_asm_switch_to_section (self,mtcs_ams_function_section (self,decl)); /* need to switch back */
   }
   /* Output labels for end of hot/cold text sections (to be used by
   debug info.)  */
   if (mtcsRtlData/*!crtl*/->has_bb_partition){
      section *save_text_section;

      save_text_section = self->in_section;
      mtcs_asm_switch_to_section (self,mtcs_asm_unlikely_text_section (self));
      if(self->declare_cold_function_size/*!#ifdef ASM_DECLARE_COLD_FUNCTION_SIZE*/){
         if (self->cold_function_name != NULL_TREE)
         self->declare_cold_function_size/*!ASM_DECLARE_COLD_FUNCTION_SIZE (asm_out_file,*/(self,
               IDENTIFIER_POINTER (self->cold_function_name), decl);
      }/*!#endif*/
      self->output_label/*!ASM_OUTPUT_LABEL*/(self, mtcsRtlData/*!crtl*/->subsections.cold_section_end_label);
      if (self->first_function_block_is_cold)
         mtcs_asm_switch_to_section (self,self->text_section);
      else
         mtcs_asm_switch_to_section (self,mtcs_ams_function_section (self,decl));
      self->output_label/*!ASM_OUTPUT_LABEL*/(self,mtcsRtlData/*!crtl*/->subsections.hot_section_end_label);
      mtcs_asm_switch_to_section (self,save_text_section);
   }
}

/* Assemble code to leave SIZE bytes of zeros.  */
void mtcs_asm_assemble_zeros (MtcsAsm *self,unsigned HOST_WIDE_INT size)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsMachine  *mtcsMachine =mtcs_target_get_machine(mtcsTarget);

  /* Do no output if -fsyntax-only.  */
    if (flag_syntax_only)
      return;
//#ifdef ASM_NO_SKIP_IN_TEXT host=1 nvptx=0
//  /* The `space' pseudo in the text section outputs nop insns rather than 0s,
//     so we must output 0s explicitly in the text section.  */
//  if (ASM_NO_SKIP_IN_TEXT && (in_section->common.flags & SECTION_CODE) != 0)
//    {
//      unsigned HOST_WIDE_INT i;
//      for (i = 0; i < size; i++)
//   assemble_integer (const0_rtx, 1, BITS_PER_UNIT, 1);
//    }
//  else
//#endif

    if (size > 0)
       target_asm_out_output_skip/*!ASM_OUTPUT_SKIP*/(mtcsMachine->asmOut, size);
}


/* Return an rtx representing a reference to constant data in memory
   for the constant expression EXP.

   If assembler code for such a constant has already been output,
   return an rtx to refer to it.
   Otherwise, output such a constant in memory
   and generate an rtx for it.

   If DEFER is nonzero, this constant can be deferred and output only
   if referenced in the function after all optimizations.

   `const_desc_table' records which constants already have label strings.  */
//原型 output_constant_def rtl.h varasm.cc
rtx mtcs_asm_output_constant_def (MtcsAsm *self,tree exp, int defer)
{
  n_debug("mtcsasm.c mtcs_asm_output_constant_def 00 %s %d\n",get_tree_code_name(TREE_CODE(exp)),defer);
  struct constant_descriptor_tree *desc = add_constant_to_table(self,exp, defer);
  n_debug("mtcsasm.c mtcs_asm_output_constant_def 11 %s %d mode:%d\n",get_tree_code_name(TREE_CODE(exp)),defer,GET_MODE(desc->rtl));

  maybe_output_constant_def_contents(self,desc, defer);
  n_debug("mtcsasm.c mtcs_asm_output_constant_def 22 %s %d mode:%d\n",get_tree_code_name(TREE_CODE(exp)),defer,GET_MODE(desc->rtl));

  return desc->rtl;
}

/* If block symbol SYMBOL has not yet been assigned an offset, place
   it at the end of its block.  */

void mtcs_asm_place_block_symbol (MtcsAsm *self,rtx symbol)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  unsigned HOST_WIDE_INT size, mask, offset;
  class constant_descriptor_rtx *desc;
  unsigned int alignment;
  struct object_block *block;
  tree decl;

  gcc_assert (SYMBOL_REF_BLOCK (symbol));
  if (SYMBOL_REF_BLOCK_OFFSET (symbol) >= 0)
    return;

  /* Work out the symbol's size and alignment.  */
  if (CONSTANT_POOL_ADDRESS_P (symbol)){
      desc = SYMBOL_REF_CONSTANT (symbol);
      alignment = desc->align;
      size = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,desc->mode);
  }else if (TREE_CONSTANT_POOL_ADDRESS_P (symbol)){
      decl = SYMBOL_REF_DECL (symbol);
      gcc_checking_assert (DECL_IN_CONSTANT_POOL (decl));
      alignment = DECL_ALIGN (decl);
      size = getConstantSize (DECL_INITIAL (decl));
      if ((flag_sanitize & SANITIZE_ADDRESS) && TREE_CODE (DECL_INITIAL (decl)) == STRING_CST && asan_protect_global (DECL_INITIAL (decl))){
          size += asan_red_zone_size (size);
          alignment = MAX (alignment,ASAN_RED_ZONE_SIZE * BITS_PER_UNIT);
      }
  }else{
      struct symtab_node *snode;
      decl = SYMBOL_REF_DECL (symbol);
      snode = symtab_node::get (decl);
      if (snode->alias){
          rtx target = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,snode->ultimate_alias_target ()->decl);
          gcc_assert (MEM_P (target) && GET_CODE (XEXP (target, 0)) == SYMBOL_REF && SYMBOL_REF_HAS_BLOCK_INFO_P (XEXP (target, 0)));
          target = XEXP (target, 0);
          mtcs_asm_place_block_symbol (self,target);
          SYMBOL_REF_BLOCK_OFFSET (symbol) = SYMBOL_REF_BLOCK_OFFSET (target);
          return;
      }
      alignment = getVariableAlign (self,decl);
      size = tree_to_uhwi (DECL_SIZE_UNIT (decl));
      if ((flag_sanitize & SANITIZE_ADDRESS)  && asan_protect_global (decl)){
          size += asan_red_zone_size (size);
          alignment = MAX (alignment,ASAN_RED_ZONE_SIZE * BITS_PER_UNIT);
      }
  }

  /* Calculate the object's offset from the start of the block.  */
  block = SYMBOL_REF_BLOCK (symbol);
  mask = alignment / BITS_PER_UNIT - 1;
  offset = (block->size + mask) & ~mask;
  SYMBOL_REF_BLOCK_OFFSET (symbol) = offset;

  /* Record the block's new alignment and size.  */
  block->alignment = MAX (block->alignment, alignment);
  block->size = offset + size;
  vec_safe_push (block->objects, symbol);
}

/* Assemble the floating-point constant D into an object of size MODE.  ALIGN
   is the alignment of the constant in bits.  If REVERSE is true, D is output
   in reverse storage order.  */
//原型 assemble_real output.h varasm.cc
void mtcs_asm_assemble_real (MtcsAsm *self,REAL_VALUE_TYPE d, scalar_float_mode mode, unsigned int align, bool reverse)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);

  long data[4] = {0, 0, 0, 0};
  int bitsize, nelts, nunits, units_per;
  rtx elt;

  /* This is hairy.  We have a quantity of known size.  real_to_target
     will put it into an array of *host* longs, 32 bits per element
     (even if long is more than 32 bits).  We need to determine the
     number of array elements that are occupied (nelts) and the number
     of *target* min-addressable units that will be occupied in the
     object file (nunits).  We cannot assume that 32 divides the
     mode's bitsize (size * BITS_PER_UNIT) evenly.

     size * BITS_PER_UNIT is used here to make sure that padding bits
     (which might appear at either end of the value; real_to_target
     will include the padding bits in its output array) are included.  */

  nunits = mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
  bitsize = nunits * BITS_PER_UNIT;
  nelts = CEIL (bitsize, 32);
  units_per = 32 / BITS_PER_UNIT;

  mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,data, &d, mode);

  /* Put out the first word with the specified alignment.  */
  unsigned int chunk_nunits = MIN (nunits, units_per);
  if (reverse)
    elt = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,mtcsMode->modes.M_SImode,
            mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,data[nelts - 1], mtcsMode->modes.M_SImode));
  else
    elt = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,sext_hwi (data[0], chunk_nunits * BITS_PER_UNIT));
  mtcs_asm_assemble_integer/*!assemble_integer*/(self,elt, chunk_nunits, align, 1);
  nunits -= chunk_nunits;

  /* Subsequent words need only 32-bit alignment.  */
  align = min_align (align, 32);

  for (int i = 1; i < nelts; i++) {
      chunk_nunits = MIN (nunits, units_per);
      if (reverse)
          elt = mtcs_expmed_flip_storage_order/*!flip_storage_order*/(mtcsExpmed,mtcsMode->modes.M_SImode,
                  mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,data[nelts - 1 - i], mtcsMode->modes.M_SImode));
      else
          elt = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,sext_hwi (data[i], chunk_nunits * BITS_PER_UNIT));
      mtcs_asm_assemble_integer/*!assemble_integer*/(self,elt, chunk_nunits, align, 1);
      nunits -= chunk_nunits;
  }
}

/* Compute the alignment of variable specified by DECL.
   DONT_OUTPUT_DATA is from mtcs_assemble_variable.  */
//原型 align_variable output.h varasm.cc
void mtcs_asm_align_variable (MtcsAsm *self,tree decl, bool dont_output_data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  unsigned int align = DECL_ALIGN (decl);
  /* In the case for initialing an array whose length isn't specified,
     where we have not yet been able to do the layout,
     figure out the proper alignment now.  */
  n_debug("mtcsasm.c mtcs_asm_align_variable 00 decl:%p align:%d dont_output_data:%d\n",decl,align,dont_output_data);

  if (dont_output_data && DECL_SIZE (decl) == 0 && TREE_CODE (TREE_TYPE (decl)) == ARRAY_TYPE){
    align = MAX (align, TYPE_ALIGN (TREE_TYPE (TREE_TYPE (decl))));
    n_debug("mtcsasm.c mtcs_asm_align_variable 11 decl:%p align:%d\n",decl,align);
  }

  /* Some object file formats have a maximum alignment which they support.
     In particular, a.out format supports a maximum alignment of 4.  */
  if (align > MAX_OFILE_ALIGNMENT){
      error ("alignment of %q+D is greater than maximum object file alignment %d", decl,MAX_OFILE_ALIGNMENT/BITS_PER_UNIT);
      align = MAX_OFILE_ALIGNMENT;
  }

  if (! DECL_USER_ALIGN (decl)){

      /* On some machines, it is good to increase alignment sometimes.
     But as DECL_ALIGN is used both for actually emitting the variable
     and for code accessing the variable as guaranteed alignment, we
     can only increase the alignment if it is a performance optimization
     if the references to it must bind to the current definition.  */
      if (mtcs_asm_decl_binds_to_current_def_p (self,decl)  && !DECL_VIRTUAL_P (decl)){
         n_debug("mtcsasm.c mtcs_asm_align_variable 22 decl:%p align:%d dont_output_data:%d\n",decl,align,dont_output_data);

          unsigned int data_align =mtcsTarget->data_alignment(mtcsTarget,TREE_TYPE (decl), align);
          /* Don't increase alignment too much for TLS variables - TLS space
             is too precious.  */
          if (! DECL_THREAD_LOCAL_P (decl) || data_align <= BITS_PER_WORD)
            align = data_align;
          if (DECL_INITIAL (decl) != 0
              /* In LTO we have no errors in program; error_mark_node is used
             to mark offlined constructors.  */
              && (in_lto_p || DECL_INITIAL (decl) != error_mark_node)){
             n_debug("mtcsasm.c mtcs_asm_align_variable 33 decl:%p align:%d dont_output_data:%d\n",decl,align,dont_output_data);

              unsigned int const_align = mtcsTarget->constant_alignment (mtcsTarget,DECL_INITIAL (decl), align);
              /* Don't increase alignment too much for TLS variables - TLS
             space is too precious.  */
              if (! DECL_THREAD_LOCAL_P (decl) || const_align <= BITS_PER_WORD)
                  align = const_align;
          }
      }
  }

  /* Reset the alignment in case we have made it tighter, so we can benefit
     from it in get_pointer_alignment.  */
  SET_DECL_ALIGN (decl, align);
}


/* Determine what kind of relocations EXP may need.  */

int mtcs_asm_compute_reloc_for_constant (MtcsAsm *self,tree exp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  int reloc = 0, reloc2;
  tree tem;
  switch (TREE_CODE (exp)){
    case ADDR_EXPR:
    case FDESC_EXPR:
      /* Go inside any operations that get_inner_reference can handle and see
     if what's inside is a constant: no need to do anything here for
     addresses of variables or functions.  */
      for (tem = TREE_OPERAND (exp, 0); handled_component_p (tem); tem = TREE_OPERAND (tem, 0))
          ;

      if (TREE_CODE (tem) == MEM_REF && TREE_CODE (TREE_OPERAND (tem, 0)) == ADDR_EXPR){
          reloc = mtcs_asm_compute_reloc_for_constant (self,TREE_OPERAND (tem, 0));
          break;
      }

      if (!mtcsTarget->binds_local_p (mtcsTarget,tem))
          reloc |= 2;
      else
          reloc |= 1;
      break;

    case PLUS_EXPR:
    case POINTER_PLUS_EXPR:
      reloc = mtcs_asm_compute_reloc_for_constant (self,TREE_OPERAND (exp, 0));
      reloc |= mtcs_asm_compute_reloc_for_constant (self,TREE_OPERAND (exp, 1));
      break;

    case MINUS_EXPR:
      reloc = mtcs_asm_compute_reloc_for_constant (self,TREE_OPERAND (exp, 0));
      reloc2 = mtcs_asm_compute_reloc_for_constant (self,TREE_OPERAND (exp, 1));
      /* The difference of two local labels is computable at link time.  */
      if (reloc == 1 && reloc2 == 1)
          reloc = 0;
      else
          reloc |= reloc2;
      break;

    CASE_CONVERT:
    case VIEW_CONVERT_EXPR:
      reloc = mtcs_asm_compute_reloc_for_constant (self,TREE_OPERAND (exp, 0));
      break;

    case CONSTRUCTOR:
      {
        unsigned HOST_WIDE_INT idx;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (exp), idx, tem)
          if (tem != 0)
            reloc |= mtcs_asm_compute_reloc_for_constant (self,tem);
      }
      break;

    default:
      break;
    }
  return reloc;
}

/* Return true when references to DECL must bind to current definition in
   final executable.

   The condition is usually equivalent to whether the function binds to the
   current module (shared library or executable), that is to binds_local_p.
   We use this fact to avoid need for another target hook and implement
   the logic using binds_local_p and just special cases where
   mtcs_decl_binds_to_current_def_p is stronger than binds_local_p.  In particular
   the weak definitions (that can be overwritten at linktime by other
   definition from different object file) and when resolution info is available
   we simply use the knowledge passed to us by linker plugin.  */
//原型 decl_binds_to_current_def_p varasm.h varasm.cc
bool mtcs_asm_decl_binds_to_current_def_p (MtcsAsm *self,const_tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  gcc_assert (DECL_P (decl));
  if (!mtcsTarget->binds_local_p (mtcsTarget,decl))
    return false;
  if (!TREE_PUBLIC (decl))
    return true;

  /* When resolution is available, just use it.  */
  if (symtab_node *node = symtab_node::get (decl)){
      if (node->resolution != LDPR_UNKNOWN && !node->can_be_discarded_p ())
          return resolution_to_local_definition_p (node->resolution);
  }

  /* Otherwise we have to assume the worst for DECL_WEAK (hidden weaks
     binds locally but still can be overwritten), DECL_COMMON (can be merged
     with a non-common definition somewhere in the same module) or
     DECL_EXTERNAL.
     This rely on fact that binds_local_p behave as mtcs_decl_replaceable_p
     for all other declaration types.  */
  if (DECL_WEAK (decl))
    return false;
  if (DECL_COMMON (decl) && (DECL_INITIAL (decl) == NULL || (!in_lto_p && DECL_INITIAL (decl) == error_mark_node)))
    return false;
  if (DECL_EXTERNAL (decl))
    return false;
  return true;
}

/* DECL is an object (either VAR_DECL or FUNCTION_DECL) which is going
   to be output to assembler.
   Set first_global_object_name and weak_global_object_name as appropriate.  */

void mtcs_asm_notice_global_symbol (MtcsAsm *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  const char **t = &first_global_object_name;
  if (first_global_object_name || !TREE_PUBLIC (decl)  || DECL_EXTERNAL (decl)
      || !DECL_NAME (decl) || (VAR_P (decl) && DECL_HARD_REGISTER (decl))
      || (TREE_CODE (decl) != FUNCTION_DECL && (!VAR_P (decl)  || (DECL_COMMON (decl)
          && (DECL_INITIAL (decl) == 0  || DECL_INITIAL (decl) == error_mark_node)))))
    return;

  /* We win when global object is found, but it is useful to know about weak
     symbol as well so we can produce nicer unique names.  */
  if (DECL_WEAK (decl) || DECL_ONE_ONLY (decl) || flag_shlib)
    t = &weak_global_object_name;

  if (!*t) {
      tree id = DECL_ASSEMBLER_NAME (decl);
      ultimate_transparent_alias_target (&id);
      *t = ggc_strdup (mtcsTarget->strip_name_encoding/*!targetm.strip_name_encoding*/(mtcsTarget,IDENTIFIER_POINTER (id)));
  }
}

/* A helper function to call assemble_visibility when needed for a decl.  */
bool mtcs_asm_maybe_assemble_visibility (MtcsAsm *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   enum symbol_visibility vis = DECL_VISIBILITY (decl);
   if (vis != VISIBILITY_DEFAULT){
      target_asm_out_assemble_visibility/*!targetm.asm_out.assemble_visibility*/(mtcsMachine->asmOut,decl, vis);
      return true;
   }else
      return false;
}

/* Assemble a string constant with the specified C string as contents.  */
//原型 output.h assemble_string varasm.cc
void mtcs_asm_assemble_string (MtcsAsm *self,const char *p, int size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine  *mtcsMachine =mtcs_target_get_machine(mtcsTarget);

   int pos = 0;
   int maximum = 2000;
   /* If the string is very long, split it up.  */
   while (pos < size){
      int thissize = size - pos;
      if (thissize > maximum)
         thissize = maximum;

      target_asm_out_output_ascii/*!ASM_OUTPUT_ASCII*/(mtcsMachine->asmOut, p, thissize);
      pos += thissize;
      p += thissize;
   }
}

/* Return the section into which the given VAR_DECL or CONST_DECL
   should be placed.  PREFER_NOSWITCH_P is true if a noswitch
   section should be used wherever possible.  */
section *mtcs_asm_get_variable_section (MtcsAsm *self,tree decl, bool prefer_noswitch_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   addr_space_t as = ADDR_SPACE_GENERIC;
   int reloc;
   varpool_node *vnode = varpool_node::get (decl);
   n_debug("mtcsasm.c get_variable_section 00 prefer_noswitch_p:%d vnode:%p decl:%p\n",prefer_noswitch_p,vnode,decl);
   if (vnode) {
      vnode = vnode->ultimate_alias_target ();
      decl = vnode->decl;
   }

   if (TREE_TYPE (decl) != error_mark_node){
      n_debug("mtcsasm.c get_variable_section 11 prefer_noswitch_p:%d vnode:%d decl:%p\n",prefer_noswitch_p,vnode,decl);
      as = TYPE_ADDR_SPACE (TREE_TYPE (decl));
   }

   /* We need the constructor to figure out reloc flag.  */
   if (vnode)
      vnode->get_constructor ();

   if (DECL_COMMON (decl)  && !lookup_attribute ("retain", DECL_ATTRIBUTES (decl))){
      /* If the decl has been given an explicit section name, or it resides
      in a non-generic address space, then it isn't common, and shouldn't
      be handled as such.  */
      gcc_assert (DECL_SECTION_NAME (decl) == NULL && ADDR_SPACE_GENERIC_P (as));
      n_debug("mtcsasm.c get_variable_section 22\n");
      if (DECL_THREAD_LOCAL_P (decl)){
         n_debug("mtcsasm.c get_variable_section 33\n");
         return self->tls_comm_section;
      }else if (TREE_PUBLIC (decl) && bss_initializer_p (decl)){
         n_debug("mtcsasm.c get_variable_section 44\n");
         return self->comm_section;
      }
   }
   reloc = compute_reloc_for_var (decl);
   mtcs_asm_resolve_unique_section/*resolve_unique_section*/ (self,decl, reloc, flag_data_sections);
   if (IN_NAMED_SECTION (decl)){
      n_debug("mtcsasm.c get_variable_section 55\n");
      section *sect = mtcs_asm_get_named_section (self,decl, NULL, reloc);
      if ((sect->common.flags & SECTION_BSS) && !bss_initializer_p (decl, true)){
         error_at (DECL_SOURCE_LOCATION (decl),"only zero initializers are allowed in section %qs",sect->named.name);
         DECL_INITIAL (decl) = error_mark_node;
      }
      return sect;
   }

   if (ADDR_SPACE_GENERIC_P (as) && !DECL_THREAD_LOCAL_P (decl) && !DECL_NOINIT_P (decl)  && !(prefer_noswitch_p
   && mtcsTarget->have_switchable_bss_sections)  && bss_initializer_p (decl)){
      n_debug("mtcsasm.c get_variable_section 66\n");

      if (!TREE_PUBLIC (decl)  && !((flag_sanitize & SANITIZE_ADDRESS) && asan_protect_global (decl))){
         n_debug("mtcsasm.c get_variable_section 77 lcomm_section 的回调是 emit_local \n");
         return self->lcomm_section;
      }
      if (self->bss_noswitch_section){
         n_debug("mtcsasm.c get_variable_section 88\n");
         return self->bss_noswitch_section;
      }
   }
   n_debug("mtcsasm.c get_variable_section 99\n");
   return target_asm_out_select_section/*!targetm.asm_out.select_section*/(mtcsMachine->asmOut,
         decl, reloc,get_variable_align (self,decl));
}

void mtcs_asm_output_addressed_constants (MtcsAsm *self,tree exp, int defer)
{
    outputAddressedConstants(self,exp,defer);
}

void mtcs_asm_output_constant_def_contents (MtcsAsm *self,rtx symbol)
{
    outputConstantDefContents(self,symbol);
}

unsigned int mtcs_asm_get_variable_align (MtcsAsm *self,tree decl)
{
    return get_variable_align(self,decl);
}

void mtcs_asm_switch_to_comdat_section (MtcsAsm *self,section *sect, tree decl)
{
    switchToComdatSection(self,sect,decl);
}

unsigned HOST_WIDE_INT mtcs_asm_output_constant (MtcsAsm *self,tree exp, unsigned HOST_WIDE_INT size,
                           unsigned int align,bool reverse, bool merge_strings)
{
    return output_constant(self,exp,size,align,reverse,merge_strings);
}


/* Make sure block symbol SYMBOL is in block BLOCK.  */

static void change_symbol_block (rtx symbol, struct object_block *block)
{
  if (block != SYMBOL_REF_BLOCK (symbol)){
      gcc_assert (SYMBOL_REF_BLOCK_OFFSET (symbol) < 0);
      SYMBOL_REF_BLOCK (symbol) = block;
  }
}

/* Return the block into which object_block DECL should be placed.  */

static struct object_block *get_block_for_decl (MtcsAsm *self,tree decl)
{
  section *sect;
  if (VAR_P (decl)){
      /* The object must be defined in this translation unit.  */
      if (DECL_EXTERNAL (decl))
          return NULL;
      /* There's no point using object blocks for something that is
     isolated by definition.  */
      if (DECL_COMDAT_GROUP (decl))
          return NULL;
  }

  /* We can only calculate block offsets if the decl has a known
     constant size.  */
  if (DECL_SIZE_UNIT (decl) == NULL)
    return NULL;
  if (!tree_fits_uhwi_p (DECL_SIZE_UNIT (decl)))
    return NULL;

  /* Find out which section should contain DECL.  We cannot put it into
     an object block if it requires a standalone definition.  */
  if (VAR_P (decl))
      mtcs_asm_align_variable (self,decl, 0);
  sect = mtcs_asm_get_variable_section (self,decl, true);
  if (SECTION_STYLE (sect) == SECTION_NOSWITCH)
    return NULL;

  if (bool (lookup_attribute ("retain", DECL_ATTRIBUTES (decl))) != bool (sect->common.flags & SECTION_RETAIN))
    return NULL;

  return get_block_for_section(self,sect);
}

/* Return true if it is possible to put DECL in an object_block.  */

static bool use_blocks_for_decl_p (MtcsAsm *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  struct symtab_node *snode;

  /* Don't create object blocks if each DECL is placed into a separate
     section because that will uselessly create a section anchor for
     each DECL.  */
  if (flag_data_sections)
    return false;

  /* Only data DECLs can be placed into object blocks.  */
  if (!VAR_P (decl) && TREE_CODE (decl) != CONST_DECL)
    return false;

  /* DECL_INITIAL (decl) set to decl is a hack used for some decls that
     are never used from code directly and we never want object block handling
     for those.  */
  if (DECL_INITIAL (decl) == decl)
    return false;

  /* If this decl is an alias, then we don't want to emit a
     definition.  */
  if (VAR_P (decl)  && (snode = symtab_node::get (decl)) != NULL && snode->alias)
    return false;

  return mtcsTarget->use_blocks_for_decl_p (mtcsTarget,decl);
}

/* Return true if REGNUM is mentioned in ELIMINABLE_REGS as a from
   register number.  */

static bool eliminable_regno_p (MtcsAsm *self,int regnum)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    //原型 ELIMINABLE_REGS 每个平台设置不一样的ELIMINABLE_REGS
    int i;
    for(i=0;i<mtcsReg->elimiableRegsCount;i++){
        if (regnum == mtcsReg->eliminableRegs[i].from)
             return true;
    }
    return false;
}


/* Create the DECL_RTL for a VAR_DECL or FUNCTION_DECL.  DECL should
   have static storage duration.  In other words, it should not be an
   automatic variable, including PARM_DECLs.

   There is, however, one exception: this function handles variables
   explicitly placed in a particular register by the user.

   This is never called for PARM_DECL nodes.  */
//原型 make_decl_rtl varasm.h varasm.cc
void mtcs_asm_make_decl_rtl (MtcsAsm *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
  MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);

  const char *name = 0;
  int reg_number;
  tree id;
  rtx x;
  /* Check that we are not being given an automatic variable.  */
  gcc_assert (TREE_CODE (decl) != PARM_DECL && TREE_CODE (decl) != RESULT_DECL);
  /* A weak alias has TREE_PUBLIC set but not the other bits.  */
  gcc_assert (!VAR_P (decl) || TREE_STATIC (decl) || TREE_PUBLIC (decl) || DECL_EXTERNAL (decl) || DECL_REGISTER (decl));
  /* And that we were not given a type or a label.  */
  gcc_assert (TREE_CODE (decl) != TYPE_DECL   && TREE_CODE (decl) != LABEL_DECL);

  char *declName=IDENTIFIER_POINTER (DECL_NAME(decl));
  n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 00 %s\n",declName);

  /* For a duplicate declaration, we can be called twice on the
     same DECL node.  Don't discard the RTL already made.  */
  if (DECL_RTL_SET_P (decl)){
     n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 11 %s\n",declName);

      /* If the old RTL had the wrong mode, fix the mode.  */
      x = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl);
      if (GET_MODE (x) != DECL_MODE (decl)){
         n_debug("mtcsasm.c mtcs_asm_make_decl_rtl --11 rtl中的mode与 decl中的mode不符\n");
         mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, mtcs_rtl_adjust_address_nv (mtcsRTL,x, DECL_MODE (decl), 0));
      }
      if (TREE_CODE (decl) != FUNCTION_DECL && DECL_REGISTER (decl))
          return;
      n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 22 %s\n",declName);

      /* ??? Another way to do this would be to maintain a hashed
     table of such critters.  Instead of adding stuff to a DECL
     to give certain attributes to it, we could use an external
     hash map from DECL to set of attributes.  */

      /* Let the target reassign the RTL if it wants.
     This is necessary, for example, when one machine specific
     decl attribute overrides another.  */
      mtcs_output_encode_section_info/*!targetm.encode_section_info*/(mtcsOutput,
            decl, mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl), false);

      /* If the symbol has a SYMBOL_REF_BLOCK field, update it based
     on the new decl information.  */
      if (MEM_P (x) && GET_CODE (XEXP (x, 0)) == SYMBOL_REF && SYMBOL_REF_HAS_BLOCK_INFO_P (XEXP (x, 0)))
          change_symbol_block (XEXP (x, 0), get_block_for_decl (self,decl));

      return;
  }

  /* If this variable belongs to the global constant pool, retrieve the
     pre-computed RTL or recompute it in LTO mode.  */
  if (VAR_P (decl) && DECL_IN_CONSTANT_POOL (decl)){
     n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 33 %s\n",declName);

     mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, mtcs_asm_output_constant_def (self,DECL_INITIAL (decl), 1));
      return;
  }

  id = DECL_ASSEMBLER_NAME (decl);
  name = IDENTIFIER_POINTER (id);
  n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 44 %s\n",name);

  if (name[0] != '*' && TREE_CODE (decl) != FUNCTION_DECL  && DECL_REGISTER (decl)){
      error ("register name not specified for %q+D", decl);
  }else if (TREE_CODE (decl) != FUNCTION_DECL && DECL_REGISTER (decl)){
      const char *asmspec = name+1;
      machine_mode mode = DECL_MODE (decl);
      reg_number = decode_reg_name (asmspec);
      /* First detect errors in declaring global registers.  */
      if (reg_number == -1)
          error ("register name not specified for %q+D", decl);
      else if (reg_number < 0)
          error ("invalid register name for %q+D", decl);
      else if (mode == mtcsMode->modes.M_BLKmode)
          error ("data type of %q+D isn%'t suitable for a register",decl);
      else if (!in_hard_reg_set_p (accessible_reg_set, mode, reg_number))
          error ("the register specified for %q+D cannot be accessed by the current target", decl);
      else if (!in_hard_reg_set_p (operand_reg_set, mode, reg_number))
          error ("the register specified for %q+D is not general enough to be used as a register variable", decl);
      else if (!targetm.hard_regno_mode_ok (reg_number, mode))
          error ("register specified for %q+D isn%'t suitable for data type",decl);
      else if (reg_number != mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(mtcsReg)
              && (reg_number == mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(mtcsReg)
           || reg_number ==mtcs_reg_get_arg_pointer_regnum/*!ARG_POINTER_REGNUM*/(mtcsReg))
              && eliminable_regno_p (self,reg_number))
          error ("register specified for %q+D is an internal GCC implementation detail", decl);
      /* Now handle properly declared static register variables.  */
      else{
          int nregs;
          if (DECL_INITIAL (decl) != 0 && TREE_STATIC (decl)){
              DECL_INITIAL (decl) = 0;
              error ("global register variable has initial value");
          }
          if (TREE_THIS_VOLATILE (decl))
            warning (OPT_Wvolatile_register_var,"optimization may eliminate reads and/or writes to register variables");

          /* If the user specified one of the eliminables registers here,
             e.g., FRAME_POINTER_REGNUM, we don't want to get this variable
             confused with that register and be eliminated.  This usage is
             somewhat suspect...  */
          n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 55 %s\n",name);

          mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, mtcs_rtl_gen_raw_REG/*!gen_raw_REG*/(mtcsRTL,mode, reg_number));
          ORIGINAL_REGNO (mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl)) = reg_number;
          REG_USERVAR_P (mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl)) = 1;

          if (TREE_STATIC (decl)){
              /* Make this register global, so not usable for anything
             else.  */
              nregs = mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(mtcsReg,reg_number, mode);
              while (nregs > 0)
                 mtcs_reg_globalize_reg/*!globalize_reg*/(mtcsReg,decl, reg_number + --nregs);
          }

          /* As a register variable, it has no section.  */
          return;
      }
      /* Avoid internal errors from invalid register
     specifications.  */
      SET_DECL_ASSEMBLER_NAME (decl, NULL_TREE);
      DECL_HARD_REGISTER (decl) = 0;
      /* Also avoid SSA inconsistencies by pretending this is an external
     decl now.  */
      DECL_EXTERNAL (decl) = 1;
      return;
  }
  /* Now handle ordinary static variables and functions (in memory).
     Also handle vars declared register invalidly.  */
  else if (name[0] == '*'){
//#ifdef REGISTER_PREFIX host=0 nvptx=0
//    if (strlen (REGISTER_PREFIX) != 0)
//      {
//    reg_number = decode_reg_name (name);
//    if (reg_number >= 0 || reg_number == -3)
//      error ("register name given for non-register variable %q+D", decl);
//      }
//#endif
  }

  /* Specifying a section attribute on a variable forces it into a
     non-.bss section, and thus it cannot be common.  */
  /* FIXME: In general this code should not be necessary because
     visibility pass is doing the same work.  But notice_global_symbol
     is called early and it needs to make DECL_RTL to get the name.
     we take care of recomputing the DECL_RTL after visibility is changed.  */
  if (VAR_P (decl)   && (TREE_STATIC (decl) || DECL_EXTERNAL (decl))
      && DECL_SECTION_NAME (decl) != NULL  && DECL_INITIAL (decl) == NULL_TREE  && DECL_COMMON (decl))
      DECL_COMMON (decl) = 0;

  /* Variables can't be both common and weak.  */
  if (VAR_P (decl) && DECL_WEAK (decl))
    DECL_COMMON (decl) = 0;

  if (use_object_blocks_p(self) && use_blocks_for_decl_p (self,decl))
    x = create_block_symbol (self,name, get_block_for_decl (self,decl), -1);
  else{
     n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 66 %s\n",name);

      machine_mode address_mode =mtcs_mode_get_Pmode(mtcsMode);
      if (TREE_TYPE (decl) != error_mark_node){
          addr_space_t as = TYPE_ADDR_SPACE (TREE_TYPE (decl));
          address_mode = target_addr_space_address_mode/*!targetm.addr_space.address_mode*/(mtcsMachine->addrSpace,as);
          n_debug("mtcsasm.c mtcs_asm_make_decl_rtl --66 %s %d %s\n",name,address_mode,mtcs_mode_get_name(mtcsMode,address_mode));

      }
      x = gen_rtx_SYMBOL_REF (address_mode, name);
  }
  SYMBOL_REF_WEAK (x) = DECL_WEAK (decl);
  SET_SYMBOL_REF_DECL (x, decl);
  n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 77 生成新的 rtl %s decl:%p %d %s\n",
        name,decl,DECL_MODE(decl),mtcs_mode_get_name(mtcsMode,DECL_MODE(decl)));

  x = gen_rtx_MEM (DECL_MODE (decl), x);
  if (TREE_CODE (decl) != FUNCTION_DECL){
     n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 88 生成新的 rtl %s\n",name);

     mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,x, decl, 1);
  }
  n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 88aa 生成新的 rtl %s\n",name);
  mtcs_print_rtl(stderr,x);
  mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, x);

  /* Optionally set flags or add text to the name to record information
     such as that it is a function name.
     If the name is changed, the macro ASM_OUTPUT_LABELREF
     will have to know how to strip this information.  */
  mtcs_output_encode_section_info/*!targetm.encode_section_info*/(mtcsOutput,
        decl, mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl), true);
  n_debug("mtcsasm.c mtcs_asm_make_decl_rtl 99 结束  %s\n",name);

}


const char *mtcs_asm_get_fnname_from_decl (MtcsAsm *self,tree decl)
{
    rtx x = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl);
    gcc_assert (MEM_P (x));
    x = XEXP (x, 0);
    gcc_assert (GET_CODE (x) == SYMBOL_REF);
    return XSTR (x, 0);
}

/* Worker for resolve_unique_section.  */

static bool set_implicit_section (struct symtab_node *n, void *data ATTRIBUTE_UNUSED)
{
  n->implicit_section = true;
  return false;
}
/* If required, set DECL_SECTION_NAME to a unique name.  */

void mtcs_asm_resolve_unique_section (MtcsAsm *self,tree decl, int reloc ATTRIBUTE_UNUSED,int flag_function_or_data_sections)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   if (DECL_SECTION_NAME (decl) == NULL
   && mtcsTarget->mtcsMachine->common->have_named_sections/*!targetm.targetm_common.have_named_sections*/
   && (flag_function_or_data_sections
   || lookup_attribute ("retain", DECL_ATTRIBUTES (decl))
   || DECL_COMDAT_GROUP (decl))){
      target_asm_out_unique_section/*!targetm.asm_out.unique_section*/(mtcsMachine->asmOut,decl, reloc);
      if (DECL_SECTION_NAME (decl))
         symtab_node::get (decl)->call_for_symbol_and_aliases(set_implicit_section, NULL, true);
   }
}

/* Hash one component of a constant.  */

static hashval_t const_rtx_hash_1 (const_rtx x)
{
  unsigned HOST_WIDE_INT hwi;
  machine_mode mode;
  enum rtx_code code;
  hashval_t h;
  int i;

  code = GET_CODE (x);
  mode = GET_MODE (x);
  h = (hashval_t) code * 1048573 + mode;

  switch (code){
    case CONST_INT:
      hwi = INTVAL (x);

    fold_hwi:
      {
        int shift = sizeof (hashval_t) * CHAR_BIT;
        const int n = sizeof (HOST_WIDE_INT) / sizeof (hashval_t);
        h ^= (hashval_t) hwi;
        for (i = 1; i < n; ++i){
            hwi >>= shift;
            h ^= (hashval_t) hwi;
        }
      }
      break;

    case CONST_WIDE_INT:
      hwi = 0;
      {
        for (i = 0; i < CONST_WIDE_INT_NUNITS (x); i++)
          hwi ^= CONST_WIDE_INT_ELT (x, i);
        goto fold_hwi;
      }

    case CONST_DOUBLE:
      if (TARGET_SUPPORTS_WIDE_INT == 0 && mode == VOIDmode){
          hwi = CONST_DOUBLE_LOW (x) ^ CONST_DOUBLE_HIGH (x);
          goto fold_hwi;
      }else
          h ^= real_hash (CONST_DOUBLE_REAL_VALUE (x));
      break;

    case CONST_FIXED:
      h ^= fixed_hash (CONST_FIXED_VALUE (x));
      break;

    case SYMBOL_REF:
      h ^= htab_hash_string (XSTR (x, 0));
      break;

    case LABEL_REF:
      h = h * 251 + CODE_LABEL_NUMBER (label_ref_label (x));
      break;

    case UNSPEC:
    case UNSPEC_VOLATILE:
      h = h * 251 + XINT (x, 1);
      break;

    default:
      break;
  }

  return h;
}

/* Compute a hash value for X, which should be a constant.  */

static hashval_t const_rtx_hash (rtx x)
{
  hashval_t h = 0;
  subrtx_iterator::array_type array;
  FOR_EACH_SUBRTX (iter, array, x, ALL)
    h = h * 509 + const_rtx_hash_1 (*iter);
  return h;
}

//原型 force_const_mem rtl.h varasm.cc
rtx mtcs_asm_force_const_mem (MtcsAsm *self,machine_mode in_mode, rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  class constant_descriptor_rtx *desc, tmp;
  struct rtx_constant_pool *pool;
  char label[256];
  rtx def, symbol;
  hashval_t hash;
  unsigned int align;
  constant_descriptor_rtx *slot;
  fixed_size_mode mode;

  /* We can't force variable-sized objects to memory.  */
  if (!mtcs_mode_is_a <fixed_size_mode> (mtcsMode,in_mode, &mode))
    return NULL_RTX;

  /* If we're not allowed to drop X into the constant pool, don't.  */
  if (mtcsTarget->cannot_force_const_mem/*!targetm.cannot_force_const_mem*/ (mtcsTarget,mode, x))
    return NULL_RTX;

  /* Record that this function has used a constant pool entry.  */
  mtcsRtlData->uses_const_pool = 1;

  /* Decide which pool to use.  */
  pool = (mtcsTarget->use_blocks_for_constant_p/*!targetm.use_blocks_for_constant_p*/ (mtcsTarget,mode, x)
      ? (struct rtx_constant_pool *)self->shared_constant_pool : mtcsRtlData->varasm.pool);

  /* Lookup the value in the hashtable.  */
  tmp.constant = x;
  tmp.mode = mode;
  hash = const_rtx_hash (x);
  //原型 slot = pool->const_rtx_htab->find_slot_with_hash (&tmp, hash, INSERT);
  slot = n_hash_table_lookup_by_hash(pool->const_rtx_htab,&tmp,hash);
  desc = slot;
  /* If the constant was already present, return its memory.  */
  if (desc)
    return copy_rtx (desc->mem);

  /* Otherwise, create a new descriptor.  */
  desc = ggc_alloc<constant_descriptor_rtx> ();
  slot = desc;
  n_hash_table_insert(pool->const_rtx_htab,desc,desc);

  /* Align the location counter as required by EXP's data type.  */
  machine_mode align_mode = (mode == VOIDmode ? word_mode : mode);
  align = mtcsTarget/*!targetm.static_rtx_alignment*/->static_rtx_alignment(mtcsTarget,align_mode);

  pool->offset += (align / BITS_PER_UNIT) - 1;
  pool->offset &= ~ ((align / BITS_PER_UNIT) - 1);

  desc->next = NULL;
  desc->constant = copy_rtx (tmp.constant);
  desc->offset = pool->offset;
  desc->hash = hash;
  desc->mode = mode;
  desc->align = align;
  desc->labelno = self->const_labelno;
  desc->mark = 0;

  pool->offset += mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
  if (pool->last)
    pool->last->next = desc;
  else
    pool->first = pool->last = desc;
  pool->last = desc;

  /* Create a string containing the label name, in LABEL.  */
  mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,label, "LC", self->const_labelno);
  ++self->const_labelno;

  /* Construct the SYMBOL_REF.  Make sure to mark it as belonging to
     the constants pool.  */
  if (use_object_blocks_p(self)
        && mtcsTarget/*!targetm.use_blocks_for_constant_p*/->use_blocks_for_constant_p(mtcsTarget,mode, x)){
      section *sect = target_asm_out_select_rtx_section/*!argetm.asm_out.select_rtx_section*/(mtcsMachine->asmOut,mode, x, align);
      symbol = create_block_symbol (self,ggc_strdup (label),get_block_for_section(self,sect), -1);
  }else
    symbol = gen_rtx_SYMBOL_REF (mtcs_mode_get_Pmode(mtcsMode), ggc_strdup (label));
  desc->sym = symbol;
  SYMBOL_REF_FLAGS (symbol) |= SYMBOL_FLAG_LOCAL;
  CONSTANT_POOL_ADDRESS_P (symbol) = 1;
  SET_SYMBOL_REF_CONSTANT (symbol, desc);

  /* Construct the MEM.  */
  desc->mem = def = gen_const_mem (mode, symbol);
  mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,def, align);

  /* If we're dropping a label to the constant pool, make sure we
     don't delete it.  */
  if (GET_CODE (x) == LABEL_REF)
    LABEL_PRESERVE_P (XEXP (x, 0)) = 1;

  return copy_rtx (def);
}


/* Return the anchor that should be used to address byte offset OFFSET
   from the first object in BLOCK.  MODEL is the TLS model used
   to access it.  */
//原型 get_section_anchor output.h varasm.cc
rtx mtcs_asm_get_section_anchor (MtcsAsm *self,struct object_block *block, HOST_WIDE_INT offset,enum tls_model model)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  char label[100];
  unsigned int begin, middle, end;
  unsigned HOST_WIDE_INT min_offset, max_offset, range, bias, delta;
  rtx anchor;

  /* Work out the anchor's offset.  Use an offset of 0 for the first
     anchor so that we don't pessimize the case where we take the address
     of a variable at the beginning of the block.  This is particularly
     useful when a block has only one variable assigned to it.

     We try to place anchors RANGE bytes apart, so there can then be
     anchors at +/-RANGE, +/-2 * RANGE, and so on, up to the limits of
     a ptr_mode offset.  With some target settings, the lowest such
     anchor might be out of range for the lowest ptr_mode offset;
     likewise the highest anchor for the highest offset.  Use anchors
     at the extreme ends of the ptr_mode range in such cases.

     All arithmetic uses unsigned integers in order to avoid
     signed overflow.  */
  max_offset = (unsigned HOST_WIDE_INT) mtcsTarget/*!targetm*.*/->max_anchor_offset;
  min_offset = (unsigned HOST_WIDE_INT) mtcsTarget/*!targetm*.*/->min_anchor_offset;
  range = max_offset - min_offset + 1;
  if (range == 0)
    offset = 0;
  else{
      bias = HOST_WIDE_INT_1U << (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,ptr_mode) - 1);
      if (offset < 0){
          delta = -(unsigned HOST_WIDE_INT) offset + max_offset;
          delta -= delta % range;
          if (delta > bias)
            delta = bias;
          offset = (HOST_WIDE_INT) (-delta);
      }else{
          delta = (unsigned HOST_WIDE_INT) offset - min_offset;
          delta -= delta % range;
          if (delta > bias - 1)
            delta = bias - 1;
          offset = (HOST_WIDE_INT) delta;
      }
  }

  /* Do a binary search to see if there's already an anchor we can use.
     Set BEGIN to the new anchor's index if not.  */
  begin = 0;
  end = vec_safe_length (block->anchors);
  while (begin != end){
      middle = (end + begin) / 2;
      anchor = (*block->anchors)[middle];
      if (SYMBOL_REF_BLOCK_OFFSET (anchor) > offset)
          end = middle;
      else if (SYMBOL_REF_BLOCK_OFFSET (anchor) < offset)
          begin = middle + 1;
      else if (SYMBOL_REF_TLS_MODEL (anchor) > model)
          end = middle;
      else if (SYMBOL_REF_TLS_MODEL (anchor) < model)
          begin = middle + 1;
      else
          return anchor;
  }

  /* Create a new anchor with a unique label.  */
  mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(self,label, "LANCHOR", self->anchor_labelno);
  anchor = create_block_symbol (self,ggc_strdup (label), block, offset);
  SYMBOL_REF_FLAGS (anchor) |= SYMBOL_FLAG_LOCAL | SYMBOL_FLAG_ANCHOR;
  SYMBOL_REF_FLAGS (anchor) |= model << SYMBOL_FLAG_TLS_SHIFT;

  /* Insert it at index BEGIN.  */
  vec_safe_insert (block->anchors, begin, anchor);
  return anchor;
}

//原型 init_varasm_once rtl.h varasm.c
void mtcs_asm_init_varasm_once (MtcsAsm *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    self->shared_constant_pool =(npointer)create_constant_pool ();

    if(self->sectionAsmOp.text){// 原型 #ifdef TEXT_SECTION_ASM_OP
        n_debug("mtcsasm.c varasm.cc init_varasm_once 11 %s\n",self->sectionAsmOp.text);
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_CODE,self->sectionAsmOp.text);
        self->text_section = createUnnamedSection(self,SECTION_CODE, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.data){// 原型#ifdef DATA_SECTION_ASM_OP
       // n_debug("mtcsasm.c varasm.cc init_varasm_once 22 %s\n",DATA_SECTION_ASM_OP);
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_WRITE,self->sectionAsmOp.data);
        self->data_section = createUnnamedSection(self,SECTION_WRITE, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.sdata){ //原型 #ifdef SDATA_SECTION_ASM_OP
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_WRITE,self->sectionAsmOp.sdata);
        self->sdata_section = createUnnamedSection(self,SECTION_WRITE, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.readonly_data){ //原型 #ifdef READONLY_DATA_SECTION_ASM_OP
       n_debug("mtcsasm.c varasm.cc init_varasm_once 33 %s\n",self->sectionAsmOp.readonly_data);
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_UNNAMED,self->sectionAsmOp.readonly_data);
        self->readonly_data_section = createUnnamedSection(self,SECTION_UNNAMED, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.ctors){ //原型 #ifdef CTORS_SECTION_ASM_OP
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_UNNAMED,self->sectionAsmOp.ctors);
        self->ctors_section = createUnnamedSection(self,SECTION_UNNAMED, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.dtors){ //原型 #ifdef DTORS_SECTION_ASM_OP
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_UNNAMED,self->sectionAsmOp.dtors);
        self->dtors_section = createUnnamedSection(self,SECTION_UNNAMED, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.bss){ //原型 #ifdef BSS_SECTION_ASM_OP
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_WRITE | SECTION_BSS,self->sectionAsmOp.bss);
        self->dtors_section = createUnnamedSection(self,SECTION_WRITE | SECTION_BSS, createUnnamedSectionCallBack_cb,(const char *)data);
    }

    if(self->sectionAsmOp.sbss){ //原型 #ifdef SBSS_SECTION_ASM_OP
        CreateUnamedSectionCallBackData *data=createUNnamdSection(self,SECTION_WRITE | SECTION_BSS,self->sectionAsmOp.sbss);
        self->dtors_section = createUnnamedSection(self,SECTION_WRITE | SECTION_BSS, createUnnamedSectionCallBack_cb,(const char *)data);
    }


    //n_debug("mtcsasm.c varasm.cc init_varasm_once 44 %s\n",DATA_SECTION_ASM_OP);
    self->tls_comm_section = createNoSwitchSection (SECTION_WRITE | SECTION_BSS | SECTION_COMMON, emit_tls_common);
    self->lcomm_section = createNoSwitchSection (SECTION_WRITE | SECTION_BSS | SECTION_COMMON, emit_local);
    self->comm_section = createNoSwitchSection (SECTION_WRITE | SECTION_BSS| SECTION_COMMON, emit_common);

    if(self->asmOutputAlignedBss){// 原型 #if defined ASM_OUTPUT_ALIGNED_BSS
        self->bss_noswitch_section = createNoSwitchSection (SECTION_WRITE | SECTION_BSS, emit_bss);
    }

    target_asm_out_init_sections/*!targetm.asm_out.init_sections*/(mtcsMachine->asmOut);
   // n_debug("mtcsasm.c varasm.cc init_varasm_once 55 %s\n",DATA_SECTION_ASM_OP);

    if (self->readonly_data_section == NULL)
        self->readonly_data_section = self->text_section;

    if(self->output_external){// 原型#ifdef ASM_OUTPUT_EXTERNAL
        self->pending_assemble_externals_set = new hash_set<tree>;
    }
}

//原型 ASM_OUTPUT_ALIGNED_BSS
void mtcs_asm_set_asm_output_aligned_bss(MtcsAsm *self,nboolean is)
{
    self->asmOutputAlignedBss=is;
}

//原型 default_asm_output_anchor output.h varasm.cc
void mtcs_asm_default_asm_output_anchor (MtcsAsm *self,rtx symbol)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine  *mtcsMachine =mtcs_target_get_machine(mtcsTarget);

  gcc_checking_assert (TARGET_SUPPORTS_ALIASES);
  char buffer[100];
  sprintf (buffer, "*. + " HOST_WIDE_INT_PRINT_DEC,SYMBOL_REF_BLOCK_OFFSET (symbol));
  //ASM_OUTPUT_DEF (asm_out_file, XSTR (symbol, 0), buffer);
  target_asm_out_output_def/*!ASM_OUTPUT_DEF*/(mtcsMachine->asmOut,XSTR (symbol, 0),buffer);
}

/* Initialize constant pool hashing for a new function.  */
//原型 init_varasm_status varasm.h varasm.cc
void mtcs_asm_init_varasm_status (MtcsAsm *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    mtcsRtlData->varasm.pool = create_constant_pool ();
    mtcsRtlData->varasm.deferred_constants = 0;
}

void mtcs_asm_print(MtcsAsm *self)
{
    if(5>3)
       return;

    int seek= fseek(self->asmFile,0,SEEK_SET);
    FILE *ff=fopen(self->asmFileName,"r");
    char brr[4096];
    int len=fread(brr,1,4096,ff);
    brr[len]='\0';
    fclose(ff);
    seek= fseek(self->asmFile,0,SEEK_END);
    printf("汇编输出nvptx: \n%s\n",brr);
}


/* If not using flag_reorder_blocks_and_partition, decide early whether the
   current function goes into the cold section, so that targets can use
   current_function_section during RTL expansion.  DECL describes the
   function.  */
//原型 decide_function_section rtl.h varasm.cc
void mtcs_asm_decide_function_section (MtcsAsm *self,tree decl)
{
   self->first_function_block_is_cold = false;

   if (DECL_SECTION_NAME (decl)){
      struct cgraph_node *node = cgraph_node::get (current_function_decl);
       /* Calls to function_section rely on first_function_block_is_cold
       being accurate.  */
      self->first_function_block_is_cold = (node && node->frequency == NODE_FREQUENCY_UNLIKELY_EXECUTED);
    }

   self->in_cold_section_p = self->first_function_block_is_cold;
}

//原型 #define JUMP_TABLES_IN_TEXT_SECTION 0
int  mtcs_asm_jump_tables_in_text_section (MtcsAsm *self)
{
   if(self->jump_tables_in_text_section)
      return self->jump_tables_in_text_section(self);
   return 0;
}

//原型 #define ASM_PREFERRED_EH_DATA_FORMAT(CODE, GLOBAL) asm_preferred_eh_data_format ((CODE), (GLOBAL))
int mtcs_asm_asm_preferred_eh_data_format (MtcsAsm *self,int code, int global)
{
   if(self->asm_preferred_eh_data_format)
      return self->asm_preferred_eh_data_format(self,code,global);
   //#define ASM_PREFERRED_EH_DATA_FORMAT(CODE,GLOBAL)  DW_EH_PE_absptr
   return DW_EH_PE_absptr;
}

//原型 #define ASM_GENERATE_INTERNAL_LABEL 无缺省实现
void mtcs_asm_generate_internal_label (MtcsAsm *self,char *buffer,char *prefix,int num)
{
   self->generate_internal_label(self,buffer,prefix,num);
}

//原型 #define ASM_COMMENT_START "//"
void  mtcs_asm_set_comment_start (MtcsAsm *self,const char *value)
{
   self->asmCommentStart=n_strdup(value);
}

const char *mtcs_asm_get_comment_start(MtcsAsm *self)
{
   return self->asmCommentStart;
}

//提升到全局的变量，输出汇编字符串
char *mtcs_asm_output_promote_decl(MtcsAsm *self,const_tree decl, const char *name, HOST_WIDE_INT size, unsigned align)
{
    if(self->output_promote_decl)
       return self->output_promote_decl(self,decl,name,size,align);
    return NULL;
}

//原型 static void finalize () toplev.cc
void mtcs_asm_close(MtcsAsm *self)
{
   if (self->asmFile){
      if (ferror (self->asmFile) != 0)
         fatal_error (input_location, "error writing to %s: %m", self->asmFileName);
      if (fclose (self->asmFile) != 0)
         fatal_error (input_location, "error closing %s: %m", self->asmFileName);
      self->asmFile = NULL;
   }
}

//原型 #define ASM_OUTPUT_ALIGN(FILE, POWER)  各平台自定义 nvptx有定义
void mtcs_asm_output_align(MtcsAsm *self,int power)
{
   if(self->output_align)
      self->output_align(self,power);
}

//原型 ASM_OUTPUT_LABEL 缺省ptx实现
void mtcs_asm_output_label(MtcsAsm *self,const char *name)
{
   if(self->output_label)
      self->output_label(self,name);
}

//原型 #ifdef ASM_OUTPUT_DWARF_DELTA
void mtcs_asm_output_dwarf_delta(MtcsAsm *self,int size, const char *lab1, const char *lab2)
{
   if(self->asm_output_dwarf_delta)
      self->asm_output_dwarf_delta(self,size,lab1,lab2);
   else
      gcc_unreachable ();
}

//原型 #ifdef ASM_OUTPUT_DWARF_OFFSET
void mtcs_asm_output_dwarf_offset(MtcsAsm *self,int size, const char *label, HOST_WIDE_INT offset,section *base)
{
   if(self->asm_output_dwarf_offset)
      self->asm_output_dwarf_offset(self,size,label,offset,base);
   else
      gcc_unreachable ();
}

//原型 #define ASM_OUTPUT_DWARF_TABLE_REF rs6000_aix_asm_output_dwarf_table_ref
void mtcs_asm_output_dwarf_table_ref(MtcsAsm *self,char * frame_table_labe)
{
   if(self->asm_output_dwarf_table_ref)
      self->asm_output_dwarf_table_ref(self,frame_table_labe);
   else
      gcc_unreachable ();
}

/* Like make_decl_rtl, but inhibit creation of new alias sets when
   calling make_decl_rtl.  Also, reset DECL_RTL before returning the
   rtl.  */
//原型 make_decl_rtl_for_debug varasm.h varasm.cc
rtx mtcs_asm_make_decl_rtl_for_debug (MtcsAsm *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);//self->mtcsMode;
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   unsigned int save_aliasing_flag;
   rtx rtl;

   if (DECL_RTL_SET_P (decl))
      return DECL_RTL (decl);

   /* Kludge alert!  Somewhere down the call chain, make_decl_rtl will
   call new_alias_set.  If running with -fcompare-debug, sometimes
   we do not want to create alias sets that will throw the alias
   numbers off in the comparison dumps.  So... clearing
   flag_strict_aliasing will keep new_alias_set() from creating a
   new set.  */
   save_aliasing_flag = flag_strict_aliasing;
   flag_strict_aliasing = 0;

   rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(self,decl);
   /* Reset DECL_RTL back, as various parts of the compiler expects
   DECL_RTL set meaning it is actually going to be output.  */
   mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,decl, NULL);

   flag_strict_aliasing = save_aliasing_flag;
   return rtl;
}

/**
 * 把汇编文件存入变量
 */
void mtcs_asm_create_asm_var(MtcsAsm *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   char *name=mtcs_target_get_platform_name(mtcsTarget);
   int isa =mtcs_target_get_isa(mtcsTarget);
   int version =mtcs_target_get_version(mtcsTarget);
   NFile *file=n_file_new(self->asmFileName);
   nuint64 size=n_file_get_length(file);
   char *data=xmalloc((int)size+1);
   FILE *fp=fopen(self->asmFileName,"r");
   size_t rd=fread(data,1,size,fp);
   fclose(fp);
   n_file_unref(file);
   if(rd!=size){
      n_error("读取文件出错 %s size:%d 读取的大小:%d\n",self->asmFileName,(int)size,rd);
   }
   data[rd]='\0';
   //生成的变量名像这样 mtcs_asm_code_cuda_6_5_2962277223
   char *varNameStr = mtcs_tool_create_asm_varname(name,isa,version,in_fnames[0]);
   n_debug("mtcsasm.c mtcs_asm_create_asm_var 汇编代码赋值给变量 源代码:\n %s\n 文件名:%s\n 变量名:%s\n",data,in_fnames[0],varNameStr);
   tree type=build_pointer_type(char_type_node);
   tree string = build_string (strlen (data), data);
   size_t length=strlen(data);
   tree typex = build_array_type (char_type_node,build_index_type (size_int (length)));
   TREE_TYPE(string)=typex;
   tree varName=get_identifier (varNameStr);
   tree first_var = build_decl (0/*!DECL_SOURCE_LOCATION (current_function_decl)*/,
                                       VAR_DECL,varName/*!create_tmp_var_name ("mtcs_asm_var")*/,
                                       typex/*!boolean_type_node*/);
   DECL_ARTIFICIAL (first_var) = 1;
   DECL_IGNORED_P (first_var) = 1;
   TREE_STATIC (first_var) = 1;
   TREE_THIS_VOLATILE (first_var) = 1;
   TREE_USED (first_var) = 1;
   DECL_INITIAL (first_var) = string;
   varpool_node::add (first_var);
   n_free(varNameStr);
   {
      char *aetDump=getenv ("GCC_AET_DUMP");
      if(aetDump && (!strcmp(aetDump,"true") || !strcmp(aetDump,"TRUE"))){
         char *objectPath=aet_mediator_get_object_file(aet_mediator_get(),(AetMediatorUser *)mtcs_compile_get());
         char name[512];
         sprintf(name,"%s_%s_%d_%d.o",objectPath,
               mtcsTarget->platformInfo.name,mtcsTarget->platformInfo.version,mtcsTarget->platformInfo.isa);
         n_debug("mtcsasm.c mtcs_asm_create_asm_var 写入MTCS汇编到文件：%s\n",name);
         FILE *testFile=fopen(name,"w");
         fwrite(data,1,rd,testFile);
         fclose(testFile);
      }
   }
}

