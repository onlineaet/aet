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


#ifndef __GCC_MTCS_IFCVT__
#define __GCC_MTCS_IFCVT__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcspass.h"

typedef struct _MtcsIfcvt MtcsIfcvt;
struct _MtcsIfcvt
{
    MtcsComponent parent;
    /* True if after combine pass.  */
     bool ifcvt_after_combine;

    /* True if the target has the cbranchcc4 optab.  */
     bool have_cbranchcc4;

    /* # of IF-THEN or IF-THEN-ELSE blocks we looked at  */
     int num_possible_if_blocks;

    /* # of IF-THEN or IF-THEN-ELSE blocks were converted to conditional
       execution.  */
     int num_updated_if_blocks;

    /* # of changes made.  */
     int num_true_changes;

    /* Whether conditional execution changes were made.  */
     bool cond_exec_changed_p;

};

MtcsIfcvt *mtcs_ifcvt_new(MtcsMode *mtcsMode);
//原型 default_noce_conversion_profitable_p targhooks.h ifcvt.cc
bool mtcs_ifcvt_default_noce_conversion_profitable_p (MtcsIfcvt *self,rtx_insn *seq,
                  struct noce_if_info *if_info);


//原型 NEXT_PASS (pass_rtl_ifcvt, 1); RTL_PASS ifcvt.cc ce1 y 有条件执行 (optimize > 0) && dbg_cnt (if_conversion);  rest_of_handle_if_conversion ();
typedef struct _MtcsPassIfcvt MtcsPassIfcvt;
struct _MtcsPassIfcvt
{
   MtcsPass parent;
};
MtcsPassIfcvt *mtcs_pass_ifcvt_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_if_after_combine, 1); RTL_PASS ifcvt.cc ce2 y 有条件执行 optimize > 0 && flag_if_conversion.. if_convert(true);
typedef struct _MtcsPassIfAfterCombine MtcsPassIfAfterCombine;
struct _MtcsPassIfAfterCombine
{
   MtcsPass parent;
};
MtcsPassIfAfterCombine *mtcs_pass_if_after_combine_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_if_after_reload, 1); RTL_PASS ifcvt.cc ce3 y 有条件执行 optimize > 0 && flag_if_conversion.. if_convert(true);
typedef struct _MtcsPassIfAfterReload MtcsPassIfAfterReload;
struct _MtcsPassIfAfterReload
{
   MtcsPass parent;
};
MtcsPassIfAfterReload *mtcs_pass_if_after_reload_new(MtcsMode *mtcsMode);

#endif

