
#include <stdlib.h>             /* posix_memalign() */
#include <string.h>
#include <errno.h>
#include <unistd.h>             /* sysconf() */
#include <stdio.h>              /* fputs */
#include <stdarg.h>              /* fputs */
#include <sys/time.h>

#include "aslice.h"
#include "amem.h"
#include "athreadspecific.h"
#include "innerlock.h"



#define LARGEALIGNMENT          (256)
#define P2ALIGNMENT             (2 * sizeof (asize))                            /* fits 2 pointers (assumed to be 2 * ALIB_SIZEOF_SIZE_T below) */
#define ALIGN(size, base)       ((base) * (asize) (((size) + (base) - 1) / (base)))
#define NATIVE_MALLOC_PADDING   P2ALIGNMENT                                     /* per-page padding left for native malloc(3) see [1] */
#define SLAB_INFO_SIZE          P2ALIGN (sizeof (SlabInfo) + NATIVE_MALLOC_PADDING)
#define MAX_MAGAZINE_SIZE       (256)                                           /* see [3] and allocator_get_magazine_threshold() for this */
#define MIN_MAGAZINE_SIZE       (4)
#define MAX_STAMP_COUNTER       (7)                                             /* distributes the load of gettimeofday() */
#define MAX_SLAB_CHUNK_SIZE(al) (((al)->max_page_size - SLAB_INFO_SIZE) / 8)    /* we want at last 8 chunks per page, see [4] */
#define MAX_SLAB_INDEX(al)      (SLAB_INDEX (al, MAX_SLAB_CHUNK_SIZE (al)) + 1)
#define SLAB_INDEX(al, asize)   ((asize) / P2ALIGNMENT - 1)                     /* asize must be P2ALIGNMENT aligned */
#define SLAB_CHUNK_SIZE(al, ix) (((ix) + 1) * P2ALIGNMENT)
#define SLAB_BPAGE_SIZE(al,csz) (8 * (csz) + SLAB_INFO_SIZE)

/* optimized version of ALIGN (size, P2ALIGNMENT) */
#if     ALIB_SIZEOF_SIZE_T * 2 == 8  /* P2ALIGNMENT */
#define P2ALIGN(size)   (((size) + 0x7) & ~(asize) 0x7)
#elif   ALIB_SIZEOF_SIZE_T * 2 == 16 /* P2ALIGNMENT */
#define P2ALIGN(size)   (((size) + 0xf) & ~(asize) 0xf)
#else
#define P2ALIGN(size)   ALIGN (size, P2ALIGNMENT)
#endif

/* special helpers to avoid gmessage.c dependency */
static void mem_error (const char *format, ...) A_GNUC_PRINTF (1,2);
#define mem_assert(cond)    do { if (A_LIKELY (cond)) ; else mem_error ("assertion failed: %s", #cond); } while (0)

/* --- structures --- */
typedef struct _ChunkLink      ChunkLink;
typedef struct _SlabInfo       SlabInfo;
typedef struct _CachedMagazine CachedMagazine;

struct _ChunkLink {
  ChunkLink *next;
  ChunkLink *data;
};

struct _SlabInfo {
  ChunkLink *chunks;
  auint n_allocated;
  SlabInfo *next, *prev;
};

typedef struct {
  ChunkLink *chunks;
  asize      count;                     /* approximative chunks list length */
} Magazine;

typedef struct {
  Magazine   *magazine1;                /* array of MAX_SLAB_INDEX (allocator) */
  Magazine   *magazine2;                /* array of MAX_SLAB_INDEX (allocator) */
} ThreadMemory;

typedef struct {
  aboolean always_malloc;
  aboolean bypass_magazines;
  aboolean debug_blocks;
  asize    workina_set_msecs;
  auint    color_increment;
} SliceConfig;

typedef struct {
  /* const after initialization */
  asize         min_page_size, max_page_size;
  SliceConfig   config;
  asize         max_slab_chunk_size_for_magazine_cache;
  /* magazine cache */
  InnerMutex        magazine_mutex;
  ChunkLink   **magazines;                /* array of MAX_SLAB_INDEX (allocator) */
  auint        *contention_counters;      /* array of MAX_SLAB_INDEX (allocator) */
  aint          mutex_counter;
  auint         stamp_counter;
  auint         last_stamp;
  /* slab allocator */
  InnerMutex        slab_mutex;
  SlabInfo    **slab_stack;                /* array of MAX_SLAB_INDEX (allocator) */
  auint        color_accu;
} Allocator;

typedef struct _ATrashStack ATrashStack ;
struct _ATrashStack
{
  ATrashStack *next;
};

static aint64 a_get_real_time (void);

/* ---ATrashStack prototypes --- */
static apointer trash_stack_pop (ATrashStack **stack_p);
static void     trash_stack_push (ATrashStack **stack_p,apointer data_p);

/* --- g-slice prototypes --- */
static apointer     slab_allocator_alloc_chunk       (asize chunk_size);
static void         slab_allocator_free_chunk        (asize  chunk_size,apointer   mem);
static void         private_thread_memory_cleanup    (apointer data);
static apointer     allocator_memalign               (asize alignment,asize memsize);
static void         allocator_memfree                (asize memsize,apointer mem);
static inline void  magazine_cache_update_stamp      (void);
static inline asize allocator_get_magazine_threshold (Allocator *allocator,auint ix);

/* --- g-slice memory checker --- */
static void     smc_notify_alloc(void   *pointer,size_t  size);
static int      smc_notify_free   (void   *pointer,size_t  size);

/* --- variables --- */
static AThreadSpecific    private_thread_memory = {NULL,(private_thread_memory_cleanup)};
static asize       sys_page_size = 0;
static Allocator   allocator[1] = { { 0, }, };
static SliceConfig slice_config = {
  FALSE,        /* always_malloc */
  FALSE,        /* bypass_magazines */
  FALSE,        /* debug_blocks */
  15 * 1000,    /* workina_set_msecs */
  1,            /* color increment, alt: 0x7fffffff */
};
static InnerMutex      smc_tree_mutex; /* mutex for A_SLICE=debug-blocks */

/* --- auxiliary functions --- */
void a_slice_set_config (ASliceConfig ckey,aint64 value)
{
  if (sys_page_size != 0)
	  return;
  switch (ckey)
    {
    case A_SLICE_CONFIA_ALWAYS_MALLOC:
      slice_config.always_malloc = value != 0;
      break;
    case A_SLICE_CONFIA_BYPASS_MAGAZINES:
      slice_config.bypass_magazines = value != 0;
      break;
    case A_SLICE_CONFIA_WORKINA_SET_MSECS:
      slice_config.workina_set_msecs = value;
      break;
    case A_SLICE_CONFIA_COLOR_INCREMENT:
      slice_config.color_increment = value;
      break;
    default: ;
    }
}

