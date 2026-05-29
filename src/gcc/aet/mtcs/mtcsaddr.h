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

#ifndef __GCC_MTCS_ADDR__
#define __GCC_MTCS_ADDR__

#include "../nlib.h"
#include "mtcscomponent.h"

//原型 insn-addr.h
typedef struct _MtcsAddr MtcsAddr;
struct _MtcsAddr
{
   MtcsComponent parent;
   //原型 insn_addresses_ insn-addr.h
   vec<int> insn_addresses_;
   //原型 insn_current_address insn-addr.h
   int insn_current_address;
};

MtcsAddr *mtcs_addr_new(MtcsMode *mtcsMode);
//原型 #define INSN_ADDRESSES(id) (insn_addresses_[id]) insn-addr.h
inline int mtcs_addr_insn_addresses(MtcsAddr *self,int id)
{
   return self->insn_addresses_[id];
}

//原型 #define INSN_ADDRESSES_ALLOC(size)        \ insn-addr.h
//  do                    \
//    {                   \
//      insn_addresses_.create (size);         \
//      insn_addresses_.safe_grow_cleared (size, true); \
//      memset (insn_addresses_.address (),    \
//         0, sizeof (int) * size);         \
//    }                   \
//  while (0)
inline void mtcs_addr_insn_addresses_alloc(MtcsAddr *self,int size)
{
   self->insn_addresses_.create (size);
   self->insn_addresses_.safe_grow_cleared (size, true);
   memset (self->insn_addresses_.address (),0, sizeof (int) * size);
}

//原型 #define INSN_ADDRESSES_FREE() (insn_addresses_.release ()) insn-addr.h
inline void mtcs_addr_insn_addresses_free(MtcsAddr *self)
{
   self->insn_addresses_.release ();
}

//原型 #define INSN_ADDRESSES_SET_P() (insn_addresses_.exists ()) insn-addr.h
inline bool mtcs_addr_insn_addresses_set_p(MtcsAddr *self)
{
   return self->insn_addresses_.exists ();
}
//原型 #define INSN_ADDRESSES_SIZE() (insn_addresses_.length ()) insn-addr.h
inline int mtcs_addr_insn_addresses_size(MtcsAddr *self)
{
   return self->insn_addresses_.length ();
}

//原型 insn_addresses_new (rtx_insn *insn, int insn_addr)
//原型 #define INSN_ADDRESSES_NEW(insn, addr) (insn_addresses_new (insn, addr))
void mtcs_addr_insn_addresses_new (MtcsAddr *self,rtx_insn *insn, int insn_addr);

#endif
