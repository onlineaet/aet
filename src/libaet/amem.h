#ifndef __N_MEM_H__
#define __N_MEM_H__

#include "abase.h"
# include <alloca.h>


extern aboolean a_mem_gc_friendly;

void	 a_free(apointer	 mem);
void     a_clear_pointer(apointer *pp,ADestroyNotify destroy);
apointer a_malloc(asize a_bytes) ;
apointer a_malloc0(asize a_bytes);
apointer a_realloc(apointer	mem,asize a_bytes) ;
apointer a_malloc0_n(asize a_blocks,asize a_block_bytes);
apointer a_memdup(aconstpointer mem,auint  byte_size);
apointer a_try_malloc_n(asize a_blocks,asize	 a_block_bytes) ;
apointer a_try_malloc(asize a_bytes);
apointer a_xrealloc_n(apointer mem,asize a_blocks, asize a_block_bytes);
apointer a_xmalloc_n(asize a_blocks,asize a_block_bytes);
apointer a_mem_expand (void *array,auint *origCount,auint uninLen,auint expendCount);
apointer a_try_realloc (apointer mem,asize nBytes);


#define _A_NEW(struct_type, a_structs, func) \
        ((struct_type *) a_##func##_n ((a_structs), sizeof (struct_type)))
#define _A_RENEW(struct_type, mem, a_structs, func) \
        ((struct_type *) a_##func##_n (mem, (a_structs), sizeof (struct_type)))



#define a_new(struct_type, a_structs)			_A_NEW (struct_type, a_structs, xmalloc)

#define a_new0(struct_type, a_structs)			_A_NEW (struct_type, a_structs, malloc0)

#define a_renew(struct_type, mem, a_structs)	_A_RENEW (struct_type, mem, a_structs, xrealloc)

static inline apointer a_steal_pointer (apointer pp)
{
  apointer *ptr = (apointer *) pp;
  apointer ref;

  ref = *ptr;
  *ptr = NULL;

  return ref;
}

#define a_steal_pointer(pp) ((__typeof__ (*pp)) (a_steal_pointer) (pp))
//#define a_steal_pointer(pp)   (0 ? (*(pp)) : (a_steal_pointer) (pp))

/**
 * 申请栈内存
 */
#define a_alloca(size)		 alloca (size)
#define a_newa(struct_type, n_structs)	((struct_type*) a_alloca (sizeof (struct_type) * (asize) (n_structs)))

#endif /* __N_MEM_H__ */