aint64 a_slice_get_config (ASliceConfig ckey)
{
  switch (ckey)
    {
    case A_SLICE_CONFIA_ALWAYS_MALLOC:
      return slice_config.always_malloc;
    case A_SLICE_CONFIA_BYPASS_MAGAZINES:
      return slice_config.bypass_magazines;
    case A_SLICE_CONFIA_WORKINA_SET_MSECS:
      return slice_config.workina_set_msecs;
    case A_SLICE_CONFIA_CHUNK_SIZES:
      return MAX_SLAB_INDEX (allocator);
    case A_SLICE_CONFIA_COLOR_INCREMENT:
      return slice_config.color_increment;
    default:
      return 0;
    }
}

aint64* a_slice_get_config_state (ASliceConfig ckey,aint64 address,auint *n_values)
{
  auint i = 0;
  if(n_values==NULL)
	  return NULL;
  *n_values = 0;
  switch (ckey)
    {
      aint64 array[64];
    case A_SLICE_CONFIA_CONTENTION_COUNTER:
      array[i++] = SLAB_CHUNK_SIZE (allocator, address);
      array[i++] = allocator->contention_counters[address];
      array[i++] = allocator_get_magazine_threshold (allocator, address);
      *n_values = i;
      return a_memdup (array, sizeof (array[0]) * *n_values);
    default:
      return NULL;
    }
}

static void slice_config_init (SliceConfig *config)
{
  const achar *val;
  achar *val_allocated = NULL;
  *config = slice_config;
  /* Note that the empty string (`A_SLICE=""`) is treated differently from the
   * envvar being unset. In the latter case, we also check whether running under
   * valgrind. */
   val = getenv ("A_SLICE");
   if (val != NULL){
        config->always_malloc = TRUE;
        config->debug_blocks = TRUE;
   }
   a_free (val_allocated);
}

static void a_slice_init_nomessage (void)
{
  /* we may not use a_error() or friends here */
  mem_assert (sys_page_size == 0);
  mem_assert (MIN_MAGAZINE_SIZE >= 4);

  sys_page_size = sysconf (_SC_PAGESIZE); /* = sysconf (_SC_PAGE_SIZE); = getpagesize(); */
  mem_assert (sys_page_size >= 2 * LARGEALIGNMENT);
  mem_assert ((sys_page_size & (sys_page_size - 1)) == 0);
  slice_config_init (&allocator->config);
  allocator->min_page_size = sys_page_size;
  /* we can only align to system page size */
  allocator->max_page_size = sys_page_size;
  if (allocator->config.always_malloc){
      allocator->contention_counters = NULL;
      allocator->magazines = NULL;
      allocator->slab_stack = NULL;
   }else{
      allocator->contention_counters = a_new0 (auint, MAX_SLAB_INDEX (allocator));
      allocator->magazines = a_new0 (ChunkLink*, MAX_SLAB_INDEX (allocator));
      allocator->slab_stack = a_new0 (SlabInfo*, MAX_SLAB_INDEX (allocator));
   }

  allocator->mutex_counter = 0;
  allocator->stamp_counter = MAX_STAMP_COUNTER; /* force initial update */
  allocator->last_stamp = 0;
  allocator->color_accu = 0;
  magazine_cache_update_stamp();
  /* values cached for performance reasons */
  allocator->max_slab_chunk_size_for_magazine_cache = MAX_SLAB_CHUNK_SIZE (allocator);
  if (allocator->config.always_malloc || allocator->config.bypass_magazines)
    allocator->max_slab_chunk_size_for_magazine_cache = 0;      /* non-optimized cases */
}

static inline auint allocator_categorize (asize aligned_chunk_size)
{
  /* speed up the likely path */
  if (A_LIKELY (aligned_chunk_size && aligned_chunk_size <= allocator->max_slab_chunk_size_for_magazine_cache))
    return 1;           /* use magazine cache */

  if (!allocator->config.always_malloc && aligned_chunk_size && aligned_chunk_size <= MAX_SLAB_CHUNK_SIZE (allocator)){
      if (allocator->config.bypass_magazines)
        return 2;       /* use slab allocator, see [2] */
      return 1;         /* use magazine cache */
  }
  return 0;             /* use malloc() */
}

static inline void inner_mutex_lock_a (InnerMutex *mutex,auint  *contention_counter)
{
  aboolean contention = FALSE;
  if (!inner_mutex_trylock (mutex)){
	  inner_mutex_lock (mutex);
      contention = TRUE;
  }
  if (contention){
      allocator->mutex_counter++;
      if (allocator->mutex_counter >= 1){       /* quickly adapt to contention */

          allocator->mutex_counter = 0;
          *contention_counter = MIN (*contention_counter + 1, MAX_MAGAZINE_SIZE);
      }
  }else {/* !contention */

      allocator->mutex_counter--;
      if (allocator->mutex_counter < -11){       /* moderately recover magazine sizes */
          allocator->mutex_counter = 0;
          *contention_counter = MAX (*contention_counter, 1) - 1;
      }
  }
}

static inline ThreadMemory* thread_memory_from_self (void)
{
  ThreadMemory *tmem = a_thread_specific_get (&private_thread_memory);
  if (A_UNLIKELY (!tmem)){
      static InnerMutex init_mutex;
      auint n_magazines;

      inner_mutex_lock (&init_mutex);
      if A_UNLIKELY (sys_page_size == 0)
        a_slice_init_nomessage ();
      inner_mutex_unlock (&init_mutex);

      n_magazines = MAX_SLAB_INDEX (allocator);
      tmem = a_thread_specific_set_alloc0 (&private_thread_memory, sizeof (ThreadMemory) + sizeof (Magazine) * 2 * n_magazines);
      tmem->magazine1 = (Magazine*) (tmem + 1);
      tmem->magazine2 = &tmem->magazine1[n_magazines];
  }
  return tmem;
}

static inline ChunkLink* magazine_chain_pop_head (ChunkLink **magazine_chunks)
{
  /* magazine chains are linked via ChunkLink->next.
   * each ChunkLink->data of the toplevel chain may point to a subchain,
   * linked via ChunkLink->next. ChunkLink->data of the subchains just
   * contains uninitialized junk.
   */
  ChunkLink *chunk = (*magazine_chunks)->data;
  if (A_UNLIKELY (chunk))
    {
      /* allocating from freed list */
      (*magazine_chunks)->data = chunk->next;
    }
  else
    {
      chunk = *magazine_chunks;
      *magazine_chunks = chunk->next;
    }
  return chunk;
}

#if 0 /* useful for debugging */
static auint
magazine_count (ChunkLink *head)
{
  auint count = 0;
  if (!head)
    return 0;
  while (head)
    {
      ChunkLink *child = head->data;
      count += 1;
      for (child = head->data; child; child = child->next)
        count += 1;
      head = head->next;
    }
  return count;
}
#endif

