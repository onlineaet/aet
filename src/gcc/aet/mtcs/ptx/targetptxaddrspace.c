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

#include "cfganal.h"
#include "cfgcleanup.h"
#include "alias.h"
#include "rtlhooks-def.h"
#include "tree-pass.h"
#include "dbgcnt.h"
#include "rtlanal.h"

#include "targetptxaddrspace.h"
#include "aet/aetprinttree.h"
#include "gen/ptx-insn-modes.h"
#include "mtcsptxmode.h"


/* Returns true if X is a valid address for use in a memory reference.  */
//原型 targetm.addr_space.legitimate_address_p (mode, addr, 0, as, ch);#define TARGET_LEGITIMATE_ADDRESS_P nvptx_legitimate_address_p
static bool legitimateAddressP_cb(TargetAddrSpace *targetAddrSpace,
      mtcs_mode mode ,rtx x ,bool strict ,addr_space_t as, code_helper ch)
{
   n_debug("mtcsptx.c -----nvptx.cc -----47-- TARGET_LEGITIMATE_ADDRESS_P legitimateAddressP_cb mode:%d strict:%d\n",mode,strict);

   enum rtx_code code = GET_CODE (x);
   switch (code){
      case REG:
         return true;
      case PLUS:
         if (REG_P (XEXP (x, 0)) && CONST_INT_P (XEXP (x, 1)))
            return true;
         return false;
      case CONST:
      case SYMBOL_REF:
      case LABEL_REF:
         return true;
      default:
         return false;
   }
}

//原型 targetm.addr_space.address_mode*/(MEM_ADDR_SPACE (mem));#define TARGET_ADDR_SPACE_ADDRESS_MODE default_addr_space_address_mode
static scalar_int_mode addressMode_cb (TargetAddrSpace *targetAddrSpace,addr_space_t addrspace ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetAddrSpace);

   scalar_int_mode mode = mtcs_mode_as_a/*!as_a*/<scalar_int_mode>(mtcsMode,PTX_Pmode);
   scalar_int_mode mode1 =( (scalar_int_mode::from_int) PTX_Pmode);
   gcc_assert(mode==mode1);
   return  mode;
}

//原型 targetm.addr_space.pointer_mode (MEM_ADDR_SPACE (target)) #define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
static scalar_int_mode pointerMode_cb (TargetAddrSpace *targetAddrSpace,addr_space_t addrspace ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetAddrSpace);
   return mtcsMode->ptr_mode;
}

static void targetPtxAddrSpaceInit(TargetPtxAddrSpace *self)
{
   TargetAddrSpace *targetAddrSpace=(TargetAddrSpace *)self;
   //原型 targetm.addr_space.legitimate_address_p (mode, addr, 0, as, ch);#define TARGET_LEGITIMATE_ADDRESS_P nvptx_legitimate_address_p
   targetAddrSpace->legitimate_address_p=legitimateAddressP_cb;
   //原型 targetm.addr_space.address_mode*/(MEM_ADDR_SPACE (mem));#define TARGET_ADDR_SPACE_ADDRESS_MODE default_addr_space_address_mode
   targetAddrSpace->address_mode=addressMode_cb;
   //原型 targetm.addr_space.pointer_mode (MEM_ADDR_SPACE (target)) #define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
   targetAddrSpace->pointer_mode=pointerMode_cb;
}

TargetPtxAddrSpace *target_ptx_addr_space_new(MtcsMode *mtcsMode)
{
   TargetPtxAddrSpace *self = n_slice_alloc0 (sizeof(TargetPtxAddrSpace));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   target_addr_space_init((TargetAddrSpace*)self);
   targetPtxAddrSpaceInit(self);
   return self;
}

