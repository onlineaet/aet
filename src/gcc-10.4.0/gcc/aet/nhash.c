/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

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


#include <string.h>  /* memset */

#include "nhash.h"
#include "nstrfuncs.h"
#include "natomic.h"
#include "nslice.h"
#include "nmem.h"
#include "nlog.h"



#define HASH_TABLE_MIN_SHIFT 3  /* 1 << 3 == 8 buckets */

#define UNUSED_HASH_VALUE 0
#define TOMBSTONE_HASH_VALUE 1
#define HASH_IS_UNUSED(h_) ((h_) == UNUSED_HASH_VALUE)
#define HASH_IS_TOMBSTONE(h_) ((h_) == TOMBSTONE_HASH_VALUE)
#define HASH_IS_REAL(h_) ((h_) >= 2)

/* If int is smaller than void * on our arch, we start out with
 * int-sized keys and values and resize to pointer-sized entries as
 * needed. This saves a good amount of memory when the HT is being
 * used with e.g. NUINT_TO_POINTER(). */

#define BIG_ENTRY_SIZE (SIZEOF_VOID_P)
#define SMALL_ENTRY_SIZE (SIZEOF_INT)

#if SMALL_ENTRY_SIZE < BIG_ENTRY_SIZE
# define USE_SMALL_ARRAYS
#endif

struct _NHashTable
{
  nsize            size;
  nint             mod;
  nuint            mask;
  nint             nnodes;
  nint             noccupied;  /* nnodes + tombstones */

  nuint            have_big_keys : 1;
  nuint            have_big_values : 1;

  npointer         keys;
  nuint           *hashes;
  npointer         values;

  NHashFunc        hash_func;
  NEqualFunc       key_equal_func;
  natomicrefcount  ref_count;
  NDestroyNotify   key_destroy_func;
  NDestroyNotify   value_destroy_func;
};

typedef struct
{
  NHashTable  *hash_table;
  npointer     dummy1;
  npointer     dummy2;
  nint         position;
  nboolean     dummy3;
  nint         version;
} RealIter;



/* Each table size has an associated prime modulo (the first prime
 * lower than the table size) used to find the initial bucket. Probing
 * then works modulo 2^n. The prime modulo is necessary to get a
 * good distribution with poor hash functions.
 */
static const nint prime_mod [] =
{
  1,          /* For 1 << 0 */
  2,
  3,
  7,
  13,
  31,
  61,
  127,
  251,
  509,
  1021,
  2039,
  4093,
  8191,
  16381,
  32749,
  65521,      /* For 1 << 16 */
  131071,
  262139,
  524287,
  1048573,
  2097143,
  4194301,
  8388593,
  16777213,
  33554393,
  67108859,
  134217689,
  268435399,
  536870909,
  1073741789,
  2147483647  /* For 1 << 31 */
};

static void n_hash_table_set_shift (NHashTable *hash_table, nint shift)
{
  hash_table->size = 1 << shift;
  hash_table->mod  = prime_mod [shift];

  /* hash_table->size is always a power of two, so we can calculate the mask
   * by simply subtracting 1 from it. The leading assertion ensures that
   * we're really dealing with a power of two. */

 // n_assert ((hash_table->size & (hash_table->size - 1)) == 0);
  hash_table->mask = hash_table->size - 1;
}

static nint n_hash_table_find_closest_shift (nint n)
{
  nint i;

  for (i = 0; n; i++)
    n >>= 1;

  return i;
}

static void n_hash_table_set_shift_from_size (NHashTable *hash_table, nint size)
{
  nint shift;

  shift = n_hash_table_find_closest_shift (size);
  shift = MAX (shift, HASH_TABLE_MIN_SHIFT);

  n_hash_table_set_shift (hash_table, shift);
}

static inline npointer n_hash_table_realloc_key_or_value_array (npointer a, nuint size, N_GNUC_UNUSED nboolean is_big)
{

  return n_renew (npointer, a, size);
}

static inline npointer n_hash_table_fetch_key_or_value (npointer a, nuint index, nboolean is_big)
{
  is_big = TRUE;
  return is_big ? *(((npointer *) a) + index) : NUINT_TO_POINTER (*(((nuint *) a) + index));
}

static inline void n_hash_table_assign_key_or_value (npointer a, nuint index, nboolean is_big, npointer v)
{
  is_big = TRUE;
  if (is_big)
    *(((npointer *) a) + index) = v;
  else
    *(((nuint *) a) + index) = NPOINTER_TO_UINT (v);
}

static inline npointer n_hash_table_evict_key_or_value (npointer a, nuint index, nboolean is_big, npointer v)
{
  is_big = TRUE;
  if (is_big)
    {
      npointer r = *(((npointer *) a) + index);
      *(((npointer *) a) + index) = v;
      return r;
    }
  else
    {
      npointer r = NUINT_TO_POINTER (*(((nuint *) a) + index));
      *(((nuint *) a) + index) = NPOINTER_TO_UINT (v);
      return r;
    }
}

static inline nuint n_hash_table_hash_to_index (NHashTable *hash_table, nuint hash)
{
  /* Multiply the hash by a small prime before applying the modulo. This
   * prevents the table from becoming densely packed, even with a poor hash
   * function. A densely packed table would have poor performance on
   * workloads with many failed lookups or a high degree of churn. */
  return (hash * 11) % hash_table->mod;
}

