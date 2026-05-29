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
#include "optabs-query.h"
#include "optabs-libfuncs.h"

#include "aet/aetprinttree.h"
#include "mtcslibfuncs.h"
#include "mtcstarget.h"

/* Prefixes for the current version of decimal floating point (BID vs. DPD) */
//#if ENABLE_DECIMAL_BID_FORMAT //host =1 nvptx =0
//#define DECIMAL_PREFIX "bid_"
//#else
#define DECIMAL_PREFIX "dpd_"
//#endif

/* Information about an optab-related libfunc.  The op field is logically
   an enum optab_d, and the mode fields are logically machine_mode.
   However, in the absence of forward-declared enums, there's no practical
   benefit of pulling in the defining headers.

   We use the same hashtable for normal optabs and conversion optabs.  In
   the first case mode2 is forced to VOIDmode.  */
//原型 libfunc_entry libfuncs.h
typedef struct _MtcsLibfuncEntry {
  int op, mode1, mode2;
  rtx libfunc;
  MtcsLibfuncs *mtcsLibfuncs;
}MtcsLibfuncEntry;


//原型 hashval_t libfunc_hasher::hash (libfunc_entry *e) optabs-libfuncs.cc
static nuint mtcsLibfuncsEntryHash_cb(nconstpointer v)
{
  MtcsLibfuncEntry *e=(MtcsLibfuncEntry *)v;
  MtcsLibfuncs *self=e->mtcsLibfuncs;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  int numMachineModes=mtcs_mode_get_number(mtcsMode);
  return ((e->mode1 + e->mode2 * numMachineModes) ^ e->op);
}

//原型 bool libfunc_hasher::equal (libfunc_entry *e1, libfunc_entry *e2) optabs-libfuncs.cc
static nboolean mtcsLibfuncsEntryHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    MtcsLibfuncEntry *e1=(MtcsLibfuncEntry *)v1;
    MtcsLibfuncEntry *e2=(MtcsLibfuncEntry *)v2;
    return e1->op == e2->op && e1->mode1 == e2->mode1 && e1->mode2 == e2->mode2;
}

//原型 struct libfunc_decl_hasher : ggc_ptr_hash<tree_node> optabs-libfuncs.cc
static nuint libfuncDeclHash_cb(nconstpointer v)
{
    tree entry=(tree)v;
    return IDENTIFIER_HASH_VALUE (DECL_NAME (entry));
}
//原型 struct libfunc_decl_hasher : ggc_ptr_hash<tree_node> optabs-libfuncs.cc
static nboolean libfuncDeclHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    tree decl=(tree)v1;
    tree name=(tree)v2;
    return DECL_NAME (decl) == name;
}

static void mtcsLibfuncsInit(MtcsLibfuncs *self)
{
   self->x_libfunc_hash=NULL;
   self->libfunc_decls=NULL;
}

void mtcs_libfuncs_set_normalib_def(MtcsLibfuncs *self,struct mtcs_optab_libcall_d *normlib_def)
{
   self->normlib_def = normlib_def;
}

void mtcs_libfuncs_set_convlib_def(MtcsLibfuncs *self,struct mtcs_convert_optab_libcall_d *convlib_def)
{
   self->convlib_def = convlib_def;

}

/* Build a decl for a libfunc named NAME with visibility VIS.  */
//原型 build_libfunc_function_visibility optabs-libfuncs.h optabs-libfuncs.cc
tree mtcs_libfuncs_build_libfunc_function_visibility (MtcsLibfuncs *self,const char *name, symbol_visibility vis)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   /* ??? We don't have any type information; pretend this is "int foo ()".  */
   n_debug("mtcslibfuncs.c mtcs_libfuncs_build_libfunc_function_visibility 00 %s\n",name);
   tree type=mtcs_tree_build_function_type/*!build_function_type*/(mtcsTree,mtcs_integer_type_node, NULL_TREE);
   n_debug("mtcslibfuncs.c  mtcs_libfuncs_build_libfunc_function_visibility 11 %d\n",TYPE_MODE(type));

   tree decl =mtcs_tree_build_decl/*!build_decl*/(mtcsTree,UNKNOWN_LOCATION, FUNCTION_DECL,
   get_identifier (name),type);
   n_debug("mtcslibfuncs.c mtcs_libfuncs_build_libfunc_function_visibility 22 %s %d\n",name,DECL_MODE(decl));
   DECL_EXTERNAL (decl) = 1;
   TREE_PUBLIC (decl) = 1;
   DECL_ARTIFICIAL (decl) = 1;
   DECL_VISIBILITY (decl) = vis;
   DECL_VISIBILITY_SPECIFIED (decl) = 1;
   gcc_assert (DECL_ASSEMBLER_NAME (decl));
   return decl;
}