static inline asize allocator_get_magazine_threshold (Allocator *allocator,auint      ix)
{
  /* the magazine size calculated here has a lower bound of MIN_MAGAZINE_SIZE,
   * which is required by the implementation. also, for moderately sized chunks
   * (say >= 64 bytes), magazine sizes shouldn't be much smaller then the number
   * of chunks available per page/2 to avoid excessive traffic in the magazine
   * cache for small to medium sized structures.
   * the upper bound of the magazine size is effectively provided by
   * MAX_MAGAZINE_SIZE. for larger chunks, this number is scaled down so that
   * the content of a single magazine doesn't exceed ca. 16KB.
   */
  asize chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
  auint threshold = MAX (MIN_MAGAZINE_SIZE, allocator->max_page_size / MAX (5 * chunk_size, 5 * 32));
  auint contention_counter = allocator->contention_counters[ix];
  if (A_UNLIKELY (contention_counter))  /* single CPU bias */
    {
      /* adapt contention counter thresholds to chunk sizes */
      contention_counter = contention_counter * 64 / chunk_size;
      threshold = MAX (threshold, contention_counter);
    }
  return threshold;
}

/* --- magazine cache --- */
static inline void magazine_cache_update_stamp (void)
{
  if (allocator->stamp_counter >= MAX_STAMP_COUNTER)
    {
      aint64 now_us = a_get_real_time ();
      allocator->last_stamp = now_us / 1000; /* milli seconds */
      allocator->stamp_counter = 0;
    }
  else
    allocator->stamp_counter++;
}

static inline ChunkLink* magazine_chain_prepare_fields (ChunkLink *magazine_chunks)
{
  ChunkLink *chunk1;
  ChunkLink *chunk2;
  ChunkLink *chunk3;
  ChunkLink *chunk4;
  /* checked upon initialization: mem_assert (MIN_MAGAZINE_SIZE >= 4); */
  /* ensure a magazine with at least 4 unused data pointers */
  chunk1 = magazine_chain_pop_head (&magazine_chunks);
  chunk2 = magazine_chain_pop_head (&magazine_chunks);
  chunk3 = magazine_chain_pop_head (&magazine_chunks);
  chunk4 = magazine_chain_pop_head (&magazine_chunks);
  chunk4->next = magazine_chunks;
  chunk3->next = chunk4;
  chunk2->next = chunk3;
  chunk1->next = chunk2;
  return chunk1;
}

/* access the first 3 fields of a specially prepared magazine chain */
#define magazine_chain_prev(mc)         ((mc)->data)
#define magazine_chain_stamp(mc)        ((mc)->next->data)
#define magazine_chain_uint_stamp(mc)   APOINTER_TO_UINT ((mc)->next->data)
#define magazine_chain_next(mc)         ((mc)->next->next->data)
#define magazine_chain_count(mc)        ((mc)->next->next->next->data)

static void magazine_cache_trim (Allocator *allocator,auint ix,auint stamp)
{
  /* a_mutex_lock (allocator->mutex); done by caller */
  /* trim magazine cache from tail */
  ChunkLink *current = magazine_chain_prev (allocator->magazines[ix]);
  ChunkLink *trash = NULL;
  while (!A_APPROX_VALUE(stamp, magazine_chain_uint_stamp (current),
                         allocator->config.workina_set_msecs))
    {
      /* unlink */
      ChunkLink *prev = magazine_chain_prev (current);
      ChunkLink *next = magazine_chain_next (current);
      magazine_chain_next (prev) = next;
      magazine_chain_prev (next) = prev;
      /* clear special fields, put on trash stack */
      magazine_chain_next (current) = NULL;
      magazine_chain_count (current) = NULL;
      magazine_chain_stamp (current) = NULL;
      magazine_chain_prev (current) = trash;
      trash = current;
      /* fixup list head if required */
      if (current == allocator->magazines[ix])
        {
          allocator->magazines[ix] = NULL;
          break;
        }
      current = prev;
    }
  inner_mutex_unlock (&allocator->magazine_mutex);
  /* free trash */
  if (trash)
    {
      const asize chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
      inner_mutex_lock (&allocator->slab_mutex);
      while (trash)
        {
          current = trash;
          trash = magazine_chain_prev (current);
          magazine_chain_prev (current) = NULL; /* clear special field */
          while (current)
            {
              ChunkLink *chunk = magazine_chain_pop_head (&current);
              slab_allocator_free_chunk (chunk_size, chunk);
            }
        }
      inner_mutex_unlock (&allocator->slab_mutex);
    }
}

static void magazine_cache_push_magazine (auint ix,ChunkLink *magazine_chunks,asize count) /* must be >= MIN_MAGAZINE_SIZE */
{
  ChunkLink *current = magazine_chain_prepare_fields (magazine_chunks);
  ChunkLink *next, *prev;
  inner_mutex_lock (&allocator->magazine_mutex);
  /* add magazine at head */
  next = allocator->magazines[ix];
  if (next)
    prev = magazine_chain_prev (next);
  else
    next = prev = current;
  magazine_chain_next (prev) = current;
  magazine_chain_prev (next) = current;
  magazine_chain_prev (current) = prev;
  magazine_chain_next (current) = next;
  magazine_chain_count (current) = (apointer) count;
  /* stamp magazine */
  magazine_cache_update_stamp();
  magazine_chain_stamp (current) = AUINT_TO_POINTER (allocator->last_stamp);
  allocator->magazines[ix] = current;
  /* free old magazines beyond a certain threshold */
  magazine_cache_trim (allocator, ix, allocator->last_stamp);
  /* inner_mutex_unlock (allocator->mutex); was done by magazine_cache_trim() */
}

static ChunkLink *magazine_cache_pop_magazine (auint ix,asize *countp)
{
	inner_mutex_lock_a (&allocator->magazine_mutex, &allocator->contention_counters[ix]);
  if (!allocator->magazines[ix])
    {
      auint magazine_threshold = allocator_get_magazine_threshold (allocator, ix);
      asize i, chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
      ChunkLink *chunk, *head;
      inner_mutex_unlock (&allocator->magazine_mutex);
      inner_mutex_lock (&allocator->slab_mutex);
      head = slab_allocator_alloc_chunk (chunk_size);
      head->data = NULL;
      chunk = head;
      for (i = 1; i < magazine_threshold; i++)
        {
          chunk->next = slab_allocator_alloc_chunk (chunk_size);
          chunk = chunk->next;
          chunk->data = NULL;
        }
      chunk->next = NULL;
      inner_mutex_unlock (&allocator->slab_mutex);
      *countp = i;
      return head;
    }
  else
    {
      ChunkLink *current = allocator->magazines[ix];
      ChunkLink *prev = magazine_chain_prev (current);
      ChunkLink *next = magazine_chain_next (current);
      /* unlink */
      magazine_chain_next (prev) = next;
      magazine_chain_prev (next) = prev;
      allocator->magazines[ix] = next == current ? NULL : next;
      inner_mutex_unlock (&allocator->magazine_mutex);
      /* clear special fields and hand out */
      *countp = (asize) magazine_chain_count (current);
      magazine_chain_prev (current) = NULL;
      magazine_chain_next (current) = NULL;
      magazine_chain_count (current) = NULL;
      magazine_chain_stamp (current) = NULL;
      return current;
    }
}