/*
 * n_hash_table_lookup_node:
 * @hash_table: our #NHashTable
 * @key: the key to look up against
 * @hash_return: key hash return location
 *
 * Performs a lookup in the hash table, preserving extra information
 * usually needed for insertion.
 *
 * This function first computes the hash value of the key using the
 * user's hash function.
 *
 * If an entry in the table matching @key is found then this function
 * returns the index of that entry in the table, and if not, the
 * index of an unused node (empty or tombstone) where the key can be
 * inserted.
 *
 * The computed hash value is returned in the variable pointed to
 * by @hash_return. This is to save insertions from having to compute
 * the hash record again for the new record.
 *
 * Returns: index of the described node
 */
static inline nuint n_hash_table_lookup_node (NHashTable    *hash_table,nconstpointer  key, nuint *hash_return)
{
  nuint node_index;
  nuint node_hash;
  nuint hash_value;
  nuint first_tombstone = 0;
  nboolean have_tombstone = FALSE;
  nuint step = 0;

  hash_value = hash_table->hash_func (key);
  if (N_UNLIKELY (!HASH_IS_REAL (hash_value)))
    hash_value = 2;


  *hash_return = hash_value;

  node_index = n_hash_table_hash_to_index (hash_table, hash_value);
  node_hash = hash_table->hashes[node_index];
  while (!HASH_IS_UNUSED (node_hash))
    {
      /* We first check if our full hash values
       * are equal so we can avoid calling the full-blown
       * key equality function in most cases.
       */
      if (node_hash == hash_value)
        {
          npointer node_key = n_hash_table_fetch_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys);

          if (hash_table->key_equal_func)
            {
              if (hash_table->key_equal_func (node_key, key))
                return node_index;

            }
          else if (node_key == key)
            {
              return node_index;
            }
        }
      else if (HASH_IS_TOMBSTONE (node_hash) && !have_tombstone)
        {
          first_tombstone = node_index;
          have_tombstone = TRUE;
        }

      step++;
      node_index += step;
      node_index &= hash_table->mask;
      node_hash = hash_table->hashes[node_index];
    }

  if (have_tombstone)
    return first_tombstone;

  return node_index;
}


static void n_hash_table_remove_node (NHashTable   *hash_table,   nint i, nboolean      notify)
{
  npointer key;
  npointer value;

  key = n_hash_table_fetch_key_or_value (hash_table->keys, i, hash_table->have_big_keys);
  value = n_hash_table_fetch_key_or_value (hash_table->values, i, hash_table->have_big_values);

  /* Erect tombstone */
  hash_table->hashes[i] = TOMBSTONE_HASH_VALUE;

  /* Be GC friendly */
  n_hash_table_assign_key_or_value (hash_table->keys, i, hash_table->have_big_keys, NULL);
  n_hash_table_assign_key_or_value (hash_table->values, i, hash_table->have_big_values, NULL);

  hash_table->nnodes--;

  if (notify && hash_table->key_destroy_func)
    hash_table->key_destroy_func (key);

  if (notify && hash_table->value_destroy_func)
    hash_table->value_destroy_func (value);

}

/*
 * n_hash_table_setup_storage:
 * @hash_table: our #NHashTable
 *
 * Initialise the hash table size, mask, mod, and arrays.
 */
static void n_hash_table_setup_storage (NHashTable *hash_table)
{
  nboolean small = FALSE;

  /* We want to use small arrays only if:
   *   - we are running on a system where that makes sense (64 bit); and
   *   - we are not running under valgrind.
   */

#ifdef USE_SMALL_ARRAYS
  small = TRUE;
#endif

  n_hash_table_set_shift (hash_table, HASH_TABLE_MIN_SHIFT);

  hash_table->have_big_keys = !small;
  hash_table->have_big_values = !small;

  hash_table->keys   = n_hash_table_realloc_key_or_value_array (NULL, hash_table->size, hash_table->have_big_keys);
  hash_table->values = hash_table->keys;
  hash_table->hashes = n_new0 (nuint, hash_table->size);
}

/*
 * n_hash_table_remove_all_nodes:
 * @hash_table: our #NHashTable
 * @notify: %TRUE if the destroy notify handlers are to be called
 *
 * Removes all nodes from the table.
 *
 * If @notify is %TRUE then the destroy notify functions are called
 * for the key and value of the hash node.
 *
 * Since this may be a precursor to freeing the table entirely, we'd
 * ideally perform no resize, and we can indeed avoid that in some
 * cases.  However: in the case that we'll be making callbacks to user
 * code (via destroy notifies) we need to consider that the user code
 * might call back into the table again.  In this case, we setup a new
 * set of arrays so that any callers will see an empty (but valid)
 * table.
 */