/* Return a libfunc for NAME, creating one if we don't already have one.
   The decl is given visibility VIS.  The returned rtx is a SYMBOL_REF.  */
//原型 init_one_libfunc_visibility optabs-libfuncs.h  optabs-libfuncs.cc
rtx mtcs_libfuncs_init_one_libfunc_visibility (MtcsLibfuncs *self,const char *name, symbol_visibility vis)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   tree id, decl;
   hashval_t hash;
   if (self->libfunc_decls == NULL)
      self->libfunc_decls = n_hash_table_new_full(libfuncDeclHash_cb, libfuncDeclHashEqual_cb,NULL, NULL);

   /* See if we have already created a libfunc decl for this function.  */
   id = get_identifier (name);
   hash = IDENTIFIER_HASH_VALUE (id);
   /*!tree *slot = libfunc_decls->find_slot_with_hash (id, hash, INSERT);*/
   tree slot=n_hash_table_lookup_by_hash(self->libfunc_decls,id,hash);
   decl = slot;
   if (decl == NULL){
      /* Create a new decl, so that it can be passed to
      targetm.encode_section_info.  */
      n_debug("mtcslibfuncs.c mtcs_libfuncs_init_one_libfunc_visibility %s\n",name);
      decl = mtcs_libfuncs_build_libfunc_function_visibility/*!build_libfunc_function_visibility*/(self,name, vis);
      slot = decl;
      n_hash_table_insert(self->libfunc_decls,slot,slot);
   }
   return XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl), 0);
}


//原型 init_one_libfunc optabs-libfuncs.h  optabs-libfuncs.cc
rtx mtcs_libfuncs_init_one_libfunc (MtcsLibfuncs *self,const char *name)
{
  return mtcs_libfuncs_init_one_libfunc_visibility/*!init_one_libfunc_visibility*/(self,name, VISIBILITY_DEFAULT);
}

/* Call this to reset the function entry for one optab (OPTABLE) in mode
   MODE to NAME, which should be either 0 or a string constant.  */
//原型 set_optab_libfunc optabs-libfuncs.h  optabs-libfuncs.cc
void mtcs_libfuncs_set_optab_libfunc (MtcsLibfuncs *self,optab op, machine_mode mode, const char *name)
{
  rtx val;
  /*!struct libfunc_entry e;
  struct libfunc_entry **slot;*/
  MtcsLibfuncEntry e;
  MtcsLibfuncEntry *slot;
  e.op = op;
  e.mode1 = mode;
  e.mode2 = VOIDmode;
  e.mtcsLibfuncs=self;

  if (name)
    val = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(self,name);
  else
    val = 0;
  slot=n_hash_table_lookup(self->x_libfunc_hash,&e);/*!  slot = libfunc_hash->find_slot (&e, INSERT);*/
  nboolean insert=FALSE;
  if (slot == NULL){
    slot = n_slice_alloc0 (sizeof(MtcsLibfuncEntry));/*!ggc_alloc<libfunc_entry> ();*/
    insert=TRUE;
  }
  slot->op = op;
  slot->mode1 = mode;
  slot->mode2 = VOIDmode;
  slot->libfunc = val;
  slot->mtcsLibfuncs=self;
  if(insert)
      n_hash_table_insert(self->x_libfunc_hash,slot,slot);
}

/* Call this to reset the function entry for one conversion optab
   (OPTABLE) from mode FMODE to mode TMODE to NAME, which should be
   either 0 or a string constant.  */
