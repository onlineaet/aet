#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "amem.h"
#include "alog.h"


#define SIZE_OVERFLOWS(a,b) (A_UNLIKELY ((b) > 0 && (a) > A_MAXSIZE / (b)))

aboolean a_mem_gc_friendly = FALSE;


apointer a_malloc (asize a_bytes)
{
  if (A_LIKELY (a_bytes))
    {
      apointer mem;

      mem = xmalloc (a_bytes);
      if (mem)
	    return mem;
      printf("a_malloc failed to calloc %lu\n",a_bytes);
      a_abort();

    }

  return NULL;
}


apointer a_malloc0 (asize a_bytes)
{
  if (A_LIKELY (a_bytes))
    {
	  apointer mem;
      mem = xcalloc (1, a_bytes);
      if (mem)
	   return mem;
      printf("a_malloc0 failed to calloc %lu\n",a_bytes);
      a_abort();

    }
  return NULL;
}


apointer a_realloc (apointer mem,asize a_bytes)
{
  apointer newmem;

  if ( A_LIKELY (a_bytes))
    {
      newmem = xrealloc (mem, a_bytes);
      if (newmem)
	    return newmem;
      printf("a_realloc failed to realloc %lu\n",a_bytes);
      a_abort();
    }

  free (mem);

  return NULL;
}


void a_free (apointer mem)
{
  free (mem);
}


void a_clear_pointer (apointer *pp,ADestroyNotify destroy)
{
  apointer _p;
  _p = *pp;
  if (_p)
    {
      *pp = NULL;
      destroy (_p);
    }
}

apointer a_malloc0_n (asize a_blocks,asize a_block_bytes)
{
  if (SIZE_OVERFLOWS (a_blocks, a_block_bytes)){
      printf("a_malloc0_n overflow allocating blocks:%lu size:%lu\n",a_blocks,a_block_bytes);
      a_abort();
  }
  return a_malloc0 (a_blocks * a_block_bytes);
}

apointer a_xmalloc_n (asize a_blocks,asize a_block_bytes)
{
  if (SIZE_OVERFLOWS (a_blocks, a_block_bytes))
    {
      printf("a_xmalloc_n overflow allocating blocks:%lu size:%lu\n",a_blocks,a_block_bytes);
      a_abort();

    }

  return a_malloc(a_blocks * a_block_bytes);
}

apointer a_xrealloc_n (apointer mem,asize a_blocks, asize a_block_bytes)
{
  if (SIZE_OVERFLOWS (a_blocks, a_block_bytes)){
      printf("a_xmalloc_n overflow allocating blocks:%lu size:%lu\n",a_blocks,a_block_bytes);
      a_abort();
  }
  return a_realloc (mem, a_blocks * a_block_bytes);
}


apointer a_memdup (aconstpointer mem,auint  byte_size)
{
  apointer new_mem;

  if (mem && byte_size != 0)
    {
      new_mem = a_malloc (byte_size);
      memcpy (new_mem, mem, byte_size);
    }
  else
    new_mem = NULL;

  return new_mem;
}

apointer a_try_malloc (asize a_bytes)
{
	apointer mem;

  if (A_LIKELY (a_bytes))
    mem = xmalloc (a_bytes);
  else
    mem = NULL;


  return mem;
}

apointer a_try_malloc_n (asize a_blocks,asize a_block_bytes)
{
  if (SIZE_OVERFLOWS (a_blocks, a_block_bytes))
    return NULL;
  return a_try_malloc (a_blocks * a_block_bytes);
}

static auint nearestPow (auint num){
   auint n = num - 1;
   if(num<=0){
       printf("nearestPow num<=0 %d",num);
       exit(-1);
   }
   n |= n >> 1;
   n |= n >> 2;
   n |= n >> 4;
   n |= n >> 8;
   n |= n >> 16;
   return n + 1;
}

#define MIN_ARRAY_SIZE 16

/**
 * 扩展array的内存大小。
 * origCount原内存数据
 * uninLen origCount的单位。
 * expendCount 扩展的数量
 */
apointer a_mem_expand (void *array,auint *oldCount,auint uninLen,auint expendCount)
{
   auint want_alloc;
   auint origCount=*oldCount;
   if A_UNLIKELY ((A_MAXUINT - origCount) < expendCount)
     a_error("正在加入到array的内存 %u 溢出。单位长:%u", expendCount,uninLen);
   want_alloc = (origCount+expendCount);
   if (want_alloc > origCount) {
      want_alloc = nearestPow (want_alloc);
      want_alloc = MAX (want_alloc, MIN_ARRAY_SIZE);
      array = a_realloc (array, want_alloc*uninLen);
      memset (array + origCount*uninLen, 0, (want_alloc - origCount)*uninLen);
      *oldCount=want_alloc;
      return array;
   }
   return 0;
}

apointer a_try_realloc (apointer mem,asize nBytes)
{
  apointer newmem;
  if (A_LIKELY (nBytes))
    newmem = realloc (mem, nBytes);
  else{
      newmem = NULL;
      free (mem);
  }
  return newmem;
}