static void n_hash_table_remove_all_nodes (NHashTable *hash_table,
                               nboolean    notify,
                               nboolean    destruction)
{
  int i;
  npointer key;
  npointer value;
  nint old_size;
  npointer *old_keys;
  npointer *old_values;
  nuint    *old_hashes;
  nboolean  old_have_big_keys;
  nboolean  old_have_big_values;

  /* If the hash table is already empty, there is nothing to be done. */
  if (hash_table->nnodes == 0)
    return;

  hash_table->nnodes = 0;
  hash_table->noccupied = 0;

  /* Easy case: no callbacks, so we just zero out the arrays */
  if (!notify ||
      (hash_table->key_destroy_func == NULL &&
       hash_table->value_destroy_func == NULL))
    {
      if (!destruction)
        {
          memset (hash_table->hashes, 0, hash_table->size * sizeof (nuint));

#ifdef USE_SMALL_ARRAYS
          memset (hash_table->keys, 0, hash_table->size * (hash_table->have_big_keys ? BIG_ENTRY_SIZE : SMALL_ENTRY_SIZE));
          memset (hash_table->values, 0, hash_table->size * (hash_table->have_big_values ? BIG_ENTRY_SIZE : SMALL_ENTRY_SIZE));
#else
          memset (hash_table->keys, 0, hash_table->size * sizeof (npointer));
          memset (hash_table->values, 0, hash_table->size * sizeof (npointer));
#endif
        }

      return;
    }

  /* Hard case: we need to do user callbacks.  There are two
   * possibilities here:
   *
   *   1) there are no outstanding references on the table and therefore
   *   nobody should be calling into it again (destroying == true)
   *
   *   2) there are outstanding references, and there may be future
   *   calls into the table, either after we return, or from the destroy
   *   notifies that we're about to do (destroying == false)
   *
   * We handle both cases by taking the current state of the table into
   * local variables and replacing it with something else: in the "no
   * outstanding references" cases we replace it with a bunch of
   * null/zero values so that any access to the table will fail.  In the
   * "may receive future calls" case, we reinitialise the struct to
   * appear like a newly-created empty table.
   *
   * In both cases, we take over the references for the current state,
   * freeing them below.
   */
  old_size = hash_table->size;
  old_have_big_keys = hash_table->have_big_keys;
  old_have_big_values = hash_table->have_big_values;
  old_keys   = n_steal_pointer (&hash_table->keys);
  old_values = n_steal_pointer (&hash_table->values);
  old_hashes = n_steal_pointer (&hash_table->hashes);

  if (!destruction)
    /* Any accesses will see an empty table */
    n_hash_table_setup_storage (hash_table);
  else
    /* Will cause a quick crash on any attempted access */
    hash_table->size = hash_table->mod = hash_table->mask = 0;

  /* Now do the actual destroy notifies */
  for (i = 0; i < old_size; i++)
    {
      if (HASH_IS_REAL (old_hashes[i]))
        {
          key = n_hash_table_fetch_key_or_value (old_keys, i, old_have_big_keys);
          value = n_hash_table_fetch_key_or_value (old_values, i, old_have_big_values);

          old_hashes[i] = UNUSED_HASH_VALUE;

          n_hash_table_assign_key_or_value (old_keys, i, old_have_big_keys, NULL);
          n_hash_table_assign_key_or_value (old_values, i, old_have_big_values, NULL);

          if (hash_table->key_destroy_func != NULL)
            hash_table->key_destroy_func (key);

          if (hash_table->value_destroy_func != NULL)
            hash_table->value_destroy_func (value);
        }
    }

  /* Destroy old storage space. */
  if (old_keys != old_values)
    n_free (old_values);

  n_free (old_keys);
  n_free (old_hashes);
}

static void realloc_arrays (NHashTable *hash_table, nboolean is_a_set)
{
  hash_table->hashes = n_renew (nuint, hash_table->hashes, hash_table->size);
  hash_table->keys = n_hash_table_realloc_key_or_value_array (hash_table->keys, hash_table->size, hash_table->have_big_keys);

  if (is_a_set)
    hash_table->values = hash_table->keys;
  else
    hash_table->values = n_hash_table_realloc_key_or_value_array (hash_table->values, hash_table->size, hash_table->have_big_values);
}

/* When resizing the table in place, we use a temporary bit array to keep
 * track of which entries have been assigned a proper location in the new
 * table layout.
 *
 * Each bit corresponds to a bucket. A bit is set if an entry was assigned
 * its corresponding location during the resize and thus should not be
 * evicted. The array starts out cleared to zero. */

static inline nboolean get_status_bit (const nuint32 *bitmap, nuint index)
{
  return (bitmap[index / 32] >> (index % 32)) & 1;
}

static inline void
set_status_bit (nuint32 *bitmap, nuint index)
{
  bitmap[index / 32] |= 1U << (index % 32);
}

/* By calling dedicated resize functions for sets and maps, we avoid 2x
 * test-and-branch per key in the inner loop. This yields a small
 * performance improvement at the cost of a bit of macro gunk. */

#define DEFINE_RESIZE_FUNC(fname) \
static void fname (NHashTable *hash_table, nuint old_size, nuint32 *reallocated_buckets_bitmap) \
{                                                                       \
  nuint i;                                                              \
                                                                        \
  for (i = 0; i < old_size; i++)                                        \
    {                                                                   \
      nuint node_hash = hash_table->hashes[i];                          \
      npointer key, value N_GNUC_UNUSED;                                \
                                                                        \
      if (!HASH_IS_REAL (node_hash))                                    \
        {                                                               \
          /* Clear tombstones */                                        \
          hash_table->hashes[i] = UNUSED_HASH_VALUE;                    \
          continue;                                                     \
        }                                                               \
                                                                        \
      /* Skip entries relocated through eviction */                     \
      if (get_status_bit (reallocated_buckets_bitmap, i))               \
        continue;                                                       \
                                                                        \
      hash_table->hashes[i] = UNUSED_HASH_VALUE;                        \
      EVICT_KEYVAL (hash_table, i, NULL, NULL, key, value);             \
                                                                        \
      for (;;)                                                          \
        {                                                               \
          nuint hash_val;                                               \
          nuint replaced_hash;                                          \
          nuint step = 0;                                               \
                                                                        \
          hash_val = n_hash_table_hash_to_index (hash_table, node_hash); \
                                                                        \
          while (get_status_bit (reallocated_buckets_bitmap, hash_val)) \
            {                                                           \
              step++;                                                   \
              hash_val += step;                                         \
              hash_val &= hash_table->mask;                             \
            }                                                           \
                                                                        \
          set_status_bit (reallocated_buckets_bitmap, hash_val);        \
                                                                        \
          replaced_hash = hash_table->hashes[hash_val];                 \
          hash_table->hashes[hash_val] = node_hash;                     \
          if (!HASH_IS_REAL (replaced_hash))                            \
            {                                                           \
              ASSIGN_KEYVAL (hash_table, hash_val, key, value);         \
              break;                                                    \
            }                                                           \
                                                                        \
          node_hash = replaced_hash;                                    \
          EVICT_KEYVAL (hash_table, hash_val, key, value, key, value);  \
        }                                                               \
    }                                                                   \
}