//原型 set_conv_libfunc optabs-libfuncs.h  optabs-libfuncs.cc
void mtcs_libfuncs_set_conv_libfunc (MtcsLibfuncs *self,convert_optab optab, machine_mode tmode,
        machine_mode fmode, const char *name)
{
  rtx val;
 // struct libfunc_entry e;
 // struct libfunc_entry **slot;
  MtcsLibfuncEntry e;
  MtcsLibfuncEntry *slot;

  e.op = optab;
  e.mode1 = tmode;
  e.mode2 = fmode;
  e.mtcsLibfuncs=self;

  if (name)
    val = mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(self,name);
  else
    val = 0;
  slot=n_hash_table_lookup(self->x_libfunc_hash,&e);/*!  slot = libfunc_hash->find_slot (&e, INSERT);*/
  nboolean insert=FALSE;
  if (slot == NULL){
     slot = n_slice_alloc0 (sizeof(MtcsLibfuncEntry));/*!ggc_alloc<libfunc_entry> ();*/
     insert=TRUE;
  }
  slot->op = optab;
  slot->mode1 = tmode;
  slot->mode2 = fmode;
  slot->libfunc = val;
  slot->mtcsLibfuncs=self;
  if(insert)
      n_hash_table_insert(self->x_libfunc_hash,slot,slot);
}

/* Call this to initialize the contents of the optabs
   appropriately for the current target machine.  */
//原型 init_optabs optabs-libfuncs.h optabs-libfuncs.cc
//#define libfunc_hash   (this_target_libfuncs->x_libfunc_hash) libfuncs.h 和 optabs-libfuncs.cc
void mtcs_libfuncs_init_optabs (MtcsLibfuncs *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  if (self->x_libfunc_hash/*!libfunc_hash*/)
     n_hash_table_remove_all(self->x_libfunc_hash);/*!libfunc_hash->empty ();*/
  else
      /*!libfunc_hash = hash_table<libfunc_hasher>::create_ggc (10);*/
     self->x_libfunc_hash = n_hash_table_new_full(mtcsLibfuncsEntryHash_cb, mtcsLibfuncsEntryHashEqual_cb,NULL, NULL);

  /* Fill in the optabs with the insns we support.  */
  mtcs_opinit_init_all_optabs(mtcsOpinit,mtcsOpinit->this_fn_optabs)/*init_all_optabs (this_fn_optabs)*/;

  /* The ffs function operates on `int'.  Fall back on it if we do not
     have a libgcc2 function for that width.  */
  if (INT_TYPE_SIZE < BITS_PER_WORD){
      scalar_int_mode mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,INT_TYPE_SIZE, 0).require ();
      n_debug("mtcslibfuncs.c mtcs_libfuncs_init_optabs 00022 %d %s INT_TYPE_SIZE:%d\n",
            mode,mtcs_mode_get_name(mtcsMode,mode),INT_TYPE_SIZE);
      mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,ffs_optab, mode, "ffs");
  }
  /* Explicitly initialize the bswap libfuncs since we need them to be
     valid for things other than word_mode.  */
  if (mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix){
      mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,bswap_optab, mtcsMode->modes.M_SImode, "__gnu_bswapsi2");
      mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,bswap_optab,  mtcsMode->modes.M_DImode, "__gnu_bswapdi2");
  }else{
      mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,bswap_optab,  mtcsMode->modes.M_SImode, "__bswapsi2");
      mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,bswap_optab,  mtcsMode->modes.M_DImode, "__bswapdi2");
  }
  /* Use cabs for double complex abs, since systems generally have cabs.
     Don't define any libcall for float complex, so that cabs will be used.  */

  if (mtcs_complex_double_type_node)
      mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,abs_optab, TYPE_MODE (mtcs_complex_double_type_node),"cabs");
  //原型 #define unwind_sjlj_register_libfunc (libfunc_table[LTI_unwind_sjlj_register]) libfuncs.h
  self->x_libfunc_table[LTI_unwind_sjlj_register]/*!unwind_sjlj_register_libfunc*/ =
          mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(self,"_Unwind_SjLj_Register");
  //原型 #define unwind_sjlj_unregister_libfunc (libfunc_table[LTI_unwind_sjlj_unregister]) libfuncs.h
  self->x_libfunc_table[LTI_unwind_sjlj_unregister]/*!unwind_sjlj_unregister_libfunc*/=
          mtcs_libfuncs_init_one_libfunc/*!init_one_libfunc*/(self,"_Unwind_SjLj_Unregister");

  /* Allow the target to add more libcalls or rename some, etc.  */
  mtcsTarget/*!targetm.init_libfuncs*/->init_libfuncs(mtcsTarget);
}


/* Return libfunc corresponding operation defined by OPTAB in MODE.
   Trigger lazy initialization if needed, return NULL if no libfunc is
   available.  */
