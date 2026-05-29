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


#ifndef __GCC_MTCS_INSN_ATTR__
#define __GCC_MTCS_INSN_ATTR__

#include "../nlib.h"
#include "mtcscomponent.h"

typedef struct _MtcsInsnAttr MtcsInsnAttr;
struct _MtcsInsnAttr
{
   MtcsComponent parent;
   //原型 static  have_bool_attr recog.cc
   bool (*have_bool_attr)(MtcsInsnAttr *self,int/*!enum bool_attr 编译不过改为 int*/ boolAttr);
   //原型 #define get_attr_enabled hook_int_rtx_1 insn-attr.h
   int (*get_enabled)(MtcsInsnAttr *self,rtx x);
   //原型 #define get_attr_preferred_for_size hook_int_rtx_1 insn-attr.h
   int (*get_preferred_for_size)(MtcsInsnAttr *self,rtx x);
   //原型 #define get_attr_preferred_for_speed hook_int_rtx_1 insn-attr.h
   int (*get_preferred_for_speed)(MtcsInsnAttr *self,rtx x);
   //原型 DELAY_SLOTS insn-attr.h
   int delaySlots;
   //原型 extern int num_delay_slots (rtx_insn *); insn-attr.h insn-attrtab.cc
   int (*num_delay_slots)(MtcsInsnAttr *self,rtx_insn *rn);
   //原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
   int (*eligible_for_annul_true) (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
       int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED);
   //原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
   int (*eligible_for_annul_false) (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
         int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED);
   //原型 eligible_for_delay insn-attr.h insn-attrtab.cc
   int (*eligible_for_delay)(MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED, int slot,
          rtx_insn *candidate_insn, int flags ATTRIBUTE_UNUSED);
   //原型 enum attr_subregs_ok get_attr_subregs_ok (rtx_insn *); insn-attr.h
   int (*get_attr_subregs_ok)(MtcsInsnAttr *self,rtx_insn * insn);
   //原型 extern enum attr_atomic get_attr_atomic (rtx_insn *); insn-attr.h
   int (*get_attr_atomic)(MtcsInsnAttr *self,rtx_insn * insn);
   //原型 extern int const_num_delay_slots (rtx_insn *);
   int (*const_num_delay_slots)(MtcsInsnAttr *self,rtx_insn * insn);
   //原型 #define insn_variable_length_p hook_int_rtx_insn_unreachable insn-attr.h
   int (*is_insn_variable_length_p)(MtcsInsnAttr *self,rtx_insn * insn);



   //原型 #define ANNUL_IFFALSE_SLOTS 0 insn-attr.h
   int annul_IFFALSE_SLOTS;
   //原型 #define ANNUL_IFTRUE_SLOTS 0 insn-attr.h
   int annul_IFTRUE_SLOTS;
   //原型 #define HAVE_ATTR_length insn-attr.h
   int haveAttrLength;
   //原型 #define insn_default_length hook_int_rtx_insn_unreachable
   int (* default_length) (MtcsInsnAttr *self,rtx_insn *insn);
   //原型 #define insn_min_length hook_int_rtx_insn_unreachable
   int (* min_length) (MtcsInsnAttr *self,rtx_insn *insn);
   //原型 length_unit_log insn-attrtab.cc
   int length_unit_log;
};

void       mtcs_insn_attr_init(MtcsInsnAttr *self);
//原型 static  have_bool_attr recog.cc
bool mtcs_insn_attr_have_bool_attr(MtcsInsnAttr *self,int/*!enum bool_attr 编译不过改为 int*/ boolAttr);
//原型 #define get_attr_enabled hook_int_rtx_1 insn-attr.h
int mtcs_insn_attr_get_enabled(MtcsInsnAttr *self,rtx x);
//原型 #define get_attr_preferred_for_size hook_int_rtx_1 insn-attr.h
int mtcs_insn_attr_get_preferred_for_size(MtcsInsnAttr *self,rtx x);
//原型 #define get_attr_preferred_for_speed hook_int_rtx_1 insn-attr.h
int mtcs_insn_attr_get_preferred_for_speed(MtcsInsnAttr *self,rtx x);
//原型 DELAY_SLOTS insn-attr.h
void mtcs_insn_attr_set_delay_slots(MtcsInsnAttr *self,int value);
int mtcs_insn_attr_get_delay_slots(MtcsInsnAttr *self);
//原型 extern int num_delay_slots (rtx_insn *); insn-attr.h insn-attrtab.cc
int mtcs_insn_attr_num_delay_slots(MtcsInsnAttr *self,rtx_insn *rn);
//原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
int  mtcs_insn_attr_eligible_for_annul_true (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
    int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED);
//原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
int  mtcs_insn_attr_eligible_for_annul_false (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
      int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED);

//原型 #define ANNUL_IFTRUE_SLOTS 0 insn-attr.h
void mtcs_insn_attr_set_ANNUL_IFTRUE_SLOTS(MtcsInsnAttr *self,int value);
int  mtcs_insn_attr_get_ANNUL_IFTRUE_SLOTS(MtcsInsnAttr *self);
//原型 #define ANNUL_IFFALSE_SLOTS 0 insn-attr.h
void mtcs_insn_attr_set_ANNUL_IFFALSE_SLOTS(MtcsInsnAttr *self,int value);
int  mtcs_insn_attr_get_ANNUL_IFFALSE_SLOTS(MtcsInsnAttr *self);
//原型 eligible_for_delay insn-attr.h insn-attrtab.cc
int mtcs_insn_attr_eligible_for_delay(MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED, int slot,
       rtx_insn *candidate_insn, int flags ATTRIBUTE_UNUSED);
//原型 #define HAVE_ATTR_length insn-attr.h
void mtcs_insn_attr_set_have_attr_length(MtcsInsnAttr *self,int value);
int  mtcs_insn_attr_get_have_attr_length(MtcsInsnAttr *self);
//原型 #define insn_default_length hook_int_rtx_insn_unreachable
int  mtcs_insn_attr_insn_default_length (MtcsInsnAttr *self,rtx_insn *insn);
//原型 #define insn_min_length hook_int_rtx_insn_unreachable
int  mtcs_insn_attr_insn_min_length (MtcsInsnAttr *self,rtx_insn *insn);
//原型 enum attr_subregs_ok get_attr_subregs_ok (rtx_insn *);insn-attr.h
int  mtcs_insn_attr_get_attr_subregs_ok (MtcsInsnAttr *self,rtx_insn *insn);
//原型 extern enum attr_atomic get_attr_atomic (rtx_insn *); insn-attr.h
int  mtcs_insn_attr_get_attr_atomic (MtcsInsnAttr *self,rtx_insn *insn);
//原型 extern int const_num_delay_slots (rtx_insn *);
int  mtcs_insn_attr_const_num_delay_slots (MtcsInsnAttr *self,rtx_insn *insn);
//原型 #define insn_variable_length_p hook_int_rtx_insn_unreachable insn-attr.h
int  mtcs_insn_attr_is_insn_variable_length_p (MtcsInsnAttr *self,rtx_insn *insn);
//原型 length_unit_log insn-attrtab.cc
void mtcs_insn_attr_set_length_unit_log(MtcsInsnAttr *self,int value);
int mtcs_insn_attr_get_length_unit_log(MtcsInsnAttr *self);
#endif