#define ASSIGN_KEYVAL(ht, index, key, value) N_STMT_START{ \
    n_hash_table_assign_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
    n_hash_table_assign_key_or_value ((ht)->values, (index), (ht)->have_big_values, (value)); \
  }N_STMT_END

#define EVICT_KEYVAL(ht, index, key, value, outkey, outvalue) N_STMT_START{ \
    (outkey) = n_hash_table_evict_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
    (outvalue) = n_hash_table_evict_key_or_value ((ht)->values, (index), (ht)->have_big_values, (value)); \
  }N_STMT_END

DEFINE_RESIZE_FUNC (resize_map)

#undef ASSIGN_KEYVAL
#undef EVICT_KEYVAL

#define ASSIGN_KEYVAL(ht, index, key, value) N_STMT_START{ \
    n_hash_table_assign_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
  }N_STMT_END

#define EVICT_KEYVAL(ht, index, key, value, outkey, outvalue) N_STMT_START{ \
    (outkey) = n_hash_table_evict_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
  }N_STMT_END

DEFINE_RESIZE_FUNC (resize_set)

#undef ASSIGN_KEYVAL
#undef EVICT_KEYVAL

/*
 * n_hash_table_resize:
 * @hash_table: our #NHashTable
 *
 * Resizes the hash table to the optimal size based on the number of
 * nodes currently held. If you call this function then a resize will
 * occur, even if one does not need to occur.
 * Use n_hash_table_maybe_resize() instead.
 *
 * This function may "resize" the hash table to its current size, with
 * the side effect of cleaning up tombstones and otherwise optimizing
 * the probe sequences.
 */
static void n_hash_table_resize (NHashTable *hash_table)
{
  nuint32 *reallocated_buckets_bitmap;
  nsize old_size;
  nboolean is_a_set;

  old_size = hash_table->size;
  is_a_set = hash_table->keys == hash_table->values;

  /* The outer checks in n_hash_table_maybe_resize() will only consider
   * cleanup/resize when the load factor goes below .25 (1/4, ignoring
   * tombstones) or above .9375 (15/16, including tombstones).
   *
   * Once this happens, tombstones will always be cleaned out. If our
   * load sans tombstones is greater than .75 (1/1.333, see below), we'll
   * take this opportunity to grow the table too.
   *
   * Immediately after growing, the load factor will be in the range
   * .375 .. .469. After shrinking, it will be exactly .5. */

  n_hash_table_set_shift_from_size (hash_table, hash_table->nnodes * 1.333);

  if (hash_table->size > old_size)
    {
      realloc_arrays (hash_table, is_a_set);
      memset (&hash_table->hashes[old_size], 0, (hash_table->size - old_size) * sizeof (nuint));

      reallocated_buckets_bitmap = n_new0 (nuint32, (hash_table->size + 31) / 32);
    }
  else
    {
      reallocated_buckets_bitmap = n_new0 (nuint32, (old_size + 31) / 32);
    }

  if (is_a_set)
    resize_set (hash_table, old_size, reallocated_buckets_bitmap);
  else
    resize_map (hash_table, old_size, reallocated_buckets_bitmap);

  n_free (reallocated_buckets_bitmap);

  if (hash_table->size < old_size)
    realloc_arrays (hash_table, is_a_set);

  hash_table->noccupied = hash_table->nnodes;
}

/*
 * n_hash_table_maybe_resize:
 * @hash_table: our #NHashTable
 *
 * Resizes the hash table, if needed.
 *
 * Essentially, calls n_hash_table_resize() if the table has strayed
 * too far from its ideal size for its number of nodes.
 */
static inline void n_hash_table_maybe_resize (NHashTable *hash_table)
{
  nint noccupied = hash_table->noccupied;
  nint size = hash_table->size;

  if ((size > hash_table->nnodes * 4 && size > 1 << HASH_TABLE_MIN_SHIFT) ||
      (size <= noccupied + (noccupied / 16)))
    n_hash_table_resize (hash_table);
}

#ifdef USE_SMALL_ARRAYS

static inline nboolean
entry_is_big (npointer v)
{
  return (((nuintptr) v) >> ((BIG_ENTRY_SIZE - SMALL_ENTRY_SIZE) * 8)) != 0;
}

static inline nboolean
n_hash_table_maybe_make_bin_keys_or_values (npointer *a_p, npointer v, nint ht_size)
{
  if (entry_is_big (v))
    {
      nuint *a = (nuint *) *a_p;
      npointer *a_new;
      nint i;

      a_new = n_new (npointer, ht_size);

      for (i = 0; i < ht_size; i++)
        {
          a_new[i] = NUINT_TO_POINTER (a[i]);
        }

      n_free (a);
      *a_p = a_new;
      return TRUE;
    }

  return FALSE;
}