//原型  optab_libfunc optabs-libfuncs.h optabs-libfuncs.cc
rtx mtcs_libfuncs_optab_libfunc (MtcsLibfuncs *self,optab optab, machine_mode mode)
{
  //struct libfunc_entry e;
  //struct libfunc_entry **slot;

  MtcsLibfuncEntry e;
  MtcsLibfuncEntry *slot;

  /* ??? This ought to be an assert, but not all of the places
     that we expand optabs know about the optabs that got moved
     to being direct.  */
  if (!(optab >= FIRST_NORM_OPTAB && optab <= LAST_NORMLIB_OPTAB))
    return NULL_RTX;

  e.op = optab;
  e.mode1 = mode;
  e.mode2 = VOIDmode;
  e.mtcsLibfuncs = self;
  slot=n_hash_table_lookup(self->x_libfunc_hash,&e);/*!  slot = libfunc_hash->find_slot (&e, NO_INSERT);*/
  if (!slot){
      const struct mtcs_optab_libcall_d *d = &self->normlib_def[optab - FIRST_NORM_OPTAB];

      if (d->libcall_gen == NULL)
         return NULL;

      d->libcall_gen (self,optab, d->libcall_basename, d->libcall_suffix, mode);
      slot=n_hash_table_lookup(self->x_libfunc_hash,&e);/*! slot = self->libfunc_hash->find_slot (&e, NO_INSERT);*/
      if (!slot)
         return NULL;
  }
  return slot->libfunc;
}

/* Return libfunc corresponding operation defined by OPTAB converting
   from MODE2 to MODE1.  Trigger lazy initialization if needed, return NULL
   if no libfunc is available.  */
//原型 convert_optab_libfunc optabs-libfuncs.h optabs-libfuncs.cc
rtx mtcs_libfuncs_convert_optab_libfunc(MtcsLibfuncs *self,convert_optab optab, machine_mode mode1,
               machine_mode mode2)
{
  //struct libfunc_entry e;
  //struct libfunc_entry **slot;
  MtcsLibfuncEntry e;
  MtcsLibfuncEntry *slot;
  /* ??? This ought to be an assert, but not all of the places
     that we expand optabs know about the optabs that got moved
     to being direct.  */
  if (!(optab >= FIRST_CONV_OPTAB && optab <= LAST_CONVLIB_OPTAB))
    return NULL_RTX;

  e.op = optab;
  e.mode1 = mode1;
  e.mode2 = mode2;
  e.mtcsLibfuncs = self;
  slot=n_hash_table_lookup(self->x_libfunc_hash,&e);/*!  slot = libfunc_hash->find_slot (&e, NO_INSERT);*/
  if (!slot){
      const struct mtcs_convert_optab_libcall_d *d = &self->convlib_def[optab - FIRST_CONV_OPTAB];
      n_debug("mtcslibfuncs.c mtcs_libfuncs_convert_optab_libfunc 00 %s %p\n",d->libcall_basename,d->libcall_gen);
      if (d->libcall_gen == NULL)
          return NULL;

      d->libcall_gen (self,optab, d->libcall_basename, mode1, mode2);
      slot=n_hash_table_lookup(self->x_libfunc_hash,&e);/*! slot = self->libfunc_hash->find_slot (&e, NO_INSERT);*/

      if (!slot)
         return NULL;
  }
  return slot->libfunc;
}


MtcsLibfuncs *mtcs_libfuncs_new(MtcsMode *mtcsMode)
{
     MtcsLibfuncs *self = n_slice_alloc0 (sizeof(MtcsLibfuncs));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcsLibfuncsInit(self);
     return self;
}

/////////////////////optabs.def 声明的函数----------------

/* Initialize the libfunc fields of an entire group of entries in some
   optab.  Each entry is set equal to a string consisting of a leading
   pair of underscores followed by a generic operation name followed by
   a mode name (downshifted to lowercase) followed by a single character
   representing the number of operands for the given operation (which is
   usually one of the characters '2', '3', or '4').

   OPTABLE is the table in which libfunc fields are to be initialized.
   OPNAME is the generic (string) name of the operation.
   SUFFIX is the character which specifies the number of operands for
     the given generic operation.
   MODE is the mode to generate for.  */
