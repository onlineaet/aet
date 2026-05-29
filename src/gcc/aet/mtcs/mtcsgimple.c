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
#include "gimple.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "output.h"
#include "graph.h"
#include "debug.h"
#include "cfgloop.h"
#include "value-prof.h"
#include "tree-cfg.h"
#include "tree-ssa-loop-manip.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "tree-eh.h"
#include "gimple-pretty-print.h"
#include "gimplify-me.h"
#include "gimple-walk.h"
#include "tree-into-ssa.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "tree-pass.h"
#include "plugin.h"
#include "ipa-utils.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "context.h"
#include "pass_manager.h"
#include "cfgrtl.h"
#include "tree-ssa-live.h"  /* For remove_unused_locals.  */
#include "tree-cfgcleanup.h"
#include "insn-addr.h" /* for INSN_ADDRESSES_ALLOC.  */
#include "diagnostic-core.h" /* for fnotice */
#include "stringpool.h"
#include "attribs.h"
#include "opts.h"
#include "tree-ssa-dce.h"
#include "except.h"
#include "cfganal.h"
#include "cfgcleanup.h"
#include "asan.h"
#include "dbgcnt.h"

#include "mtcsgimple.h"
#include "mtcstarget.h"

/*************--------以下是MtcsGimple对象实现-----------------------------*/
static void mtcsGimpleInit(MtcsGimple *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
}

/* Set FNDECL to be the function called by call statement GS.  */
//原型 gimple_call_set_fndecl gimple.h
void mtcs_gimple_call_set_fndecl (MtcsGimple *self,gcall *gs, tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   gcc_gimple_checking_assert (!gimple_call_internal_p (gs));
   gs->op[1] = build1_loc (gimple_location (gs), ADDR_EXPR,
        mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (decl)), decl);
}
//原型 gimple_call_set_fndecl gimple.h
void mtcs_gimple_call_set_fndecl (MtcsGimple *self,gimple *gs, tree decl)
{
  gcall *gc = GIMPLE_CHECK2<gcall *> (gs);
  mtcs_gimple_call_set_fndecl(self,gc, decl);
}


MtcsGimple *mtcs_gimple_new(MtcsMode *mtcsMode)
{
    MtcsGimple *self = n_slice_alloc0 (sizeof(MtcsGimple));
    mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
    mtcsGimpleInit(self);
    return self;
}





