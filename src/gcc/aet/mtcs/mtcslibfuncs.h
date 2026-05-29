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


#ifndef __GCC_MTCS_LIBFUNCS__
#define __GCC_MTCS_LIBFUNCS__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "libfuncs.h"

typedef struct _MtcsLibfuncs MtcsLibfuncs;

//原型 optab_libcall_d insn-opinit.h
struct mtcs_optab_libcall_d
{
  char libcall_suffix;
  const char *libcall_basename;
  void (*libcall_gen) (MtcsLibfuncs *self,optab, const char *name,
             char suffix, machine_mode);
};

//原型 convert_optab_libcall_d insn-opinit.h
struct mtcs_convert_optab_libcall_d
{
  const char *libcall_basename;
  void (*libcall_gen) (MtcsLibfuncs *self,convert_optab, const char *name,
             machine_mode, machine_mode);
};

struct _MtcsLibfuncs
{
    MtcsComponent parent;

    /* SYMBOL_REF rtx's for the library functions that are called
       implicitly and not via optabs.  */
    rtx x_libfunc_table[LTI_MAX];//原型 struct GTY(()) target_libfuncs libfuncs.h

    /* Hash table used to convert declarations into nodes.  */
   // hash_table<libfunc_hasher> *GTY(()) x_libfunc_hash;//原型 struct GTY(()) target_libfuncs libfuncs.h
    NHashTable *x_libfunc_hash;//原型 struct GTY(()) target_libfuncs libfuncs.h

    /* A table of previously-created libfuncs, hashed by name.  */
    NHashTable *libfunc_decls;  //原型 libfunc_decls optabs-libfuncs.cc
    //原型 extern const struct convert_optab_libcall_d convlib_def[NUM_CONVLIB_OPTABS]; insn-opinit.h insn-opinit.cc
    struct mtcs_convert_optab_libcall_d *convlib_def;
    //原型 extern const struct optab_libcall_d normlib_def[NUM_NORMLIB_OPTABS];insn-opinit.h insn-opinit.cc
    struct mtcs_optab_libcall_d *normlib_def;

};


MtcsLibfuncs     *mtcs_libfuncs_new(MtcsMode *mtcsMode);
void mtcs_libfuncs_set_normalib_def(MtcsLibfuncs *self,struct mtcs_optab_libcall_d *normlib_def);
void mtcs_libfuncs_set_convlib_def(MtcsLibfuncs *self,struct mtcs_convert_optab_libcall_d *convlib_def);

//原型 init_optabs optabs-libfuncs.h optabs-libfuncs.cc
//#define libfunc_hash   (this_target_libfuncs->x_libfunc_hash) libfuncs.h 和 optabs-libfuncs.cc
void mtcs_libfuncs_init_optabs (MtcsLibfuncs *self);
//原型 init_one_libfunc_visibility optabs-libfuncs.h  optabs-libfuncs.cc
rtx mtcs_libfuncs_init_one_libfunc_visibility (MtcsLibfuncs *self,const char *name, symbol_visibility vis);
//原型 init_one_libfunc optabs-libfuncs.h  optabs-libfuncs.cc
rtx mtcs_libfuncs_init_one_libfunc (MtcsLibfuncs *self,const char *name);
//原型 set_optab_libfunc optabs-libfuncs.h  optabs-libfuncs.cc
void mtcs_libfuncs_set_optab_libfunc (MtcsLibfuncs *self,optab op, machine_mode mode, const char *name);
//原型 set_conv_libfunc optabs-libfuncs.h  optabs-libfuncs.cc
void mtcs_libfuncs_set_conv_libfunc (MtcsLibfuncs *self,convert_optab optab, machine_mode tmode,
        machine_mode fmode, const char *name);
//原型  optab_libfunc optabs-libfuncs.h optabs-libfuncs.cc
rtx mtcs_libfuncs_optab_libfunc (MtcsLibfuncs *self,optab optab, machine_mode mode);
//原型 convert_optab_libfunc optabs-libfuncs.h optabs-libfuncs.cc
rtx mtcs_libfuncs_convert_optab_libfunc(MtcsLibfuncs *self,convert_optab optab, machine_mode mode1,
               machine_mode mode2);
//原型 build_libfunc_function_visibility optabs-libfuncs.h optabs-libfuncs.cc
tree mtcs_libfuncs_build_libfunc_function_visibility (MtcsLibfuncs *self,const char *name, symbol_visibility vis);
//原型 gen_int_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
       machine_mode mode);
//原型 gen_fp_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fp_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
      machine_mode mode);
//原型 gen_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
         machine_mode mode);
//原型 gen_signed_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_signed_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
           machine_mode mode);
//原型 gen_unsigned_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_unsigned_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *opname, char suffix,
             machine_mode mode);
//原型 gen_int_fp_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fp_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
          machine_mode mode);
//原型 gen_intv_fp_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_intv_fp_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
           machine_mode mode);
//原型 gen_int_fp_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fp_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
           machine_mode mode);
//原型 gen_int_fp_signed_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fp_signed_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
             machine_mode mode);
//原型 gen_int_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
             machine_mode mode);
//原型 gen_int_signed_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_signed_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
               machine_mode mode);
//原型 gen_int_unsigned_fixed_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_unsigned_fixed_libfunc (MtcsLibfuncs *self,optab optable, const char *name, char suffix,
            machine_mode mode);
//原型 gen_interclass_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_interclass_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
              const char *opname,
              machine_mode tmode,
              machine_mode fmode);
//原型 gen_int_to_fp_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_to_fp_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
             const char *opname,
             machine_mode tmode,
             machine_mode fmode);
//原型 gen_ufloat_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_ufloat_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
          const char *opname ATTRIBUTE_UNUSED,machine_mode tmode, machine_mode fmode);
//原型 gen_int_to_fp_nondecimal_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_int_to_fp_nondecimal_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
                   const char *opname,
                   machine_mode tmode,
                   machine_mode fmode);
//原型 gen_fp_to_int_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fp_to_int_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
             const char *opname,
             machine_mode tmode,
             machine_mode fmode);
//原型 gen_intraclass_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_intraclass_conv_libfunc (MtcsLibfuncs *self,convert_optab tab, const char *opname,
              machine_mode tmode, machine_mode fmode);
//原型 gen_trunc_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_trunc_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
         const char *opname,
         machine_mode tmode,
         machine_mode fmode);
//原型 gen_extend_conv_libfunc optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_extend_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
          const char *opname ATTRIBUTE_UNUSED,
          machine_mode tmode,
          machine_mode fmode);
//原型 gen_fract_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fract_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
         const char *opname,
         machine_mode tmode,
         machine_mode fmode);
//原型 gen_fractuns_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_fractuns_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
            const char *opname,
            machine_mode tmode,
            machine_mode fmode);
//原型 gen_satfract_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_satfract_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
            const char *opname,
            machine_mode tmode,
            machine_mode fmode);
//原型 gen_satfractuns_conv_libfunc  optabs-libfuncs.h optabs-libfuncs.cc optabs.def引用
void mtcs_libfuncs_gen_satfractuns_conv_libfunc (MtcsLibfuncs *self,convert_optab tab,
               const char *opname,
               machine_mode tmode,
               machine_mode fmode);

#endif