//原型 gen_libfunc optabs-libfuncs.cc
static void gen_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, int suffix,
        machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   unsigned opname_len = strlen (opname);
   const char *mname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,mode);
   unsigned mname_len = strlen (mname);
   int prefix_len = mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix ? 6 : 2;
   int len = prefix_len + opname_len + mname_len + 1 + 1;
   char *libfunc_name = XALLOCAVEC (char, len);
   char *p;
   const char *q;

   p = libfunc_name;
   *p++ = '_';
   *p++ = '_';
   if (mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix){
      *p++ = 'g';
      *p++ = 'n';
      *p++ = 'u';
      *p++ = '_';
   }
   for (q = opname; *q;)
      *p++ = *q++;
   for (q = mname; *q; q++)
      *p++ = TOLOWER (*q);
   *p++ = suffix;
   *p = '\0';

   mtcs_libfuncs_set_optab_libfunc/*!set_optab_libfunc*/(self,optable, mode,
         ggc_alloc_string (libfunc_name, p - libfunc_name));
}

/* Like gen_libfunc, but verify that integer operation is involved.  */
//原型 gen_int_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
       machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   int maxsize = 2 * BITS_PER_WORD;
   int minsize = BITS_PER_WORD;
   scalar_int_mode int_mode;

   if (!mtcs_mode_is_int_mode/*!is_int_mode*/(mtcsMode,mode, &int_mode))
      return;
   if (maxsize < LONG_LONG_TYPE_SIZE)
   maxsize = LONG_LONG_TYPE_SIZE;
   if (minsize > INT_TYPE_SIZE  && (trapv_binoptab_p (optable) || trapv_unoptab_p (optable)))
      minsize = INT_TYPE_SIZE;
   if (mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_mode) < minsize
   || mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,int_mode) > maxsize)
      return;
   gen_libfunc(self,optable, opname, suffix, int_mode);
}

/* Like gen_libfunc, but verify that FP and set decimal prefix if needed.  */
//原型 gen_fp_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fp_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
      machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   char *dec_opname;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_FLOAT)
      gen_libfunc(self,optable, opname, suffix, mode);
   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode)){
      dec_opname = XALLOCAVEC (char, sizeof (DECIMAL_PREFIX) + strlen (opname));
      /* For BID support, change the name to have either a bid_ or dpd_ prefix
      depending on the low level floating format used.  */
      memcpy (dec_opname, DECIMAL_PREFIX, sizeof (DECIMAL_PREFIX) - 1);
      strcpy (dec_opname + sizeof (DECIMAL_PREFIX) - 1, opname);
      gen_libfunc(self,optable, dec_opname, suffix, mode);
   }
}

/* Like gen_libfunc, but verify that fixed-point operation is involved.  */
//原型 gen_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
         machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (!mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      return;
   gen_libfunc(self,optable, opname, suffix, mode);
}

/* Like gen_libfunc, but verify that signed fixed-point operation is
   involved.  */
//原型 gen_signed_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_signed_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
           machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

  if (!mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode))
    return;
  gen_libfunc(self,optable, opname, suffix, mode);
}

/* Like gen_libfunc, but verify that unsigned fixed-point operation is
   involved.  */
//原型 gen_unsigned_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_unsigned_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
             machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (!mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      return;
   gen_libfunc(self,optable, opname, suffix, mode);
}

/* Like gen_libfunc, but verify that FP or INT operation is involved.  */
//原型 gen_int_fp_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fp_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
          machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode) ||
         mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_FLOAT)
      mtcs_libfuncs_gen_fp_libfunc/*!gen_fp_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, name, suffix, mode);
}

/* Like gen_libfunc, but verify that FP or INT operation is involved
   and add 'v' suffix for integer operation.  */
//原型 gen_intv_fp_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_intv_fp_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
           machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode) ||
            mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_FLOAT)
      mtcs_libfuncs_gen_fp_libfunc/*!gen_fp_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_INT){
      int len = strlen (name);
      char *v_name = XALLOCAVEC (char, len + 2);
      strcpy (v_name, name);
      v_name[len] = 'v';
      v_name[len + 1] = 0;
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, v_name, suffix, mode);
   }
}

/* Like gen_libfunc, but verify that FP or INT or FIXED operation is
   involved.  */
