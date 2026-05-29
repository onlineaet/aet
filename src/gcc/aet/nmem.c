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
#include <signal.h>
#include <stdio.h>

#include "nmem.h"
#include "nlog.h"


#define SIZE_OVERFLOWS(a,b) (N_UNLIKELY ((b) > 0 && (a) > N_MAXSIZE / (b)))

nboolean n_mem_gc_friendly = FALSE;


npointer n_malloc (nsize n_bytes)
{
  if (N_LIKELY (n_bytes))
    {
      npointer mem;

      mem = xmalloc (n_bytes);
      if (mem)
	    return mem;

      n_error("failed to malloc %d\n",n_bytes);
    }

  return NULL;
}


npointer n_malloc0 (nsize n_bytes)
{
  if (N_LIKELY (n_bytes))
    {
	  npointer mem;
      mem = xcalloc (1, n_bytes);
      if (mem)
	   return mem;
      n_error("failed to calloc %d\n",n_bytes);

    }
  return NULL;
}


npointer n_realloc (npointer mem,nsize    n_bytes)
{
  npointer newmem;

  if ( N_LIKELY (n_bytes))
    {
      newmem = xrealloc (mem, n_bytes);
      if (newmem)
	    return newmem;
      n_error("failed to realloc %d\n",n_bytes);
    }

  free (mem);

  return NULL;
}


void n_free (npointer mem)
{
  free (mem);
}


void n_clear_pointer (npointer *pp,NDestroyNotify destroy)
{
  npointer _p;
  _p = *pp;
  if (_p)
    {
      *pp = NULL;
      destroy (_p);
    }
}

npointer n_malloc0_n (nsize n_blocks,nsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    {
     // n_error ("%s: overflow allocating %"N_NSIZE_FORMAT"*%"N_NSIZE_FORMAT" bytes",N_STRLOC, n_blocks, n_block_bytes);
      n_error ("%s: overflow allocating %l*%l bytes",N_STRLOC, n_blocks, n_block_bytes);

    }

  return n_malloc0 (n_blocks * n_block_bytes);
}

npointer n_xmalloc_n (nsize n_blocks,nsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    {
     // n_error ("%s: overflow allocating %"N_NSIZE_FORMAT"*%"N_NSIZE_FORMAT" bytes",N_STRLOC, n_blocks, n_block_bytes);
      n_error ("%s: overflow allocating %l*%l bytes",N_STRLOC, n_blocks, n_block_bytes);

    }

  return n_malloc(n_blocks * n_block_bytes);
}

npointer n_xrealloc_n (npointer mem,nsize    n_blocks, nsize    n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    {
      n_error ("%s: overflow allocating %"N_NSIZE_FORMAT"*%"N_NSIZE_FORMAT" bytes",
               N_STRLOC, n_blocks, n_block_bytes);
    }

  return n_realloc (mem, n_blocks * n_block_bytes);
}


npointer n_memdup (nconstpointer mem,nuint  byte_size)
{
  npointer new_mem;

  if (mem && byte_size != 0)
    {
      new_mem = n_malloc (byte_size);
      memcpy (new_mem, mem, byte_size);
    }
  else
    new_mem = NULL;

  return new_mem;
}

npointer n_try_malloc (nsize n_bytes)
{
	npointer mem;

  if (N_LIKELY (n_bytes))
    mem = xmalloc (n_bytes);
  else
    mem = NULL;


  return mem;
}

npointer n_try_malloc_n (nsize n_blocks,nsize n_block_bytes)
{
  if (SIZE_OVERFLOWS (n_blocks, n_block_bytes))
    return NULL;
  return n_try_malloc (n_blocks * n_block_bytes);
}