/* --- thread magazines --- */
static void private_thread_memory_cleanup (apointer data)
{
  ThreadMemory *tmem = data;
  const auint n_magazines = MAX_SLAB_INDEX (allocator);
  auint ix;
  for (ix = 0; ix < n_magazines; ix++){
      Magazine *mags[2];
      auint j;
      mags[0] = &tmem->magazine1[ix];
      mags[1] = &tmem->magazine2[ix];
      for (j = 0; j < 2; j++){
          Magazine *mag = mags[j];
          if (mag->count >= MIN_MAGAZINE_SIZE)
            magazine_cache_push_magazine (ix, mag->chunks, mag->count);
          else{
              const asize chunk_size = SLAB_CHUNK_SIZE (allocator, ix);
              inner_mutex_lock (&allocator->slab_mutex);
              while (mag->chunks){
                  ChunkLink *chunk = magazine_chain_pop_head (&mag->chunks);
                  slab_allocator_free_chunk (chunk_size, chunk);
              }
              inner_mutex_unlock (&allocator->slab_mutex);
          }
      }
  }
  a_free (tmem);
}

static void thread_memory_magazine1_reload (ThreadMemory *tmem,auint ix)
{
  Magazine *mag = &tmem->magazine1[ix];
  mem_assert (mag->chunks == NULL); /* ensure that we may reset mag->count */
  mag->count = 0;
  mag->chunks = magazine_cache_pop_magazine (ix, &mag->count);
}

static void thread_memory_magazine2_unload (ThreadMemory *tmem,auint ix)
{
  Magazine *mag = &tmem->magazine2[ix];
  magazine_cache_push_magazine (ix, mag->chunks, mag->count);
  mag->chunks = NULL;
  mag->count = 0;
}

static inline void thread_memory_swap_magazines (ThreadMemory *tmem,auint         ix)
{
  Magazine xmag = tmem->magazine1[ix];
  tmem->magazine1[ix] = tmem->magazine2[ix];
  tmem->magazine2[ix] = xmag;
}

static inline aboolean thread_memory_magazine1_is_empty (ThreadMemory *tmem,auint         ix)
{
  return tmem->magazine1[ix].chunks == NULL;
}

static inline aboolean thread_memory_magazine2_is_full (ThreadMemory *tmem, auint ix)
{
  return tmem->magazine2[ix].count >= allocator_get_magazine_threshold (allocator, ix);
}

static inline apointer thread_memory_magazine1_alloc (ThreadMemory *tmem,auint         ix)
{
  Magazine *mag = &tmem->magazine1[ix];
  ChunkLink *chunk = magazine_chain_pop_head (&mag->chunks);
  if (A_LIKELY (mag->count > 0))
    mag->count--;
  return chunk;
}

static inline void thread_memory_magazine2_free (ThreadMemory *tmem,auint ix,apointer      mem)
{
  Magazine *mag = &tmem->magazine2[ix];
  ChunkLink *chunk = mem;
  chunk->data = NULL;
  chunk->next = mag->chunks;
  mag->chunks = chunk;
  mag->count++;
}

/* --- API functions --- */

/**
 * a_slice_new:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate a block of memory from the
 * slice allocator.
 *
 * It calls a_slice_alloc() with `sizeof (@type)` and casts the
 * returned pointer to a pointer of the given type, avoiding a type
 * cast in the source code. Note that the underlying slice allocation
 * mechanism can be changed with the [`A_SLICE=always-malloc`][A_SLICE]
 * environment variable.
 *
 * This can never return %NULL as the minimum allocation size from
 * `sizeof (@type)` is 1 byte.
 *
 * Returns: (not nullable): a pointer to the allocated block, cast to a pointer
 *    to @type
 *
 * Since: 2.10
 */

/**
 * a_slice_new0:
 * @type: the type to allocate, typically a structure name
 *
 * A convenience macro to allocate a block of memory from the
 * slice allocator and set the memory to 0.
 *
 * It calls a_slice_alloc0() with `sizeof (@type)`
 * and casts the returned pointer to a pointer of the given type,
 * avoiding a type cast in the source code.
 * Note that the underlying slice allocation mechanism can
 * be changed with the [`A_SLICE=always-malloc`][A_SLICE]
 * environment variable.
 *
 * This can never return %NULL as the minimum allocation size from
 * `sizeof (@type)` is 1 byte.
 *
 * Returns: (not nullable): a pointer to the allocated block, cast to a pointer
 *    to @type
 *
 * Since: 2.10
 */

/**
 * a_slice_dup:
 * @type: the type to duplicate, typically a structure name
 * @mem: (not nullable): the memory to copy into the allocated block
 *
 * A convenience macro to duplicate a block of memory using
 * the slice allocator.
 *
 * It calls a_slice_copy() with `sizeof (@type)`
 * and casts the returned pointer to a pointer of the given type,
 * avoiding a type cast in the source code.
 * Note that the underlying slice allocation mechanism can
 * be changed with the [`A_SLICE=always-malloc`][A_SLICE]
 * environment variable.
 *
 * This can never return %NULL.
 *
 * Returns: (not nullable): a pointer to the allocated block, cast to a pointer
 *    to @type
 *
 * Since: 2.14
 */

/**
 * a_slice_free:
 * @type: the type of the block to free, typically a structure name
 * @mem: a pointer to the block to free
 *
 * A convenience macro to free a block of memory that has
 * been allocated from the slice allocator.
 *
 * It calls a_slice_free1() using `sizeof (type)`
 * as the block size.
 * Note that the exact release behaviour can be changed with the
 * [`A_DEBUG=gc-friendly`][A_DEBUG] environment variable, also see
 * [`A_SLICE`][A_SLICE] for related debugging options.
 *
 * If @mem is %NULL, this macro does nothing.
 *
 * Since: 2.10
 */

/**
 * a_slice_free_chain:
 * @type: the type of the @mem_chain blocks
 * @mem_chain: a pointer to the first block of the chain
 * @next: the field name of the next pointer in @type
 *
 * Frees a linked list of memory blocks of structure type @type.
 * The memory blocks must be equal-sized, allocated via
 * a_slice_alloc() or a_slice_alloc0() and linked together by
 * a @next pointer (similar to #GSList). The name of the
 * @next field in @type is passed as third argument.
 * Note that the exact release behaviour can be changed with the
 * [`A_DEBUG=gc-friendly`][A_DEBUG] environment variable, also see
 * [`A_SLICE`][A_SLICE] for related debugging options.
 *
 * If @mem_chain is %NULL, this function does nothing.
 *
 * Since: 2.10
 */