//原型 gen_int_fp_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fp_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
           machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode) ||
         mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_FLOAT)
      mtcs_libfuncs_gen_fp_libfunc/*!gen_fp_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_fixed_libfunc/*!gen_fixed_libfunc*/(self,optable, name, suffix, mode);
}

/* Like gen_libfunc, but verify that FP or INT or signed FIXED operation is
   involved.  */
//原型 gen_int_fp_signed_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fp_signed_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
             machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,mode) ||
         mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,mode) == MODE_FLOAT)
      mtcs_libfuncs_gen_fp_libfunc/*!gen_fp_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_signed_fixed_libfunc/*!gen_signed_fixed_libfunc*/(self,optable, name, suffix, mode);
}

/* Like gen_libfunc, but verify that INT or FIXED operation is
   involved.  */
//原型 gen_int_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
             machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_fixed_libfunc/*!gen_fixed_libfunc*/(self,optable, name, suffix, mode);
}

/* Like gen_libfunc, but verify that INT or signed FIXED operation is
   involved.  */
//原型 gen_int_signed_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_signed_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
               machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_signed_fixed_point_p/*!SIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_signed_fixed_libfunc/*!gen_signed_fixed_libfunc*/(self,optable, name, suffix, mode);
}

/* Like gen_libfunc, but verify that INT or unsigned FIXED operation is
   involved.  */
//原型 gen_int_unsigned_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_unsigned_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
            machine_mode mode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_integral_p/*!INTEGRAL_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_int_libfunc/*!gen_int_libfunc*/(self,optable, name, suffix, mode);
   if (mtcs_mode_is_unsigned_fixed_point_p/*!UNSIGNED_FIXED_POINT_MODE_P*/(mtcsMode,mode))
      mtcs_libfuncs_gen_unsigned_fixed_libfunc/*!gen_unsigned_fixed_libfunc*/(self,optable, name, suffix, mode);
}

/* Initialize the libfunc fields of an entire group of entries of an
   inter-mode-class conversion optab.  The string formation rules are
   similar to the ones for init_libfuncs, above, but instead of having
   a mode name and an operand count these functions have two mode names
   and no operand count.  */
//原型 gen_interclass_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_interclass_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
              const char *opname,
              machine_mode tmode,
              machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   size_t opname_len = strlen (opname);
   size_t mname_len = 0;

   const char *fname, *tname;
   const char *q;
   int prefix_len = mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix ? 6 : 2;
   char *libfunc_name, *suffix;
   char *nondec_name, *dec_name, *nondec_suffix, *dec_suffix;
   char *p;
   n_debug("mtcslibfuncs.c mtcs_libfuncs_gen_interclass_conv_libfunc %s\n",opname);

   /* If this is a decimal conversion, add the current BID vs. DPD prefix that
   depends on which underlying decimal floating point format is used.  */
   const size_t dec_len = sizeof (DECIMAL_PREFIX) - 1;

   mname_len = strlen (mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,tmode))
         + strlen (mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,fmode));

   nondec_name = XALLOCAVEC (char, prefix_len + opname_len + mname_len + 1 + 1);
   nondec_name[0] = '_';
   nondec_name[1] = '_';
   if (mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix){
      nondec_name[2] = 'g';
      nondec_name[3] = 'n';
      nondec_name[4] = 'u';
      nondec_name[5] = '_';
   }

   memcpy (&nondec_name[prefix_len], opname, opname_len);
   nondec_suffix = nondec_name + opname_len + prefix_len;

   dec_name = XALLOCAVEC (char, 2 + dec_len + opname_len + mname_len + 1 + 1);
   dec_name[0] = '_';
   dec_name[1] = '_';
   memcpy (&dec_name[2], DECIMAL_PREFIX, dec_len);
   memcpy (&dec_name[2+dec_len], opname, opname_len);
   dec_suffix = dec_name + dec_len + opname_len + 2;

   fname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,fmode);
   tname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,tmode);

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,fmode) ||
            mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,tmode)){
      libfunc_name = dec_name;
      suffix = dec_suffix;
   }else{
      libfunc_name = nondec_name;
      suffix = nondec_suffix;
   }

   p = suffix;
   for (q = fname; *q; p++, q++)
      *p = TOLOWER (*q);
   for (q = tname; *q; p++, q++)
      *p = TOLOWER (*q);

   *p = '\0';

   mtcs_libfuncs_set_conv_libfunc/*!set_conv_libfunc*/(self,tab, tmode, fmode,
         ggc_alloc_string (libfunc_name, p - libfunc_name));
}

