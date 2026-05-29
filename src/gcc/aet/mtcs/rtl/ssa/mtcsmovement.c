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
 * base on movement.cc
 */

#define INCLUDE_ALGORITHM
#define INCLUDE_FUNCTIONAL
#define INCLUDE_ARRAY

//#include "config.h"
//#include "system.h"
//#include "coretypes.h"
//#include "backend.h"
//#include "rtl.h"
//#include "df.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "df.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "expmed.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "alias.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "attribs.h"
#include "varasm.h"
#include "except.h"
#include "insn-attr.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "stmt.h"
/* Include expr.h after insn-config.h so we get HAVE_conditional_move.  */
#include "expr.h"
#include "optabs-tree.h"
#include "libfuncs.h"
#include "reload.h"
#include "langhooks.h"
#include "common/common-target.h"
#include "tree-dfa.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "tree-ssa-address.h"
#include "builtins.h"
#include "ccmp.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "rtx-vector-builder.h"
#include "tree-pretty-print.h"
#include "flags.h"


#include "mtcsrtlssa.h"
#include "internals.h"
#include "internals.inl"

#include "../../mtcsmicro.h"
#include "../../mtcstarget.h"
#include "../../mtcscompile.h"

using namespace mtcs_rtl_ssa;

// See the comment above the declaration.
bool mtcs_rtl_ssa::can_move_insn_p (insn_info *insn)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   return (!mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn->rtl ())
         && !mtcs_rtlanal_may_trap_p/*!may_trap_p*/(mtcsRtlanal,PATTERN (insn->rtl ())));
}

// Return true if it is possible to insert a new instruction after INSN.
bool mtcs_rtl_ssa:: can_insert_after (insn_info *insn)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsCfgBuild *mtcsCfgBuild=mtcs_target_get_cfg_build(mtcsTarget);

   return (insn->is_bb_head () || (insn->is_real ()
      && !mtcs_cfg_build_control_flow_insn_p/*!control_flow_insn_p*/(mtcsCfgBuild,insn->rtl ())));
}