#endif

static inline void
n_hash_table_ensure_keyval_fits (NHashTable *hash_table, npointer key, npointer value)
{
  nboolean is_a_set = (hash_table->keys == hash_table->values);


  /* Just split if necessary */
  if (is_a_set && key != value){
    hash_table->values = n_memdup (hash_table->keys, sizeof (npointer) * hash_table->size);
  }

}

/**
 * n_hash_table_new:
 * @hash_func: a function to create a hash value from a key
 * @key_equal_func: a function to check two keys for equality
 *
 * Creates a new #NHashTable with a reference count of 1.
 *
 * Hash values returned by @hash_func are used to determine where keys
 * are stored within the #NHashTable data structure. The n_direct_hash(),
 * n_int_hash(), n_int64_hash(), n_double_hash() and n_str_hash()
 * functions are provided for some common types of keys.
 * If @hash_func is %NULL, n_direct_hash() is used.
 *
 * @key_equal_func is used when looking up keys in the #NHashTable.
 * The n_direct_equal(), n_int_equal(), n_int64_equal(), n_double_equal()
 * and n_str_equal() functions are provided for the most common types
 * of keys. If @key_equal_func is %NULL, keys are compared directly in
 * a similar fashion to n_direct_equal(), but without the overhead of
 * a function call. @key_equal_func is called with the key from the hash table
 * as its first parameter, and the user-provided key to check against as
 * its second.
 *
 * Returns: a new #NHashTable
 */
NHashTable *n_hash_table_new (NHashFunc  hash_func,NEqualFunc key_equal_func)
{
  return n_hash_table_new_full (hash_func, key_equal_func, NULL, NULL);
}



NHashTable *n_hash_table_new_full (NHashFunc      hash_func,
                       NEqualFunc     key_equal_func,
                       NDestroyNotify key_destroy_func,
                       NDestroyNotify value_destroy_func)
{
  NHashTable *hash_table;

  hash_table = (NHashTable *)n_slice_new (NHashTable);
  hash_table->ref_count=1;
  hash_table->nnodes             = 0;
  hash_table->noccupied          = 0;
  hash_table->hash_func          = hash_func ? hash_func : n_direct_hash;
  hash_table->key_equal_func     = key_equal_func;
  hash_table->key_destroy_func   = key_destroy_func;
  hash_table->value_destroy_func = value_destroy_func;

  n_hash_table_setup_storage (hash_table);

  return hash_table;
}

/**
 * n_hash_table_iter_init:
 * @iter: an uninitialized #NHashTableIter
 * @hash_table: a #NHashTable
 *
 * Initializes a key/value pair iterator and associates it with
 * @hash_table. Modifying the hash table after calling this function
 * invalidates the returned iterator.
 *
 * The iteration order of a #NHashTableIter over the keys/values in a hash
 * table is not defined.
 *
 * |[<!-- language="C" -->
 * NHashTableIter iter;
 * npointer key, value;
 *
 * n_hash_table_iter_init (&iter, hash_table);
 * while (n_hash_table_iter_next (&iter, &key, &value))
 *   {
 *     // do something with key and value
 *   }
 * ]|
 *
 * Since: 2.16
 */
void n_hash_table_iter_init (NHashTableIter *iter,NHashTable     *hash_table)
{
  RealIter *ri = (RealIter *) iter;

  n_return_if_fail (iter != NULL);
  n_return_if_fail (hash_table != NULL);

  ri->hash_table = hash_table;
  ri->position = -1;

}

/**
 * n_hash_table_iter_next:
 * @iter: an initialized #NHashTableIter
 * @key: (out) (optional): a location to store the key
 * @value: (out) (optional) (nullable): a location to store the value
 *
 * Advances @iter and retrieves the key and/or value that are now
 * pointed to as a result of this advancement. If %FALSE is returned,
 * @key and @value are not set, and the iterator becomes invalid.
 *
 * Returns: %FALSE if the end of the #NHashTable has been reached.
 *
 * Since: 2.16
 */
nboolean n_hash_table_iter_next (NHashTableIter *iter,
                        npointer       *key,
                        npointer       *value)
{
  RealIter *ri = (RealIter *) iter;
  nint position;

  n_return_val_if_fail (iter != NULL, FALSE);
  n_return_val_if_fail (ri->position < (nssize) ri->hash_table->size, FALSE);

  position = ri->position;

  do
    {
      position++;
      if (position >= (nssize) ri->hash_table->size)
        {
          ri->position = position;
          return FALSE;
        }
    }
  while (!HASH_IS_REAL (ri->hash_table->hashes[position]));

  if (key != NULL)
    *key = n_hash_table_fetch_key_or_value (ri->hash_table->keys, position, ri->hash_table->have_big_keys);
  if (value != NULL)
    *value = n_hash_table_fetch_key_or_value (ri->hash_table->values, position, ri->hash_table->have_big_values);

  ri->position = position;
  return TRUE;
}


NHashTable *n_hash_table_iter_get_hash_table (NHashTableIter *iter)
{
  n_return_val_if_fail (iter != NULL, NULL);
  return ((RealIter *) iter)->hash_table;
}

static void iter_remove_or_steal (RealIter *ri, nboolean notify)
{
  n_return_if_fail (ri != NULL);
  n_return_if_fail (ri->position >= 0);
  n_return_if_fail ((nsize) ri->position < ri->hash_table->size);

  n_hash_table_remove_node (ri->hash_table, ri->position, notify);

}

void n_hash_table_iter_remove (NHashTableIter *iter)
{
  iter_remove_or_steal ((RealIter *) iter, TRUE);
}