/**
 * a_slice_alloc:
 * @block_size: the number of bytes to allocate
 *
 * Allocates a block of memory from the slice allocator.
 * The block address handed out can be expected to be aligned
 * to at least 1 * sizeof (void*),
 * though in general slices are 2 * sizeof (void*) bytes aligned,
 * if a malloc() fallback implementation is used instead,
 * the alignment may be reduced in a libc dependent fashion.
 * Note that the underlying slice allocation mechanism can
 * be changed with the [`A_SLICE=always-malloc`][A_SLICE]
 * environment variable.
 *
 * Returns: a pointer to the allocated memory block, which will be %NULL if and
 *    only if @mem_size is 0
 *
 * Since: 2.10
 */
apointer a_slice_alloc (asize mem_size)
{
  ThreadMemory *tmem;
  asize chunk_size;
  apointer mem;
  auint acat;

  /* This gets the private structure for this thread.  If the private
   * structure does not yet exist, it is created.
   *
   * This has a side effect of causing GSlice to be initialised, so it
   * must come first.
   */
  tmem = thread_memory_from_self ();

  chunk_size = P2ALIGN (mem_size);
  acat = allocator_categorize (chunk_size);
  if (A_LIKELY (acat == 1))     /* allocate through magazine layer */
    {
      auint ix = SLAB_INDEX (allocator, chunk_size);
      if (A_UNLIKELY (thread_memory_magazine1_is_empty (tmem, ix)))
        {
          thread_memory_swap_magazines (tmem, ix);
          if (A_UNLIKELY (thread_memory_magazine1_is_empty (tmem, ix)))
            thread_memory_magazine1_reload (tmem, ix);
        }
      mem = thread_memory_magazine1_alloc (tmem, ix);
    }
  else if (acat == 2)           /* allocate through slab allocator */
    {
	  inner_mutex_lock (&allocator->slab_mutex);
      mem = slab_allocator_alloc_chunk (chunk_size);
      inner_mutex_unlock (&allocator->slab_mutex);
    }
  else                          /* delegate to system malloc */
    mem = a_malloc (mem_size);
  if (A_UNLIKELY (allocator->config.debug_blocks))
    smc_notify_alloc (mem, mem_size);

  //TRACE (ALIB_SLICE_ALLOC((void*)mem, mem_size));

  return mem;
}

/**
 * a_slice_alloc0:
 * @block_size: the number of bytes to allocate
 *
 * Allocates a block of memory via a_slice_alloc() and initializes
 * the returned memory to 0. Note that the underlying slice allocation
 * mechanism can be changed with the [`A_SLICE=always-malloc`][A_SLICE]
 * environment variable.
 *
 * Returns: a pointer to the allocated block, which will be %NULL if and only
 *    if @mem_size is 0
 *
 * Since: 2.10
 */
apointer a_slice_alloc0 (asize mem_size)
{
  apointer mem = a_slice_alloc (mem_size);
  if (mem)
    memset (mem, 0, mem_size);
  return mem;
}

/**
 * a_slice_copy:
 * @block_size: the number of bytes to allocate
 * @mem_block: the memory to copy
 *
 * Allocates a block of memory from the slice allocator
 * and copies @block_size bytes into it from @mem_block.
 *
 * @mem_block must be non-%NULL if @block_size is non-zero.
 *
 * Returns: a pointer to the allocated memory block, which will be %NULL if and
 *    only if @mem_size is 0
 *
 * Since: 2.14
 */
apointer a_slice_copy (asize mem_size,aconstpointer mem_block)
{
  apointer mem = a_slice_alloc (mem_size);
  if (mem)
    memcpy (mem, mem_block, mem_size);
  return mem;
}

/**
 * a_slice_free1:
 * @block_size: the size of the block
 * @mem_block: a pointer to the block to free
 *
 * Frees a block of memory.
 *
 * The memory must have been allocated via a_slice_alloc() or
 * a_slice_alloc0() and the @block_size has to match the size
 * specified upon allocation. Note that the exact release behaviour
 * can be changed with the [`A_DEBUG=gc-friendly`][A_DEBUG] environment
 * variable, also see [`A_SLICE`][A_SLICE] for related debugging options.
 *
 * If @mem_block is %NULL, this function does nothing.
 *
 * Since: 2.10
 */
