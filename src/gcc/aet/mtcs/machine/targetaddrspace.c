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

#include "targetaddrspace.h"
#include "aet/aetprinttree.h"
#include "../mtcstarget.h"


static void targetAddrSpaceInit(TargetAddrSpace *self)
{

}

//原型targetm.addr_space.legitimize_address #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
static rtx legitimizeAddrss_cb (TargetAddrSpace *self,rtx x, rtx orig_x ATTRIBUTE_UNUSED,
             machine_mode mode ATTRIBUTE_UNUSED)
{
   return x;
}

//原型 targetm.addr_space.subset_p (as_from, as_to) #define TARGET_ADDR_SPACE_SUBSET_P default_addr_space_subset_p
static bool subsetP_cb (TargetAddrSpace *self,addr_space_t subset,addr_space_t superset)
{
   return subset==superset;
}

//原型 targetm.addr_space.convert (op0, treeop0_type, type) #define TARGET_ADDR_SPACE_CONVERT default_addr_space_convert
static rtx convert_cb(TargetAddrSpace *self,rtx op ATTRIBUTE_UNUSED,
      tree from_type ATTRIBUTE_UNUSED,tree to_type ATTRIBUTE_UNUSED)
{
   gcc_unreachable ();
}

//原型targetm.addr_space.valid_pointer_mode#define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
static bool validPointerMode_cb(TargetAddrSpace *self,scalar_int_mode mode,addr_space_t as ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   return mtcsTarget->valid_pointer_mode (mtcsTarget,mode);
}

//原型  targetm.addr_space.zero_address_valid #define TARGET_ADDR_SPACE_ZERO_ADDRESS_VALID default_addr_space_zero_address_valid
static bool zeroAddressValid_cb(TargetAddrSpace *self,addr_space_t as ATTRIBUTE_UNUSED)
{
    return false;
}

void target_addr_space_init(TargetAddrSpace *self)
{
   //原型targetm.addr_space.legitimize_address #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
   self->legitimize_address=legitimizeAddrss_cb;
   //原型 targetm.addr_space.subset_p (as_from, as_to) #define TARGET_ADDR_SPACE_SUBSET_P default_addr_space_subset_p
   self->subset_p=subsetP_cb;
   //原型 targetm.addr_space.convert (op0, treeop0_type, type) #define TARGET_ADDR_SPACE_CONVERT default_addr_space_convert
   self->convert=convert_cb;
   //原型targetm.addr_space.valid_pointer_mode#define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
   self->valid_pointer_mode=validPointerMode_cb;
   //原型  targetm.addr_space.zero_address_valid #define TARGET_ADDR_SPACE_ZERO_ADDRESS_VALID default_addr_space_zero_address_valid
   self->zero_address_valid=zeroAddressValid_cb;
}

//原型 targetm.addr_space.legitimate_address_p (mode, addr, 0, as, ch);#define TARGET_LEGITIMATE_ADDRESS_P nvptx_legitimate_address_p
bool target_addr_space_legitimate_address_p(TargetAddrSpace *self,mtcs_mode mode ,
      rtx addr ,bool strict ,addr_space_t as, code_helper ch)
{
   return self->legitimate_address_p(self,mode,addr,strict,as, ch);
}

//原型 targetm.addr_space.address_mode*/(MEM_ADDR_SPACE (mem));#define TARGET_ADDR_SPACE_ADDRESS_MODE default_addr_space_address_mode
scalar_int_mode target_addr_space_address_mode(TargetAddrSpace *self,addr_space_t addrspace ATTRIBUTE_UNUSED)
{
   return self->address_mode(self,addrspace);
}

//原型 targetm.addr_space.legitimize_address #define TARGET_LEGITIMIZE_ADDRESS default_legitimize_address
//target也有,但没有参数addr_space_t as
rtx target_addr_space_legitimize_address(TargetAddrSpace *self,rtx x,
      rtx orig_x ATTRIBUTE_UNUSED,mtcs_mode mode ATTRIBUTE_UNUSED,addr_space_t as ATTRIBUTE_UNUSED)
{
   return self->legitimize_address(self,x,orig_x,mode,as);
}

//原型 targetm.addr_space.pointer_mode (MEM_ADDR_SPACE (target)) #define TARGET_ADDR_SPACE_POINTER_MODE default_addr_space_pointer_mode
scalar_int_mode target_addr_space_pointer_mode(TargetAddrSpace *self,addr_space_t addrspace ATTRIBUTE_UNUSED)
{
   return self->pointer_mode(self,addrspace);
}

//原型 targetm.addr_space.subset_p (as_from, as_to) #define TARGET_ADDR_SPACE_SUBSET_P default_addr_space_subset_p
bool target_addr_space_subset_p(TargetAddrSpace *self,addr_space_t subset,addr_space_t superset)
{
   return self->subset_p(self,subset,superset);
}

//原型 targetm.addr_space.convert (op0, treeop0_type, type) #define TARGET_ADDR_SPACE_CONVERT default_addr_space_convert
rtx target_addr_space_convert(TargetAddrSpace *self,rtx op ATTRIBUTE_UNUSED,
      tree from_type ATTRIBUTE_UNUSED,tree to_type ATTRIBUTE_UNUSED)
{
   return self->convert(self,op,from_type,to_type);
}
//原型targetm.addr_space.valid_pointer_mode#define TARGET_ADDR_SPACE_VALID_POINTER_MODE default_addr_space_valid_pointer_mode
bool target_addr_space_valid_pointer_mode(TargetAddrSpace *self,scalar_int_mode mode,addr_space_t as ATTRIBUTE_UNUSED)
{
   return self->valid_pointer_mode(self,mode,as);
}

//原型  targetm.addr_space.zero_address_valid #define TARGET_ADDR_SPACE_ZERO_ADDRESS_VALID default_addr_space_zero_address_valid
bool target_addr_space_zero_address_valid(TargetAddrSpace *self,addr_space_t as ATTRIBUTE_UNUSED)
{
   return self->zero_address_valid(self,as);
}