/*
 * n_hash_table_insert_node:
 * @hash_table: our #NHashTable
 * @node_index: pointer to node to insert/replace
 * @key_hash: key hash
 * @key: (nullable): key to replace with, or %NULL
 * @value: value to replace with
 * @keep_new_key: whether to replace the key in the node with @key
 * @reusinn_key: whether @key was taken out of the existing node
 *
 * Inserts a value at @node_index in the hash table and updates it.
 *
 * If @key has been taken out of the existing node (ie it is not
 * passed in via a n_hash_table_insert/replace) call, then @reusinn_key
 * should be %TRUE.
 *
 * Returns: %TRUE if the key did not exist yet
 */
static nboolean n_hash_table_insert_node (NHashTable *hash_table,
                          nuint       node_index,
                          nuint       key_hash,
                          npointer    new_key,
                          npointer    new_value,
                          nboolean    keep_new_key,
                          nboolean    reusinn_key)
{
  nboolean already_exists;
  nuint old_hash;
  npointer key_to_free = NULL;
  npointer key_to_keep = NULL;
  npointer value_to_free = NULL;

  old_hash = hash_table->hashes[node_index];
  already_exists = HASH_IS_REAL (old_hash);

  /* Proceed in three steps.  First, deal with the key because it is the
   * most complicated.  Then consider if we need to split the table in
   * two (because writing the value will result in the set invariant
   * becoming broken).  Then deal with the value.
   *
   * There are three cases for the key:
   *
   *  - entry already exists in table, reusing key:
   *    free the just-passed-in new_key and use the existing value
   *
   *  - entry already exists in table, not reusing key:
   *    free the entry in the table, use the new key
   *
   *  - entry not already in table:
   *    use the new key, free nothing
   *
   * We update the hash at the same time...
   */

  if (already_exists)
    {
      /* Note: we must record the old value before writing the new key
       * because we might change the value in the event that the two
       * arrays are shared.
       */
      value_to_free = n_hash_table_fetch_key_or_value (hash_table->values, node_index, hash_table->have_big_values);

      if (keep_new_key)
        {
          key_to_free = n_hash_table_fetch_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys);
          key_to_keep = new_key;
        }
      else
        {
          key_to_free = new_key;
          key_to_keep = n_hash_table_fetch_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys);
        }
    }
  else
    {
      hash_table->hashes[node_index] = key_hash;
      key_to_keep = new_key;
    }

  /* Resize key/value arrays and split table as necessary */
  n_hash_table_ensure_keyval_fits (hash_table, key_to_keep, new_value);
  n_hash_table_assign_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys, key_to_keep);

  /* Step 3: Actually do the write */
  n_hash_table_assign_key_or_value (hash_table->values, node_index, hash_table->have_big_values, new_value);

  /* Now, the bookkeeping... */
  if (!already_exists)
    {
      hash_table->nnodes++;

      if (HASH_IS_UNUSED (old_hash))
        {
          /* We replaced an empty node, and not a tombstone */
          hash_table->noccupied++;
          n_hash_table_maybe_resize (hash_table);
        }

    }

  if (already_exists)
    {
      if (hash_table->key_destroy_func && !reusinn_key)
        (* hash_table->key_destroy_func) (key_to_free);
      if (hash_table->value_destroy_func)
        (* hash_table->value_destroy_func) (value_to_free);
    }

  return !already_exists;
}


void n_hash_table_iter_replace (NHashTableIter *iter,npointer        value)
{
  RealIter *ri;
  nuint node_hash;
  npointer key;

  ri = (RealIter *) iter;

  n_return_if_fail (ri != NULL);

  n_return_if_fail (ri->position >= 0);
  n_return_if_fail ((nsize) ri->position < ri->hash_table->size);

  node_hash = ri->hash_table->hashes[ri->position];

  key = n_hash_table_fetch_key_or_value (ri->hash_table->keys, ri->position, ri->hash_table->have_big_keys);

  n_hash_table_insert_node (ri->hash_table, ri->position, node_hash, key, value, TRUE, TRUE);


}

void n_hash_table_iter_steal (NHashTableIter *iter)
{
  iter_remove_or_steal ((RealIter *) iter, FALSE);
}



NHashTable *n_hash_table_ref (NHashTable *hash_table)
{
  n_return_val_if_fail (hash_table != NULL, NULL);

  n_atomic_int_inc (&hash_table->ref_count);

  return hash_table;
}


void n_hash_table_unref (NHashTable *hash_table)
{
  n_return_if_fail (hash_table != NULL);

  if (n_atomic_int_dec_and_test (&hash_table->ref_count))
    {
      n_hash_table_remove_all_nodes (hash_table, TRUE, TRUE);
      if (hash_table->keys != hash_table->values)
        n_free (hash_table->values);
      n_free (hash_table->keys);
      n_free (hash_table->hashes);
      n_slice_free (NHashTable, hash_table);
    }
}


void n_hash_table_destroy (NHashTable *hash_table)
{
  n_return_if_fail (hash_table != NULL);

  n_hash_table_remove_all (hash_table);
  n_hash_table_unref (hash_table);
}


npointer n_hash_table_lookup (NHashTable    *hash_table,
                     nconstpointer  key)
{
  nuint node_index;
  nuint node_hash;

  n_return_val_if_fail (hash_table != NULL, NULL);
  node_index = n_hash_table_lookup_node (hash_table, key, &node_hash);
  return HASH_IS_REAL (hash_table->hashes[node_index])
    ? n_hash_table_fetch_key_or_value (hash_table->values, node_index, hash_table->have_big_values)
    : NULL;
}