/* Same as gen_interclass_conv_libfunc but verify that we are producing
   int->fp conversion.  */
//原型 gen_int_to_fp_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_to_fp_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
             const char *opname,
             machine_mode tmode,
             machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode) != MODE_INT)
      return;
   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode) != MODE_FLOAT &&
         !mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,tmode))
      return;
   mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* ufloat_optab is special by using floatun for FP and floatuns decimal fp
   naming scheme.  */
//原型 gen_ufloat_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_ufloat_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
          const char *opname ATTRIBUTE_UNUSED,machine_mode tmode, machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,tmode))
      mtcs_libfuncs_gen_int_to_fp_conv_libfunc/*!gen_int_to_fp_conv_libfunc*/(self,tab, "floatuns", tmode, fmode);
   else
      mtcs_libfuncs_gen_int_to_fp_conv_libfunc/*!gen_int_to_fp_conv_libfunc*/(self,tab, "floatun", tmode, fmode);
}

/* Same as gen_interclass_conv_libfunc but verify that we are producing
   fp->int conversion.  */
//原型 gen_int_to_fp_nondecimal_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_to_fp_nondecimal_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
                   const char *opname,
                   machine_mode tmode,
                   machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode) != MODE_INT)
      return;
   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode) != MODE_FLOAT)
      return;
   mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* Same as gen_interclass_conv_libfunc but verify that we are producing
   fp->int conversion with no decimal floating point involved.  */
//原型 gen_fp_to_int_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fp_to_int_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
             const char *opname,
             machine_mode tmode,
             machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode) != MODE_FLOAT &&
         !mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,fmode))
      return;
   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode) != MODE_INT)
      return;
   mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* Initialize the libfunc fields of an of an intra-mode-class conversion optab.
   The string formation rules are
   similar to the ones for init_libfunc, above.  */
//原型 gen_intraclass_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_intraclass_conv_libfunc (MtcsLibfuncs *self,convert_optab tab, const char *opname,
              machine_mode tmode, machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   size_t opname_len = strlen (opname);
   size_t mname_len = 0;

   const char *fname, *tname;
   const char *q;
   int prefix_len = mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix ? 6 : 2;
   char *nondec_name, *dec_name, *nondec_suffix, *dec_suffix;
   char *libfunc_name, *suffix;
   char *p;

   /* If this is a decimal conversion, add the current BID vs. DPD prefix that
   depends on which underlying decimal floating point format is used.  */
   const size_t dec_len = sizeof (DECIMAL_PREFIX) - 1;

   mname_len = strlen (mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,tmode)) + strlen (mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,fmode));

   nondec_name = XALLOCAVEC (char, 2 + opname_len + mname_len + 1 + 1);
   nondec_name[0] = '_';
   nondec_name[1] = '_';
   if (mtcsTarget/*!targetm.libfunc_gnu_prefix*/->libfunc_gnu_prefix){
      nondec_name[2] = 'g';
      nondec_name[3] = 'n';
      nondec_name[4] = 'u';
      nondec_name[5] = '_';
   }
   memcpy (&nondec_name[prefix_len], opname, opname_len);
   nondec_suffix = nondec_name + opname_len + prefix_len;

   dec_name = XALLOCAVEC (char, 2 + dec_len + opname_len + mname_len + 1 + 1);
   dec_name[0] = '_';
   dec_name[1] = '_';
   memcpy (&dec_name[2], DECIMAL_PREFIX, dec_len);
   memcpy (&dec_name[2 + dec_len], opname, opname_len);
   dec_suffix = dec_name + dec_len + opname_len + 2;

   fname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,fmode);
   tname = mtcs_mode_get_name/*!GET_MODE_NAME*/(mtcsMode,tmode);

   if (mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,fmode) ||
   mtcs_mode_is_decimal_float_p/*!DECIMAL_FLOAT_MODE_P*/(mtcsMode,tmode)){
      libfunc_name = dec_name;
      suffix = dec_suffix;
   }else{
      libfunc_name = nondec_name;
      suffix = nondec_suffix;
   }

   p = suffix;
   for (q = fname; *q; p++, q++)
      *p = TOLOWER (*q);
   for (q = tname; *q; p++, q++)
      *p = TOLOWER (*q);

   *p++ = '2';
   *p = '\0';

   mtcs_libfuncs_set_conv_libfunc/*!set_conv_libfunc*/(self,tab, tmode, fmode,
   ggc_alloc_string (libfunc_name, p - libfunc_name));
}

