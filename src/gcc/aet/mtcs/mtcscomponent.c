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
#include "tree-pass.h"
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "optabs.h"
#include "regs.h" /* For reg_renumber.  */
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "stmt.h"
#include "print-tree.h"
#include "cfgrtl.h"
#include "cfganal.h"
#include "cfgbuild.h"
#include "cfgcleanup.h"
#include "dojump.h"
#include "explow.h"
#include "calls.h"
#include "expr.h"
#include "internal-fn.h"
#include "tree-eh.h"
#include "gimple-iterator.h"
#include "gimple-expr.h"
#include "gimple-walk.h"
#include "tree-cfg.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "except.h"
#include "gimple-pretty-print.h"
#include "toplev.h"
#include "debug.h"
#include "tree-inline.h"
#include "value-prof.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "cfgloop.h"
#include "insn-attr.h" /* For INSN_SCHEDULING.  */
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tree-ssa-address.h"
#include "output.h"
#include "builtins.h"
#include "opts.h"

#include "mtcscomponent.h"


MtcsMode *mtcs_component_get_mode(MtcsComponent *self)
{
    return self->mtcsMode;
}
void      mtcs_component_set_mode(MtcsComponent *self,MtcsMode *mtcsMode)
{
    self->mtcsMode=mtcsMode;
}



