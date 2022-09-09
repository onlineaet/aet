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


#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <malloc.h>


#include <stdio.h>
#include "nbase.h"

#include "nslice.h"
#include "nmem.h"



npointer n_slice_alloc (nsize mem_size)
{
	npointer mem;
    mem = n_malloc (mem_size);
    return mem;
}


npointer n_slice_alloc0 (nsize mem_size)
{
  npointer mem = n_slice_alloc (mem_size);
  if (mem)
    memset (mem, 0, mem_size);
  return mem;
}


npointer n_slice_copy (nsize mem_size,nconstpointer mem_block)
{
  npointer mem = n_slice_alloc (mem_size);
  if (mem)
    memcpy (mem, mem_block, mem_size);
  return mem;
}


void n_slice_free1 (nsize    mem_size,    npointer mem_block)
{

  if (N_UNLIKELY (!mem_block))
    return;
  n_free (mem_block);
}

void n_slice_free_chain_with_offset (nsize mem_size,npointer mem_chain,nsize next_offset)
{
    npointer slice = mem_chain;
    while (slice){
        nuint8 *current = slice;
        slice = *(npointer*) (current + next_offset);
        n_free (current);
   }
}