nboolean n_hash_table_lookup_extended (NHashTable    *hash_table,
                              nconstpointer  lookup_key,
                              npointer      *orin_key,
                              npointer      *value)
{
  nuint node_index;
  nuint node_hash;

  n_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = n_hash_table_lookup_node (hash_table, lookup_key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    {
      if (orin_key != NULL)
        *orin_key = NULL;
      if (value != NULL)
        *value = NULL;

      return FALSE;
    }

  if (orin_key)
    *orin_key = n_hash_table_fetch_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys);

  if (value)
    *value = n_hash_table_fetch_key_or_value (hash_table->values, node_index, hash_table->have_big_values);

  return TRUE;
}


static nboolean n_hash_table_insert_internal (NHashTable *hash_table,
                              npointer    key,
                              npointer    value,
                              nboolean    keep_new_key)
{
  nuint key_hash;
  nuint node_index;

  n_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = n_hash_table_lookup_node (hash_table, key, &key_hash);
  return n_hash_table_insert_node (hash_table, node_index, key_hash, key, value, keep_new_key, FALSE);
}


nboolean n_hash_table_insert (NHashTable *hash_table,npointer    key,npointer    value)
{
  return n_hash_table_insert_internal (hash_table, key, value, FALSE);
}


nboolean n_hash_table_replace (NHashTable *hash_table,
                      npointer    key,
                      npointer    value)
{
  return n_hash_table_insert_internal (hash_table, key, value, TRUE);
}


nboolean n_hash_table_add (NHashTable *hash_table,
                  npointer    key)
{
  return n_hash_table_insert_internal (hash_table, key, key, TRUE);
}

/**
 * n_hash_table_contains:
 * @hash_table: a #NHashTable
 * @key: a key to check
 *
 * Checks if @key is in @hash_table.
 *
 * Returns: %TRUE if @key is in @hash_table, %FALSE otherwise.
 *
 * Since: 2.32
 **/
nboolean n_hash_table_contains (NHashTable    *hash_table,
                       nconstpointer  key)
{
  nuint node_index;
  nuint node_hash;

  n_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = n_hash_table_lookup_node (hash_table, key, &node_hash);

  return HASH_IS_REAL (hash_table->hashes[node_index]);
}


