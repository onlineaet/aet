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

#ifndef __GCC_MTCS_RELOAD__
#define __GCC_MTCS_RELOAD__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcsmicro.h"


typedef struct _MtcsReplacement
{
    rtx *where;           /* Location to store in */
    int what;         /* which reload this is for */
    machine_mode mode;    /* mode it must have */
}MtcsReplacement;

typedef struct _MtcsReload MtcsReload;


struct _MtcsReload
{
    MtcsComponent parent;
   /* Each replacement is recorded with a structure like this.  */
   MtcsReplacement replacements[MAX_MAX_RECOG_OPERANDS*((MAX_MAX_REGS_PER_ADDRESS*2)+1)
                                /*!MAX_RECOG_OPERANDS * ((MAX_REGS_PER_ADDRESS * 2) + 1)*/];
   /* Number of replacements currently recorded.  */
   int n_replacements;
    //以下成员变量来自reload.h struct target_reload
   struct {
       /* Nonzero if indirect addressing is supported when the innermost MEM is
           of the form (MEM (SYMBOL_REF sym)).  It is assumed that the level to
           which these are valid is the same as spill_indirect_levels, above.  */
       bool x_indirect_symref_ok;

       /* Nonzero if indirect addressing is supported on the machine; this means
          that spilling (REG n) does not require reloading it into a register in
          order to do (MEM (REG n)) or (MEM (PLUS (REG n) (CONST_INT c))).  The
          value indicates the level of indirect addressing supported, e.g., two
          means that (MEM (MEM (REG n))) is also valid if (REG n) does not get
          a hard register.  */
       unsigned char x_spill_indirect_levels;

       /* True if caller-save has been reinitialized.  */
       bool x_caller_save_initialized_p;

       /* Modes for each hard register that we can save.  The smallest mode is wide
          enough to save the entire contents of the register.  When saving the
          register because it is live we first try to save in multi-register modes.
          If that is not possible the save is done one register at a time.  */
       machine_mode (x_regno_save_mode[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/][100/*!MAX_MOVE_MAX / MIN_UNITS_PER_WORD + 1*/]);

       /* Nonzero if an address (plus (reg frame_pointer) (reg ...)) is valid
          in the given mode.  */
       bool x_double_reg_address_ok[MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];

       /* We will only make a register eligible for caller-save if it can be
          saved in its widest mode with a simple SET insn as long as the memory
          address is valid.  We record the INSN_CODE is those insns here since
          when we emit them, the addresses might not be valid, so they might not
          be recognized.  */
       int x_cached_reg_save_code[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/][MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];
       int x_cached_reg_restore_code[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/][MAX_MAX_MACHINE_MODE/*!MAX_MACHINE_MODE*/];
   }target_reload;

   /* All the "earlyclobber" operands of the current insn
      are recorded here.  */
   //原型 reload.h
   int n_earlyclobbers;
   rtx reload_earlyclobbers[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];
};



MtcsReload *mtcs_reload_new(MtcsMode *mtcsMode);
//原型 find_replacement reload.h reload.cc
rtx mtcs_reload_find_replacement (MtcsReload *self,rtx *loc);
//原型 reload_adjust_reg_for_mode reload.h reload.cc
rtx mtcs_reload_adjust_reg_for_mode (MtcsReload *self,rtx reloadreg, machine_mode mode);
//原型 copy_replacements reload.h reload.cc
void mtcs_reload_copy_replacements ( MtcsReload *self,rtx x, rtx y);
//原型 operands_match_p reload.h reload.c
int mtcs_reload_operands_match_p (MtcsReload *self,rtx x, rtx y);
//原型 safe_from_earlyclobber reload.h reload.cc
int mtcs_reload_safe_from_earlyclobber (MtcsReload *self,rtx op, rtx clobber);
//原型 earlyclobber_operand_p reload.h reload.cc
int mtcs_reload_earlyclobber_operand_p (MtcsReload *self,rtx x);
//原型 memory_move_cost reload.h reginfo.cc
int mtcs_reload_memory_move_cost (MtcsReload *self,machine_mode mode, reg_class_t rclass, bool in);
//原型 scratch_reload_class reload.h reload.cc
enum reg_class mtcs_reload_scratch_reload_class (MtcsReload *self,enum insn_code icode);
//原型 secondary_reload_class reload.h reload.cc
reg_class_t mtcs_reload_secondary_reload_class (MtcsReload *self,bool in_p,
      reg_class_t rclass, machine_mode mode,rtx x);
//原型 register_move_cost reload.h reginfo.cc
int mtcs_reload_register_move_cost (MtcsReload *self,machine_mode mode, reg_class_t from, reg_class_t to);
//原型 memory_move_secondary_cost reload.h reginfo.cc
int mtcs_reload_memory_move_secondary_cost (MtcsReload *self,machine_mode mode, reg_class_t rclass,bool in);
//原型 init_reload reload.h reload1.cc 依赖ira_use_lra_p
void mtcs_reload_init_reload (MtcsReload *self);

#endif
