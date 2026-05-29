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

#include "mtcsptxinsnattr.h"
#include "ptx-common.h"
#include "gen/ptx-insn-attr.h"

//原型 static  have_bool_attr recog.cc
static bool haveBoolAttr_cb(MtcsInsnAttr *self,int/*!enum bool_attr 编译不过改为 int*/ boolAttr);
//原型 #define get_attr_enabled hook_int_rtx_1 insn-attr.h
static int  getEnabled_cb(MtcsInsnAttr *self,rtx x);
//原型 #define get_attr_preferred_for_size hook_int_rtx_1 insn-attr.h
static int  getPreferredForSize_cb(MtcsInsnAttr *self,rtx x);
//原型 #define get_attr_preferred_for_speed hook_int_rtx_1 insn-attr.h
static int  getPreferredForSpeed_cb(MtcsInsnAttr *self,rtx x);

//原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
static int  eligibleForAnnulTrue_cb (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
    int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED);
//原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
static int  eligibleForAnnulFalse_cb (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
      int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED);

//原型 #define insn_default_length hook_int_rtx_insn_unreachable insn-attr.h
static int defaultLength_cb (MtcsInsnAttr *self,rtx_insn *insn);
//原型 #define insn_min_length hook_int_rtx_insn_unreachable insn-attr.h
static int minLength_cb (MtcsInsnAttr *self,rtx_insn *insn);
//原型 extern int const_num_delay_slots (rtx_insn *);
static int constNumDelaySlots_cb (MtcsInsnAttr *self,rtx_insn *insn);
//原型 #define insn_variable_length_p hook_int_rtx_insn_unreachable insn-attr.h
static int isInsnVariableLengthP_cb (MtcsInsnAttr *self,rtx_insn *insn);

static void mtcsPtxInsnAttrInit(MtcsPtxInsnAttr *self)
{
   MtcsInsnAttr *mtcsInsnAttr=(MtcsInsnAttr *)self;
   //原型 static  have_bool_attr recog.cc
   mtcsInsnAttr->have_bool_attr=haveBoolAttr_cb;
   //原型 #define get_attr_enabled hook_int_rtx_1 insn-attr.h
   mtcsInsnAttr->get_enabled=getEnabled_cb;
   //原型 #define get_attr_preferred_for_size hook_int_rtx_1 insn-attr.h
   mtcsInsnAttr->get_preferred_for_size=getPreferredForSize_cb;
   //原型 #define get_attr_preferred_for_speed hook_int_rtx_1 insn-attr.h
   mtcsInsnAttr->get_preferred_for_speed=getPreferredForSpeed_cb;
   //原型 extern int num_delay_slots (rtx_insn *); insn-attr.h insn-attrtab.cc
   mtcsInsnAttr->num_delay_slots=ptx_num_delay_slots;//声明 ptx-insn-attr.h ptx-insn-attr.c
   //原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
   mtcsInsnAttr->eligible_for_annul_true=eligibleForAnnulTrue_cb;
   //原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
   mtcsInsnAttr->eligible_for_annul_false=eligibleForAnnulFalse_cb;
   //原型 eligible_for_delay insn-attr.h insn-attrtab.cc
   mtcsInsnAttr->eligible_for_delay=ptx_eligible_for_delay;
   //原型 enum attr_subregs_ok get_attr_subregs_ok (rtx_insn *); insn-attr.h
   mtcsInsnAttr->get_attr_subregs_ok=ptx_get_attr_subregs_ok;
   //原型 extern enum attr_atomic get_attr_atomic (rtx_insn *); insn-attr.h
   mtcsInsnAttr->get_attr_atomic=ptx_get_attr_atomic;

   //原型 DELAY_SLOTS insn-attr.h
   mtcs_insn_attr_set_delay_slots(mtcsInsnAttr,PTX_DELAY_SLOTS);
   //原型 #define ANNUL_IFTRUE_SLOTS 0 insn-attr.h
   mtcs_insn_attr_set_ANNUL_IFTRUE_SLOTS(mtcsInsnAttr,PTX_ANNUL_IFTRUE_SLOTS);
     //原型 #define ANNUL_IFFALSE_SLOTS 0 insn-attr.h
   mtcs_insn_attr_set_ANNUL_IFFALSE_SLOTS(mtcsInsnAttr,PTX_ANNUL_IFFALSE_SLOTS);
   //原型 #define HAVE_ATTR_length insn-attr.h
   mtcs_insn_attr_set_have_attr_length(mtcsInsnAttr,PTX_HAVE_ATTR_length);
   //原型 length_unit_log insn-attrtab.cc
   mtcs_insn_attr_set_length_unit_log(mtcsInsnAttr,0);
   //原型 #define insn_default_length hook_int_rtx_insn_unreachable insn-attr.h
   mtcsInsnAttr->default_length=defaultLength_cb;
   //原型 #define insn_min_length hook_int_rtx_insn_unreachable insn-attr.h
   mtcsInsnAttr->min_length=minLength_cb;
   //原型 extern int const_num_delay_slots (rtx_insn *);
   mtcsInsnAttr->const_num_delay_slots=constNumDelaySlots_cb;
   //原型 #define insn_variable_length_p hook_int_rtx_insn_unreachable insn-attr.h
   mtcsInsnAttr->is_insn_variable_length_p=isInsnVariableLengthP_cb;

}

