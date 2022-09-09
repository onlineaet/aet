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

#ifndef __N_SLICE_H__
#define __N_SLICE_H__



#include <string.h>
#include "nbase.h"



npointer n_slice_alloc          	    (nsize	block_size);
npointer n_slice_alloc0         	    (nsize  block_size) ;
npointer n_slice_copy                   (nsize  block_size,nconstpointer mem_block) ;
void     n_slice_free1          	    (nsize  block_size, npointer      mem_block);
void     n_slice_free_chain_with_offset (nsize mem_size,npointer mem_chain,nsize next_offset);

#define  n_slice_new(type)      ((type*) n_slice_alloc (sizeof (type)))
#define  n_slice_new0(type)    ((type*) n_slice_alloc0 (sizeof (type)))


/* we go through extra hoops to ensure type safety */
#define n_slice_dup(type, mem)                                  \
  (1 ? (type*) n_slice_copy (sizeof (type), (mem))              \
     : ((void) ((type*) 0 == (mem)), (type*) 0))

#define n_slice_free(type, mem)                                 \
N_STMT_START {                                                  \
  if (1) n_slice_free1 (sizeof (type), (mem));			\
  else   (void) ((type*) 0 == (mem)); 				\
} N_STMT_END

#define n_slice_free_chain(type, mem_chain, next)               \
N_STMT_START {                                                  \
  if (1) n_slice_free_chain_with_offset (sizeof (type),		\
                 (mem_chain), N_STRUCT_OFFSET (type, next)); 	\
  else   (void) ((type*) 0 == (mem_chain));			\
} N_STMT_END




#endif /* __N_SLICE_H__ */
