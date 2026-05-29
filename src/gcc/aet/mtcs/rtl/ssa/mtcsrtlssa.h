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

#ifndef GCC_MTCS_RTL_SSA_H
#define GCC_MTCS_RTL_SSA_H 1

#include "pretty-print.h"

// Needed directly by recog.h.
#include "insn-config.h"

// Needed directly by rtl-ssa.
#include "splay-tree-utils.h"
#include "recog.h"
#include "regs.h"
#include "function-abi.h"
#include "obstack-utils.h"
#include "mux-utils.h"
#include "rtlanal.h"
#include "cfgbuild.h"
#include "hash-set.h"


// Provides the global crtl->ssa.
#include "memmodel.h"
#include "tm_p.h"
#include "emit-rtl.h"

// The rtl-ssa files themselves.
//#include "rtl/ssa/accesses.h"
//#include "rtl/ssa/insns.h"
//#include "rtl/ssa/blocks.h"
//#include "rtl/ssa/changes.h"
//#include "rtl/ssa/functions.h"
//#include "rtl/ssa/is-a.inl"
//#include "rtl/ssa/access-utils.h"
//#include "rtl/ssa/insn-utils.h"
//#include "rtl/ssa/mtcsmovement.h"
//#include "rtl/ssa/change-utils.h"
//#include "rtl/ssa/member-fns.inl"

//#include "mtcsaccesses.h"
//#include "mtcsinsns.h"
//#include "mtcsblocks.h"
//#include "mtcschanges.h"
//#include "mtcsfunctions.h"
//#include "is-a.inl"
//#include "insn-utils.h"
//#include "mtcsmovement.h"
//#include "member-fns.inl"

#include "mtcsaccesses.h"
#include "mtcsinsns.h"
#include "mtcsblocks.h"
#include "mtcschanges.h"
#include "mtcsfunctions.h"
#include "predicates.h"
#include "is-a.inl"
#include "access-utils.h"
#include "insn-utils.h"
#include "mtcsmovement.h"
#include "change-utils.h"
#include "member-fns.inl"



#endif
