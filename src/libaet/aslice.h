#ifndef __A_SLICE_H__
#define __A_SLICE_H__

#include <string.h>
#include "abase.h"



/* slices - fast allocation/release of small memory blocks
 */
apointer a_slice_alloc    (asize block_size) A_GNUC_MALLOC A_GNUC_ALLOC_SIZE(1);
apointer a_slice_alloc0   (asize  block_size) A_GNUC_MALLOC A_GNUC_ALLOC_SIZE(1);
apointer a_slice_copy (asize block_size,aconstpointer mem_block) A_GNUC_ALLOC_SIZE(1);
void     a_slice_free1 (asize block_size,apointer mem_block);
void     a_slice_free_chain_with_offset (asize block_size,apointer mem_chain, asize  next_offset);

#define  a_slice_new(type)      ((type*) a_slice_alloc (sizeof (type)))

/* Allow the compiler to inline memset(). Since the size is a constant, this
 * can significantly improve performance. */
#  define a_slice_new0(type)                                    \
  (type *) (A_GNUC_EXTENSION ({                                 \
    asize __s = sizeof (type);                                  \
    apointer __p;                                               \
    __p = a_slice_alloc (__s);                                  \
    memset (__p, 0, __s);                                       \
    __p;                                                        \
  }))


/* MemoryBlockType *
 *       a_slice_dup                    (MemoryBlockType,
 *	                                 MemoryBlockType *mem_block);
 *       a_slice_free                   (MemoryBlockType,
 *	                                 MemoryBlockType *mem_block);
 *       a_slice_free_chain             (MemoryBlockType,
 *                                       MemoryBlockType *first_chain_block,
 *                                       memory_block_next_field);
 * pseudo prototypes for the macro
 * definitions following below.
 */

/* we go through extra hoops to ensure type safety */
#define a_slice_dup(type, mem)                                  \
  (1 ? (type*) a_slice_copy (sizeof (type), (mem))              \
     : ((void) ((type*) 0 == (mem)), (type*) 0))
#define a_slice_free(type, mem)                                 \
A_STMT_START {                                                  \
  if (1) a_slice_free1 (sizeof (type), (mem));			\
  else   (void) ((type*) 0 == (mem)); 				\
} A_STMT_END
#define a_slice_free_chain(type, mem_chain, next)               \
A_STMT_START {                                                  \
  if (1) a_slice_free_chain_with_offset (sizeof (type),		\
                 (mem_chain), A_STRUCT_OFFSET (type, next)); 	\
  else   (void) ((type*) 0 == (mem_chain));			\
} A_STMT_END

/* --- internal debugging API --- */
typedef enum {
  A_SLICE_CONFIA_ALWAYS_MALLOC = 1,
  A_SLICE_CONFIA_BYPASS_MAGAZINES,
  A_SLICE_CONFIA_WORKINA_SET_MSECS,
  A_SLICE_CONFIA_COLOR_INCREMENT,
  A_SLICE_CONFIA_CHUNK_SIZES,
  A_SLICE_CONFIA_CONTENTION_COUNTER
} ASliceConfig;



#endif /* __A_SLICE_H__ */