static nboolean n_hash_table_remove_internal (NHashTable    *hash_table,
                              nconstpointer  key,
                              nboolean       notify)
{
  nuint node_index;
  nuint node_hash;

  n_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = n_hash_table_lookup_node (hash_table, key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    return FALSE;

  n_hash_table_remove_node (hash_table, node_index, notify);
  n_hash_table_maybe_resize (hash_table);


  return TRUE;
}


nboolean n_hash_table_remove (NHashTable    *hash_table,       nconstpointer  key)
{
  return n_hash_table_remove_internal (hash_table, key, TRUE);
}

/**
 * n_hash_table_steal:
 * @hash_table: a #NHashTable
 * @key: the key to remove
 *
 * Removes a key and its associated value from a #NHashTable without
 * calling the key and value destroy functions.
 *
 * Returns: %TRUE if the key was found and removed from the #NHashTable
 */
nboolean n_hash_table_steal (NHashTable    *hash_table,       nconstpointer  key)
{
  return n_hash_table_remove_internal (hash_table, key, FALSE);
}


nboolean n_hash_table_steal_extended (NHashTable    *hash_table,
                             nconstpointer  lookup_key,
                             npointer      *stolen_key,
                             npointer      *stolen_value)
{
  nuint node_index;
  nuint node_hash;

  n_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = n_hash_table_lookup_node (hash_table, lookup_key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    {
      if (stolen_key != NULL)
        *stolen_key = NULL;
      if (stolen_value != NULL)
        *stolen_value = NULL;
      return FALSE;
    }

  if (stolen_key != NULL)
  {
    *stolen_key = n_hash_table_fetch_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys);
    n_hash_table_assign_key_or_value (hash_table->keys, node_index, hash_table->have_big_keys, NULL);
  }

  if (stolen_value != NULL)
  {
    *stolen_value = n_hash_table_fetch_key_or_value (hash_table->values, node_index, hash_table->have_big_values);
    n_hash_table_assign_key_or_value (hash_table->values, node_index, hash_table->have_big_values, NULL);
  }

  n_hash_table_remove_node (hash_table, node_index, FALSE);
  n_hash_table_maybe_resize (hash_table);

  return TRUE;
}

/**
 * n_hash_table_remove_all:
 * @hash_table: a #NHashTable
 *
 * Removes all keys and their associated values from a #NHashTable.
 *
 * If the #NHashTable was created using n_hash_table_new_full(),
 * the keys and values are freed using the supplied destroy functions,
 * otherwise you have to make sure that any dynamically allocated
 * values are freed yourself.
 *
 * Since: 2.12
 */
void n_hash_table_remove_all (NHashTable *hash_table)
{
  n_return_if_fail (hash_table != NULL);


  n_hash_table_remove_all_nodes (hash_table, TRUE, FALSE);
  n_hash_table_maybe_resize (hash_table);
}

/**
 * n_hash_table_steal_all:
 * @hash_table: a #NHashTable
 *
 * Removes all keys and their associated values from a #NHashTable
 * without calling the key and value destroy functions.
 *
 * Since: 2.12
 */
void n_hash_table_steal_all (NHashTable *hash_table)
{
  n_return_if_fail (hash_table != NULL);

  n_hash_table_remove_all_nodes (hash_table, FALSE, FALSE);
  n_hash_table_maybe_resize (hash_table);
}


static nuint n_hash_table_foreach_remove_or_steal (NHashTable *hash_table,
                                      NHRFunc     func,
                                      npointer    user_data,
                                      nboolean    notify)
{
  nuint deleted = 0;
  nsize i;

  for (i = 0; i < hash_table->size; i++)
    {
      nuint node_hash = hash_table->hashes[i];
      npointer node_key = n_hash_table_fetch_key_or_value (hash_table->keys, i, hash_table->have_big_keys);
      npointer node_value = n_hash_table_fetch_key_or_value (hash_table->values, i, hash_table->have_big_values);

      if (HASH_IS_REAL (node_hash) &&
          (* func) (node_key, node_value, user_data))
        {
          n_hash_table_remove_node (hash_table, i, notify);
          deleted++;
        }

    }

  n_hash_table_maybe_resize (hash_table);

  return deleted;
}


nuint n_hash_table_foreach_remove (NHashTable *hash_table,
                             NHRFunc     func,
                             npointer    user_data)
{
  n_return_val_if_fail (hash_table != NULL, 0);
  n_return_val_if_fail (func != NULL, 0);

  return n_hash_table_foreach_remove_or_steal (hash_table, func, user_data, TRUE);
}


nuint n_hash_table_foreach_steal (NHashTable *hash_table,
                            NHRFunc     func,
                            npointer    user_data)
{
  n_return_val_if_fail (hash_table != NULL, 0);
  n_return_val_if_fail (func != NULL, 0);

  return n_hash_table_foreach_remove_or_steal (hash_table, func, user_data, FALSE);
}


void n_hash_table_foreach (NHashTable *hash_table,
                      NHFunc      func,
                      npointer    user_data)
{
  nsize i;

  n_return_if_fail (hash_table != NULL);
  n_return_if_fail (func != NULL);


  for (i = 0; i < hash_table->size; i++)
    {
      nuint node_hash = hash_table->hashes[i];
      npointer node_key = n_hash_table_fetch_key_or_value (hash_table->keys, i, hash_table->have_big_keys);
      npointer node_value = n_hash_table_fetch_key_or_value (hash_table->values, i, hash_table->have_big_values);

      if (HASH_IS_REAL (node_hash))
        (* func) (node_key, node_value, user_data);

    }
}


npointer n_hash_table_find (NHashTable *hash_table,NHRFunc     predicate,  npointer    user_data)
{
  nsize i;
  nboolean match;
  n_return_val_if_fail (hash_table != NULL, NULL);
  n_return_val_if_fail (predicate != NULL, NULL);
  match = FALSE;

  for (i = 0; i < hash_table->size; i++)
    {
      nuint node_hash = hash_table->hashes[i];
      npointer node_key = n_hash_table_fetch_key_or_value (hash_table->keys, i, hash_table->have_big_keys);
      npointer node_value = n_hash_table_fetch_key_or_value (hash_table->values, i, hash_table->have_big_values);

      if (HASH_IS_REAL (node_hash))
        match = predicate (node_key, node_value, user_data);


      if (match)
        return node_value;
    }

  return NULL;
}


nuint n_hash_table_size (NHashTable *hash_table)
{
  return hash_table->nnodes;
}


NList *n_hash_table_get_keys (NHashTable *hash_table)
{
  nsize i;
  NList *retval;

  n_return_val_if_fail (hash_table != NULL, NULL);

  retval = NULL;
  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        retval = n_list_prepend (retval, n_hash_table_fetch_key_or_value (hash_table->keys, i, hash_table->have_big_keys));
    }

  return retval;
}


NList *n_hash_table_get_values (NHashTable *hash_table)
{
  nsize i;
  NList *retval;

  n_return_val_if_fail (hash_table != NULL, NULL);

  retval = NULL;
  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        retval = n_list_prepend (retval, n_hash_table_fetch_key_or_value (hash_table->values, i, hash_table->have_big_values));
    }

  return retval;
}

//////////////////////////hash func------

nboolean n_str_equal (nconstpointer v1,
             nconstpointer v2)
{
  const nchar *string1 = v1;
  const nchar *string2 = v2;

  return strcmp (string1, string2) == 0;
}


nuint n_str_hash (nconstpointer v)
{
  const signed char *p;
  nuint32 h = 5381;
  for (p = v; *p != '\0'; p++)
    h = (h << 5) + h + *p;

  return h;
}


nuint n_direct_hash (nconstpointer v)
{
  return NPOINTER_TO_UINT (v);
}

nboolean n_direct_equal (nconstpointer v1,nconstpointer v2)
{
  return v1 == v2;
}


nboolean n_int_equal (nconstpointer v1,nconstpointer v2)
{
  return *((const nint*) v1) == *((const nint*) v2);
}


nuint n_int_hash (nconstpointer v)
{
  return *(const nint*) v;
}


nboolean n_int64_equal (nconstpointer v1,
               nconstpointer v2)
{
  return *((const nint64*) v1) == *((const nint64*) v2);
}


nuint n_int64_hash (nconstpointer v)
{
  return (nuint) *(const nint64*) v;
}


nboolean n_double_equal (nconstpointer v1,nconstpointer v2)
{
  return *((const ndouble*) v1) == *((const ndouble*) v2);
}


nuint n_double_hash (nconstpointer v)
{
  return (nuint) *(const ndouble*) v;
}