/* Pick proper libcall for trunc_optab.  We need to chose if we do
   truncation or extension and interclass or intraclass.  */
//原型 gen_trunc_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_trunc_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
         const char *opname,
         machine_mode tmode,
         machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   scalar_float_mode float_tmode, float_fmode;
   if (!mtcs_mode_is_a <scalar_float_mode>(mtcsMode,fmode, &float_fmode)
   || !mtcs_mode_is_a <scalar_float_mode>(mtcsMode,tmode, &float_tmode)
   || float_tmode == float_fmode)
      return;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_tmode) != mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_fmode))
      mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, float_tmode, float_fmode);

   if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,float_fmode) <=
         mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,float_tmode)
   && (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,float_tmode) != &arm_bfloat_half_format
   || mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,float_fmode) != &ieee_half_format)
   && (mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,float_tmode) != &ieee_quad_format
   || mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,float_fmode) != &ibm_extended_format))
      return;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_tmode) == mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_fmode))
      mtcs_libfuncs_gen_intraclass_conv_libfunc/*!gen_intraclass_conv_libfunc*/(self,tab, opname, float_tmode, float_fmode);
}

/* Pick proper libcall for extend_optab.  We need to chose if we do
   truncation or extension and interclass or intraclass.  */
//原型 gen_extend_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_extend_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
          const char *opname ATTRIBUTE_UNUSED,
          machine_mode tmode,
          machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   scalar_float_mode float_tmode, float_fmode;
   if (!mtcs_mode_is_a <scalar_float_mode>(mtcsMode,fmode, &float_fmode)
   || !mtcs_mode_is_a <scalar_float_mode>(mtcsMode,tmode, &float_tmode)
   || float_tmode == float_fmode)
      return;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_tmode) !=
            mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_fmode))
      mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, float_tmode, float_fmode);

   if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,float_fmode) >
            mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,float_tmode))
      return;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_tmode) == mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,float_fmode))
      mtcs_libfuncs_gen_intraclass_conv_libfunc/*!gen_intraclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* Pick proper libcall for fract_optab.  We need to chose if we do
   interclass or intraclass.  */
//原型 gen_fract_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fract_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
         const char *opname,
         machine_mode tmode,
         machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (tmode == fmode)
      return;
   if (!(mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,tmode) ||
         mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,fmode)))
      return;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode) == mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode))
      mtcs_libfuncs_gen_intraclass_conv_libfunc/*!gen_intraclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
   else
      mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* Pick proper libcall for fractuns_optab.  */
//原型 gen_fractuns_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fractuns_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
            const char *opname,
            machine_mode tmode,
            machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (tmode == fmode)
      return;
   /* One mode must be a fixed-point mode, and the other must be an integer
   mode.  */
   if (!((mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,tmode)
         && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode) == MODE_INT)
   || (mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,fmode)
   && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode) == MODE_INT)))
      return;

   mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* Pick proper libcall for satfract_optab.  We need to chose if we do
   interclass or intraclass.  */
//原型 gen_satfract_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_satfract_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
            const char *opname,
            machine_mode tmode,
            machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (tmode == fmode)
      return;
   /* TMODE must be a fixed-point mode.  */
   if (!mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,tmode))
      return;

   if (mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,tmode) == mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode))
      mtcs_libfuncs_gen_intraclass_conv_libfunc/*!gen_intraclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
   else
      mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}

/* Pick proper libcall for satfractuns_optab.  */
//原型 gen_satfractuns_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_satfractuns_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
               const char *opname,
               machine_mode tmode,
               machine_mode fmode)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   if (tmode == fmode)
      return;
   /* TMODE must be a fixed-point mode, and FMODE must be an integer mode.  */
   if (!(mtcs_mode_is_all_fixed_point_p/*!ALL_FIXED_POINT_MODE_P*/(mtcsMode,tmode)
         && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,fmode) == MODE_INT))
      return;

   mtcs_libfuncs_gen_interclass_conv_libfunc/*!gen_interclass_conv_libfunc*/(self,tab, opname, tmode, fmode);
}
