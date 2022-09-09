/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef __N_MEM_H__
#define __N_MEM_H__



#include "nbase.h"


extern nboolean n_mem_gc_friendly;

void	 n_free	          (npointer	 mem);
void     n_clear_pointer  (npointer *pp,NDestroyNotify destroy);
npointer n_malloc         (nsize n_bytes) ;
npointer n_malloc0        (nsize n_bytes);
npointer n_realloc        (npointer	mem,nsize n_bytes) ;
npointer n_malloc0_n      (nsize n_blocks,nsize n_block_bytes);
npointer n_memdup (nconstpointer mem,nuint  byte_size);
npointer n_try_malloc_n   (nsize	 n_blocks,nsize	 n_block_bytes) ;
npointer n_try_malloc (nsize n_bytes);
//#ifdef GCC_COMPILE_AET
npointer n_xrealloc_n      (npointer	 mem,nsize n_blocks, nsize	 n_block_bytes);
npointer n_xmalloc_n      (nsize n_blocks,nsize n_block_bytes);
//#else
//npointer n_realloc_n      (npointer	 mem,nsize n_blocks, nsize	 n_block_bytes);
//npointer n_malloc_n      (nsize n_blocks,nsize n_block_bytes);
//#endif
///* Unoptimised version: always call the _n() function. */

#define _N_NEW(struct_type, n_structs, func) \
        ((struct_type *) n_##func##_n ((n_structs), sizeof (struct_type)))
#define _N_RENEW(struct_type, mem, n_structs, func) \
        ((struct_type *) n_##func##_n (mem, (n_structs), sizeof (struct_type)))



#define n_new(struct_type, n_structs)			_N_NEW (struct_type, n_structs, xmalloc)

#define n_new0(struct_type, n_structs)			_N_NEW (struct_type, n_structs, malloc0)

#define n_renew(struct_type, mem, n_structs)	_N_RENEW (struct_type, mem, n_structs, xrealloc)

static inline npointer n_steal_pointer (npointer pp)
{
  npointer *ptr = (npointer *) pp;
  npointer ref;

  ref = *ptr;
  *ptr = NULL;

  return ref;
}


#define n_steal_pointer(pp) \
  (0 ? (*(pp)) : (n_steal_pointer) (pp))



#endif /* __N_MEM_H__ */