//原型 static  have_bool_attr recog.cc
static bool haveBoolAttr_cb(MtcsInsnAttr *self,int/*!enum bool_attr 编译不过改为 int*/ boolAttr)
{
   enum bool_attr attr=(enum bool_attr)boolAttr;
     switch (attr){
       case BA_ENABLED:
         return PTX_HAVE_ATTR_enabled;
       case BA_PREFERRED_FOR_SIZE:
         return PTX_HAVE_ATTR_enabled || PTX_HAVE_ATTR_preferred_for_size;
       case BA_PREFERRED_FOR_SPEED:
         return PTX_HAVE_ATTR_enabled || PTX_HAVE_ATTR_preferred_for_speed;
       }
     gcc_unreachable ();
}

//原型 #define get_attr_enabled hook_int_rtx_1 insn-attr.h
static int  getEnabled_cb(MtcsInsnAttr *self,rtx x)
{
   return 1;
}
//原型 #define get_attr_preferred_for_size hook_int_rtx_1 insn-attr.h
static int  getPreferredForSize_cb(MtcsInsnAttr *self,rtx x)
{
   return 1;
}
//原型 #define get_attr_preferred_for_speed hook_int_rtx_1 insn-attr.h
static int  getPreferredForSpeed_cb(MtcsInsnAttr *self,rtx x)
{
   return 1;
}

//原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
static int  eligibleForAnnulTrue_cb (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
    int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED)
{
   return ptx_eligible_for_annul_true(delay_insn,slot,candidate_insn,flags);

}

//原型 eligible_for_annul_true insn-attr.h insn-attrtab.cc
static int  eligibleForAnnulFalse_cb (MtcsInsnAttr *self,rtx_insn *delay_insn ATTRIBUTE_UNUSED,
      int slot ATTRIBUTE_UNUSED,  rtx_insn *candidate_insn ATTRIBUTE_UNUSED, int flags ATTRIBUTE_UNUSED)
{
   return ptx_eligible_for_annul_false(delay_insn,slot,candidate_insn,flags);
}

//原型 #define insn_default_length hook_int_rtx_insn_unreachable
static int defaultLength_cb (MtcsInsnAttr *self,rtx_insn *insn)
{
   fprintf(stderr,"不应该执行到这里 insn_default_length\n");
   gcc_unreachable ();
}

//原型 #define insn_min_length hook_int_rtx_insn_unreachable insn-attr.h
static int minLength_cb (MtcsInsnAttr *self,rtx_insn *insn)
{
   fprintf(stderr,"不应该执行到这里 insn_min_length\n");
   gcc_unreachable ();
}

//原型 extern int const_num_delay_slots (rtx_insn *);
static int constNumDelaySlots_cb (MtcsInsnAttr *self,rtx_insn *insn)
{
   return ptx_const_num_delay_slots(self,insn);
}

//原型 #define insn_variable_length_p hook_int_rtx_insn_unreachable insn-attr.h
static int isInsnVariableLengthP_cb (MtcsInsnAttr *self,rtx_insn *insn)
{
   fprintf(stderr,"不应该执行到这里 insn_variable_length_p\n");
   gcc_unreachable ();
}


MtcsPtxInsnAttr *mtcs_ptx_insn_attr_new(MtcsMode *mtcsMode)
{
     MtcsPtxInsnAttr *self = n_slice_alloc0 (sizeof(MtcsPtxInsnAttr));
     mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
     mtcs_insn_attr_init((MtcsInsnAttr *)self);
     mtcsPtxInsnAttrInit(self);
     return self;
}