void a_slice_free1 (asize    mem_size,apointer mem_block)
{
  asize chunk_size = P2ALIGN (mem_size);
  auint acat = allocator_categorize (chunk_size);
  if (A_UNLIKELY (!mem_block))
    return;
  if (A_UNLIKELY (allocator->config.debug_blocks) && !smc_notify_free (mem_block, mem_size))
    abort();
  if (A_LIKELY (acat == 1))             /* allocate through magazine layer */
    {
      ThreadMemory *tmem = thread_memory_from_self();
      auint ix = SLAB_INDEX (allocator, chunk_size);
      if (A_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
        {
          thread_memory_swap_magazines (tmem, ix);
          if (A_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
            thread_memory_magazine2_unload (tmem, ix);
        }
      if (A_UNLIKELY (a_mem_gc_friendly))
        memset (mem_block, 0, chunk_size);
      thread_memory_magazine2_free (tmem, ix, mem_block);
    }
  else if (acat == 2)                   /* allocate through slab allocator */
    {
      if (A_UNLIKELY (a_mem_gc_friendly))
        memset (mem_block, 0, chunk_size);
      inner_mutex_lock (&allocator->slab_mutex);
      slab_allocator_free_chunk (chunk_size, mem_block);
      inner_mutex_unlock (&allocator->slab_mutex);
    }
  else                                  /* delegate to system malloc */
    {
      if (A_UNLIKELY (a_mem_gc_friendly))
        memset (mem_block, 0, mem_size);
      a_free (mem_block);
    }
  //TRACE (ALIB_SLICE_FREE((void*)mem_block, mem_size));
}

/**
 * a_slice_free_chain_with_offset:
 * @block_size: the size of the blocks
 * @mem_chain:  a pointer to the first block of the chain
 * @next_offset: the offset of the @next field in the blocks
 *
 * Frees a linked list of memory blocks of structure type @type.
 *
 * The memory blocks must be equal-sized, allocated via
 * a_slice_alloc() or a_slice_alloc0() and linked together by a
 * @next pointer (similar to #GSList). The offset of the @next
 * field in each block is passed as third argument.
 * Note that the exact release behaviour can be changed with the
 * [`A_DEBUG=gc-friendly`][A_DEBUG] environment variable, also see
 * [`A_SLICE`][A_SLICE] for related debugging options.
 *
 * If @mem_chain is %NULL, this function does nothing.
 *
 * Since: 2.10
 */
void a_slice_free_chain_with_offset (asize    mem_size,
                                apointer mem_chain,
                                asize    next_offset)
{
  apointer slice = mem_chain;
  /* while the thread magazines and the magazine cache are implemented so that
   * they can easily be extended to allow for free lists containing more free
   * lists for the first level nodes, which would allow O(1) freeing in this
   * function, the benefit of such an extension is questionable, because:
   * - the magazine size counts will become mere lower bounds which confuses
   *   the code adapting to lock contention;
   * - freeing a single node to the thread magazines is very fast, so this
   *   O(list_length) operation is multiplied by a fairly small factor;
   * - memory usage histograms on larger applications seem to indicate that
   *   the amount of released multi node lists is negligible in comparison
   *   to single node releases.
   * - the major performance bottle neck, namely a_private_get() or
   *   inner_mutex_lock()/inner_mutex_unlock() has already been moved out of the
   *   inner loop for freeing chained slices.
   */
  asize chunk_size = P2ALIGN (mem_size);
  auint acat = allocator_categorize (chunk_size);
  if (A_LIKELY (acat == 1))             /* allocate through magazine layer */
    {
      ThreadMemory *tmem = thread_memory_from_self();
      auint ix = SLAB_INDEX (allocator, chunk_size);
      while (slice)
        {
          auint8 *current = slice;
          slice = *(apointer*) (current + next_offset);
          if (A_UNLIKELY (allocator->config.debug_blocks) &&
              !smc_notify_free (current, mem_size))
            abort();
          if (A_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
            {
              thread_memory_swap_magazines (tmem, ix);
              if (A_UNLIKELY (thread_memory_magazine2_is_full (tmem, ix)))
                thread_memory_magazine2_unload (tmem, ix);
            }
          if (A_UNLIKELY (a_mem_gc_friendly))
            memset (current, 0, chunk_size);
          thread_memory_magazine2_free (tmem, ix, current);
        }
    }
  else if (acat == 2)                   /* allocate through slab allocator */
    {
	  inner_mutex_lock (&allocator->slab_mutex);
      while (slice)
        {
          auint8 *current = slice;
          slice = *(apointer*) (current + next_offset);
          if (A_UNLIKELY (allocator->config.debug_blocks) &&
              !smc_notify_free (current, mem_size))
            abort();
          if (A_UNLIKELY (a_mem_gc_friendly))
            memset (current, 0, chunk_size);
          slab_allocator_free_chunk (chunk_size, current);
        }
      inner_mutex_unlock (&allocator->slab_mutex);
    }
  else                                  /* delegate to system malloc */
    while (slice)
      {
        auint8 *current = slice;
        slice = *(apointer*) (current + next_offset);
        if (A_UNLIKELY (allocator->config.debug_blocks) &&
            !smc_notify_free (current, mem_size))
          abort();
        if (A_UNLIKELY (a_mem_gc_friendly))
          memset (current, 0, mem_size);
        a_free (current);
      }
}

/* --- single page allocator --- */
static void allocator_slab_stack_push (Allocator *allocator,auint ix,SlabInfo  *sinfo)
{
  /* insert slab at slab ring head */
  if (!allocator->slab_stack[ix])
    {
      sinfo->next = sinfo;
      sinfo->prev = sinfo;
    }
  else
    {
      SlabInfo *next = allocator->slab_stack[ix], *prev = next->prev;
      next->prev = sinfo;
      prev->next = sinfo;
      sinfo->next = next;
      sinfo->prev = prev;
    }
  allocator->slab_stack[ix] = sinfo;
}

static inline auint a_bit_storage (aulong number)
{
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(__OPTIMIZE__)
  return A_LIKELY (number) ?
           ((ALIB_SIZEOF_LONG * 8U - 1) ^ (auint) __builtin_clzl(number)) + 1 : 1;
#else
  auint n_bits = 0;

  do
    {
      n_bits++;
      number >>= 1;
    }
  while (number);
  return n_bits;
#endif
}

static asize allocator_aligned_page_size (Allocator *allocator,asize n_bytes)
{
  asize val = 1 << a_bit_storage (n_bytes - 1);
  val = MAX (val, allocator->min_page_size);
  return val;
}

static void allocator_add_slab (Allocator *allocator,auint ix,asize chunk_size)
{
  ChunkLink *chunk;
  SlabInfo *sinfo;
  asize addr, padding, n_chunks, color = 0;
  asize page_size;
  int errsv;
  apointer aligned_memory;
  auint8 *mem;
  auint i;

  page_size = allocator_aligned_page_size (allocator, SLAB_BPAGE_SIZE (allocator, chunk_size));
  /* allocate 1 page for the chunks and the slab */
  aligned_memory = allocator_memalign (page_size, page_size - NATIVE_MALLOC_PADDING);
  errsv = errno;
  mem = aligned_memory;

  if (!mem)
    {
      const achar *syserr = strerror (errsv);
      mem_error ("failed to allocate %u bytes (alignment: %u): %s\n",
                 (auint) (page_size - NATIVE_MALLOC_PADDING), (auint) page_size, syserr);
    }
  /* mask page address */
  addr = ((asize) mem / page_size) * page_size;
  /* assert alignment */
  mem_assert (aligned_memory == (apointer) addr);
  /* basic slab info setup */
  sinfo = (SlabInfo*) (mem + page_size - SLAB_INFO_SIZE);
  sinfo->n_allocated = 0;
  sinfo->chunks = NULL;
  /* figure cache colorization */
  n_chunks = ((auint8*) sinfo - mem) / chunk_size;
  padding = ((auint8*) sinfo - mem) - n_chunks * chunk_size;
  if (padding)
    {
      color = (allocator->color_accu * P2ALIGNMENT) % padding;
      allocator->color_accu += allocator->config.color_increment;
    }
  /* add chunks to free list */
  chunk = (ChunkLink*) (mem + color);
  sinfo->chunks = chunk;
  for (i = 0; i < n_chunks - 1; i++)
    {
      chunk->next = (ChunkLink*) ((auint8*) chunk + chunk_size);
      chunk = chunk->next;
    }
  chunk->next = NULL;   /* last chunk */
  /* add slab to slab ring */
  allocator_slab_stack_push (allocator, ix, sinfo);
}

static apointer slab_allocator_alloc_chunk (asize chunk_size)
{
  ChunkLink *chunk;
  auint ix = SLAB_INDEX (allocator, chunk_size);
  /* ensure non-empty slab */
  if (!allocator->slab_stack[ix] || !allocator->slab_stack[ix]->chunks)
    allocator_add_slab (allocator, ix, chunk_size);
  /* allocate chunk */
  chunk = allocator->slab_stack[ix]->chunks;
  allocator->slab_stack[ix]->chunks = chunk->next;
  allocator->slab_stack[ix]->n_allocated++;
  /* rotate empty slabs */
  if (!allocator->slab_stack[ix]->chunks)
    allocator->slab_stack[ix] = allocator->slab_stack[ix]->next;
  return chunk;
}

static void slab_allocator_free_chunk (asize chunk_size,apointer mem)
{
  ChunkLink *chunk;
  aboolean was_empty;
  auint ix = SLAB_INDEX (allocator, chunk_size);
  asize page_size = allocator_aligned_page_size (allocator, SLAB_BPAGE_SIZE (allocator, chunk_size));
  asize addr = ((asize) mem / page_size) * page_size;
  /* mask page address */
  auint8 *page = (auint8*) addr;
  SlabInfo *sinfo = (SlabInfo*) (page + page_size - SLAB_INFO_SIZE);
  /* assert valid chunk count */
  mem_assert (sinfo->n_allocated > 0);
  /* add chunk to free list */
  was_empty = sinfo->chunks == NULL;
  chunk = (ChunkLink*) mem;
  chunk->next = sinfo->chunks;
  sinfo->chunks = chunk;
  sinfo->n_allocated--;
  /* keep slab ring partially sorted, empty slabs at end */
  if (was_empty)
    {
      /* unlink slab */
      SlabInfo *next = sinfo->next, *prev = sinfo->prev;
      next->prev = prev;
      prev->next = next;
      if (allocator->slab_stack[ix] == sinfo)
        allocator->slab_stack[ix] = next == sinfo ? NULL : next;
      /* insert slab at head */
      allocator_slab_stack_push (allocator, ix, sinfo);
    }
  /* eagerly free complete unused slabs */
  if (!sinfo->n_allocated)
    {
      /* unlink slab */
      SlabInfo *next = sinfo->next, *prev = sinfo->prev;
      next->prev = prev;
      prev->next = next;
      if (allocator->slab_stack[ix] == sinfo)
        allocator->slab_stack[ix] = next == sinfo ? NULL : next;
      /* free slab */
      allocator_memfree (page_size, page);
    }
}

/* --- memalign implementation --- */
#ifdef HAVE_MALLOC_H
#include <malloc.h>             /* memalign() */
#endif

/* from config.h:
 * define HAVE_POSIX_MEMALIGN           1 // if free(posix_memalign(3)) works, <stdlib.h>
 * define HAVE_MEMALIGN                 1 // if free(memalign(3)) works, <malloc.h>
 * define HAVE_VALLOC                   1 // if free(valloc(3)) works, <stdlib.h> or <malloc.h>
 * if none is provided, we implement malloc(3)-based alloc-only page alignment
 */

#if !(HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN || HAVE_VALLOC)
static ATrashStack *compat_valloc_trash = NULL;
#endif

static apointer allocator_memalign (asize alignment,asize memsize)
{
  apointer aligned_memory = NULL;
  aint err = ENOMEM;
#if     HAVE_POSIX_MEMALIGN
  err = posix_memalign (&aligned_memory, alignment, memsize);
#elif   HAVE_MEMALIGN
  errno = 0;
  aligned_memory = memalign (alignment, memsize);
  err = errno;
#elif   HAVE_VALLOC
  errno = 0;
  aligned_memory = valloc (memsize);
  err = errno;
#else
  /* simplistic non-freeing page allocator */
  mem_assert (alignment == sys_page_size);
  mem_assert (memsize <= sys_page_size);
  if (!compat_valloc_trash)
    {
      const auint n_pages = 16;
      auint8 *mem = malloc (n_pages * sys_page_size);
      err = errno;
      if (mem)
        {
          aint i = n_pages;
          auint8 *amem = (auint8*) ALIGN ((asize) mem, sys_page_size);
          if (amem != mem)
            i--;        /* mem wasn't page aligned */
          while (--i >= 0)
            trash_stack_push (&compat_valloc_trash, amem + i * sys_page_size);
        }
    }
  aligned_memory = trash_stack_pop (&compat_valloc_trash);
#endif
  if (!aligned_memory)
    errno = err;
  return aligned_memory;
}

static void allocator_memfree (asize    memsize,apointer mem)
{
#if     HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN || HAVE_VALLOC
  free (mem);
#else
  mem_assert (memsize <= sys_page_size);
  trash_stack_push (&compat_valloc_trash, mem);
#endif
}

static aint a_fprintf (FILE *file,achar const *format,...)
{
  va_list args;
  aint retval;
  va_start (args, format);
  retval = vfprintf (file, format, args);
  va_end (args);

  return retval;
}

static void mem_error (const char *format,...)
{
  const char *pname;
  va_list args;
  /* at least, put out "MEMORY-ERROR", in case we segfault during the rest of the function */
  fputs ("\n***MEMORY-ERROR***: ", stderr);
  pname = NULL;//a_get_prgname();
  a_fprintf (stderr, "%s[%ld]: ASlice: ", pname ? pname : "", (long)getpid());
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  fputs ("\n", stderr);
  abort();
  _exit (1);
}

/* --- g-slice memory checker tree --- */
typedef size_t SmcKType;                /* key type */
typedef size_t SmcVType;                /* value type */
typedef struct {
  SmcKType key;
  SmcVType value;
} SmcEntry;
static void             smc_tree_insert      (SmcKType  key,
                                              SmcVType  value);
static aboolean         smc_tree_lookup      (SmcKType  key,
                                              SmcVType *value_p);
static aboolean         smc_tree_remove      (SmcKType  key);


/* --- g-slice memory checker implementation --- */
static void smc_notify_alloc (void   *pointer,size_t  size)
{
  size_t address = (size_t) pointer;
  if (pointer)
    smc_tree_insert (address, size);
}

#if 0
static void
smc_notify_ignore (void *pointer)
{
  size_t address = (size_t) pointer;
  if (pointer)
    smc_tree_remove (address);
}
#endif

static int smc_notify_free (void   *pointer,size_t  size)
{
  size_t address = (size_t) pointer;
  SmcVType real_size;
  aboolean found_one;

  if (!pointer)
    return 1; /* ignore */
  found_one = smc_tree_lookup (address, &real_size);
  if (!found_one)
    {
      fprintf (stderr, "ASlice: MemChecker: attempt to release non-allocated block: %p size=%lu\n", pointer, size);
      return 0;
    }
  if (real_size != size && (real_size || size))
    {
      fprintf (stderr, "ASlice: MemChecker: attempt to release block with invalid size: %p size=%lu invalid-size=%lu\n", pointer, real_size, size);
      return 0;
    }
  if (!smc_tree_remove (address))
    {
      fprintf (stderr, "ASlice: MemChecker: attempt to release non-allocated block: %p size=%lu \n", pointer, size);
      return 0;
    }
  return 1; /* all fine */
}

/* --- g-slice memory checker tree implementation --- */
#define SMC_TRUNK_COUNT     (4093 /* 16381 */)          /* prime, to distribute trunk collisions (big, allocated just once) */
#define SMC_BRANCH_COUNT    (511)                       /* prime, to distribute branch collisions */
#define SMC_TRUNK_EXTENT    (SMC_BRANCH_COUNT * 2039)   /* key address space per trunk, should distribute uniformly across BRANCH_COUNT */
#define SMC_TRUNK_HASH(k)   ((k / SMC_TRUNK_EXTENT) % SMC_TRUNK_COUNT)  /* generate new trunk hash per megabyte (roughly) */
#define SMC_BRANCH_HASH(k)  (k % SMC_BRANCH_COUNT)

typedef struct {
  SmcEntry    *entries;
  unsigned int n_entries;
} SmcBranch;

static SmcBranch     **smc_tree_root = NULL;

static void smc_tree_abort (int errval)
{
  const char *syserr = strerror (errval);
  mem_error ("MemChecker: failure in debugging tree: %s", syserr);
}

static inline SmcEntry* smc_tree_branch_grow_L (SmcBranch   *branch,unsigned int index)
{
  unsigned int old_size = branch->n_entries * sizeof (branch->entries[0]);
  unsigned int new_size = old_size + sizeof (branch->entries[0]);
  SmcEntry *entry;
  mem_assert (index <= branch->n_entries);
  branch->entries = (SmcEntry*) realloc (branch->entries, new_size);
  if (!branch->entries)
    smc_tree_abort (errno);
  entry = branch->entries + index;
  memmove (entry + 1, entry, (branch->n_entries - index) * sizeof (entry[0]));
  branch->n_entries += 1;
  return entry;
}

static inline SmcEntry* smc_tree_branch_lookup_nearest_L (SmcBranch *branch,SmcKType   key)
{
  unsigned int n_nodes = branch->n_entries, offs = 0;
  SmcEntry *check = branch->entries;
  int cmp = 0;
  while (offs < n_nodes)
    {
      unsigned int i = (offs + n_nodes) >> 1;
      check = branch->entries + i;
      cmp = key < check->key ? -1 : key != check->key;
      if (cmp == 0)
        return check;                   /* return exact match */
      else if (cmp < 0)
        n_nodes = i;
      else /* (cmp > 0) */
        offs = i + 1;
    }
  /* check points at last mismatch, cmp > 0 indicates greater key */
  return cmp > 0 ? check + 1 : check;   /* return insertion position for inexact match */
}

static void smc_tree_insert (SmcKType key,SmcVType value)
{
  unsigned int ix0, ix1;
  SmcEntry *entry;
  inner_mutex_lock (&smc_tree_mutex);
  ix0 = SMC_TRUNK_HASH (key);
  ix1 = SMC_BRANCH_HASH (key);
  if (!smc_tree_root)
    {
      smc_tree_root = calloc (SMC_TRUNK_COUNT, sizeof (smc_tree_root[0]));
      if (!smc_tree_root)
        smc_tree_abort (errno);
    }
  if (!smc_tree_root[ix0])
    {
      smc_tree_root[ix0] = calloc (SMC_BRANCH_COUNT, sizeof (smc_tree_root[0][0]));
      if (!smc_tree_root[ix0])
        smc_tree_abort (errno);
    }
  entry = smc_tree_branch_lookup_nearest_L (&smc_tree_root[ix0][ix1], key);
  if (!entry ||                                                                         /* need create */
      entry >= smc_tree_root[ix0][ix1].entries + smc_tree_root[ix0][ix1].n_entries ||   /* need append */
      entry->key != key)                                                                /* need insert */
    entry = smc_tree_branch_grow_L (&smc_tree_root[ix0][ix1], entry - smc_tree_root[ix0][ix1].entries);
  entry->key = key;
  entry->value = value;
  inner_mutex_unlock (&smc_tree_mutex);
}

static aboolean smc_tree_lookup (SmcKType  key,SmcVType *value_p)
{
  SmcEntry *entry = NULL;
  unsigned int ix0 = SMC_TRUNK_HASH (key), ix1 = SMC_BRANCH_HASH (key);
  aboolean found_one = FALSE;
  *value_p = 0;
  inner_mutex_lock (&smc_tree_mutex);
  if (smc_tree_root && smc_tree_root[ix0])
    {
      entry = smc_tree_branch_lookup_nearest_L (&smc_tree_root[ix0][ix1], key);
      if (entry &&
          entry < smc_tree_root[ix0][ix1].entries + smc_tree_root[ix0][ix1].n_entries &&
          entry->key == key)
        {
          found_one = TRUE;
          *value_p = entry->value;
        }
    }
  inner_mutex_unlock (&smc_tree_mutex);
  return found_one;
}

static aboolean smc_tree_remove (SmcKType key)
{
  unsigned int ix0 = SMC_TRUNK_HASH (key), ix1 = SMC_BRANCH_HASH (key);
  aboolean found_one = FALSE;
  inner_mutex_lock (&smc_tree_mutex);
  if (smc_tree_root && smc_tree_root[ix0])
    {
      SmcEntry *entry = smc_tree_branch_lookup_nearest_L (&smc_tree_root[ix0][ix1], key);
      if (entry &&
          entry < smc_tree_root[ix0][ix1].entries + smc_tree_root[ix0][ix1].n_entries &&
          entry->key == key)
        {
          unsigned int i = entry - smc_tree_root[ix0][ix1].entries;
          smc_tree_root[ix0][ix1].n_entries -= 1;
          memmove (entry, entry + 1, (smc_tree_root[ix0][ix1].n_entries - i) * sizeof (entry[0]));
          if (!smc_tree_root[ix0][ix1].n_entries)
            {
              /* avoid useless pressure on the memory system */
              free (smc_tree_root[ix0][ix1].entries);
              smc_tree_root[ix0][ix1].entries = NULL;
            }
          found_one = TRUE;
        }
    }
  inner_mutex_unlock (&smc_tree_mutex);
  return found_one;
}


static void trash_stack_push (ATrashStack **stack_p,apointer      data_p)
{
	ATrashStack *data = (ATrashStack *) data_p;
    data->next = *stack_p;
   *stack_p = data;
}


static apointer trash_stack_pop (ATrashStack **stack_p)
{
	ATrashStack *data;

  data = *stack_p;
  if (data)
    {
      *stack_p = data->next;
      /* NULLify private pointer here, most platforms store NULL as
       * subsequent 0 bytes
       */
      data->next = NULL;
    }

  return data;
}


static aint64 a_get_real_time (void)
{
  struct timeval r;
  gettimeofday (&r, NULL);
  return (((aint64) r.tv_sec) * 1000000) + r.tv_usec;

}

