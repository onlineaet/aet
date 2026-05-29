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
#include "memmodel.h"
#include "tm_p.h"
#include "ssa.h"
#include "expmed.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "stor-layout.h"
#include "tree-ssa-live.h"
#include "tree-outof-ssa.h"
#include "cfgexpand.h"
#include "ccmp.h"
#include "predict.h"

#include "mtcstarget.h"
#include "mtcsaddr.h"


static void mtcsAddrInit(MtcsAddr *self)
{
}

//原型 insn_addresses_new (rtx_insn *insn, int insn_addr)
//原型 #define INSN_ADDRESSES_NEW(insn, addr) (insn_addresses_new (insn, addr))
void mtcs_addr_insn_addresses_new (MtcsAddr *self,rtx_insn *insn, int insn_addr)
{
   unsigned insn_uid = INSN_UID ((insn));

   if (mtcs_addr_insn_addresses_set_p/*!INSN_ADDRESSES_SET_P*/(self)){
      size_t size = mtcs_addr_insn_addresses_size/*!INSN_ADDRESSES_SIZE*/(self);
      if (size <= insn_uid){
         int *p;
         self->insn_addresses_.safe_grow (insn_uid + 1, true);
         p = self->insn_addresses_.address ();
         memset (&p[size], 0, sizeof (int) * (insn_uid + 1 - size));
      }
      self->insn_addresses_[insn_uid]/*!INSN_ADDRESSES (insn_uid)*/ = insn_addr;
   }
}

MtcsAddr *mtcs_addr_new(MtcsMode *mtcsMode)
{
     MtcsAddr *self = n_slice_alloc0 (sizeof(MtcsAddr));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsAddrInit(self);
     return self;
}

