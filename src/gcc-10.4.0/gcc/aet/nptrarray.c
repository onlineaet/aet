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

#include <string.h>
#include <stdlib.h>

#include "nptrarray.h"
#include "nhash.h"
#include "nqsort.h"
#include "nslice.h"
#include "nmem.h"
#include "nlog.h"
#include "natomic.h"

#define MIN_ARRAY_SIZE  16


typedef enum
{
  FREE_SEGMENT = 1 << 0,
  PRESERVE_WRAPPER = 1 << 1
} ArrayFreeFlags;

typedef struct _NRealPtrArray  NRealPtrArray;


struct _NRealPtrArray
{
  npointer       *pdata;
  nuint           len;
  nuint           alloc;
  natomicrefcount ref_count;
  NDestroyNotify  element_free_func;
};



static void n_ptr_array_maybe_expand (NRealPtrArray *array,  nuint          len);


/* Returns the smallest power of 2 greater than n, or n if
 * such power does not fit in a nuint
 */
static nuint n_nearest_pow (nuint num)
{
  nuint n = num - 1;

  n_assert (num > 0);

  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#if SIZEOF_INT == 8
  n |= n >> 32;
#endif
  return n + 1;
}

static NPtrArray *ptr_array_new (nuint reserved_size,NDestroyNotify element_free_func)
{
  NRealPtrArray *array;

  array = n_slice_new (NRealPtrArray);

  array->pdata = NULL;
  array->len = 0;
  array->alloc = 0;
  array->element_free_func = element_free_func;

  array->ref_count=1;

  if (reserved_size != 0)
    n_ptr_array_maybe_expand (array, reserved_size);

  return (NPtrArray *) array;
}


NPtrArray* n_ptr_array_new (void)
{
  return ptr_array_new (0, NULL);
}


npointer *n_ptr_array_steal (NPtrArray *array,nsize *len)
{
  NRealPtrArray *rarray;
  npointer *segment;

  n_return_val_if_fail (array != NULL, NULL);

  rarray = (NRealPtrArray *) array;
  segment = (npointer *) rarray->pdata;

  if (len != NULL)
    *len = rarray->len;

  rarray->pdata = NULL;
  rarray->len   = 0;
  rarray->alloc = 0;
  return segment;
}


NPtrArray *n_ptr_array_copy (NPtrArray *array,NCopyFunc  func,npointer   user_data)
{
  NPtrArray *new_array;

  n_return_val_if_fail (array != NULL, NULL);

  new_array = ptr_array_new (array->len,((NRealPtrArray *) array)->element_free_func);

  if (func != NULL)
    {
      nuint i;

      for (i = 0; i < array->len; i++)
        new_array->pdata[i] = func (array->pdata[i], user_data);
    }
  else if (array->len > 0)
    {
      memcpy (new_array->pdata, array->pdata,
              array->len * sizeof (*array->pdata));
    }

  new_array->len = array->len;

  return new_array;
}


NPtrArray* n_ptr_array_sized_new (nuint reserved_size)
{
  return ptr_array_new (reserved_size, NULL);
}




NPtrArray* n_ptr_array_new_with_free_func (NDestroyNotify element_free_func)
{
  return ptr_array_new (0, element_free_func);
}


NPtrArray* n_ptr_array_new_full (nuint reserved_size,NDestroyNotify element_free_func)
{
  return ptr_array_new (reserved_size, element_free_func);
}


void n_ptr_array_set_free_func (NPtrArray  *array,NDestroyNotify  element_free_func)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;

  n_return_if_fail (array);

  rarray->element_free_func = element_free_func;
}


NPtrArray* n_ptr_array_ref (NPtrArray *array)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;

  n_return_val_if_fail (array, NULL);

  n_atomic_int_inc (&rarray->ref_count);

  return array;
}

static npointer *ptr_array_free (NPtrArray *, ArrayFreeFlags);

/**
 * n_ptr_array_unref:
 * @array: A #NPtrArray
 *
 * Atomically decrements the reference count of @array by one. If the
 * reference count drops to 0, the effect is the same as calling
 * n_ptr_array_free() with @free_segment set to %TRUE. This function
 * is thread-safe and may be called from any thread.
 *
 * Since: 2.22
 */
void n_ptr_array_unref (NPtrArray *array)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;
  n_return_if_fail (array);
  if (n_atomic_int_dec_and_test (&rarray->ref_count))
    ptr_array_free (array, FREE_SEGMENT);
}

/**
 * n_ptr_array_free:
 * @array: a #NPtrArray
 * @free_seg: if %TRUE the actual pointer array is freed as well
 *
 * Frees the memory allocated for the #NPtrArray. If @free_seg is %TRUE
 * it frees the memory block holding the elements as well. Pass %FALSE
 * if you want to free the #NPtrArray wrapper but preserve the
 * underlying array for use elsewhere. If the reference count of @array
 * is greater than one, the #NPtrArray wrapper is preserved but the
 * size of @array will be set to zero.
 *
 * If array contents point to dynamically-allocated memory, they should
 * be freed separately if @free_seg is %TRUE and no #NDestroyNotify
 * function has been set for @array.
 *
 * This function is not thread-safe. If using a #NPtrArray from multiple
 * threads, use only the atomic n_ptr_array_ref() and n_ptr_array_unref()
 * functions.
 *
 * Returns: the pointer array if @free_seg is %FALSE, otherwise %NULL.
 *     The pointer array should be freed using n_free().
 */
npointer* n_ptr_array_free (NPtrArray *array,       nboolean   free_segment)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;
  ArrayFreeFlags flags;

  n_return_val_if_fail (rarray, NULL);

  flags = (free_segment ? FREE_SEGMENT : 0);

  /* if others are holding a reference, preserve the wrapper but
   * do free/return the data
   */
  if (!n_atomic_int_dec_and_test (&rarray->ref_count))
    flags |= PRESERVE_WRAPPER;

  return ptr_array_free (array, flags);
}

static npointer * ptr_array_free (NPtrArray      *array,ArrayFreeFlags  flags)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;
  npointer *segment;

  if (flags & FREE_SEGMENT)
    {
      /* Data here is stolen and freed manually. It is an
       * error to attempt to access the array data (including
       * mutating the array bounds) during destruction).
       *
       * https://bugzilla.gnome.org/show_bug.cgi?id=769064
       */
      npointer *stolen_pdata = n_steal_pointer (&rarray->pdata);
      if (rarray->element_free_func != NULL)
        {
          nuint i;

          for (i = 0; i < rarray->len; ++i)
            rarray->element_free_func (stolen_pdata[i]);
        }

      n_free (stolen_pdata);
      segment = NULL;
    }
  else
    segment = rarray->pdata;

  if (flags & PRESERVE_WRAPPER)
    {
      rarray->pdata = NULL;
      rarray->len = 0;
      rarray->alloc = 0;
    }
  else
    {
      n_slice_free1 (sizeof (NRealPtrArray), rarray);
    }

  return segment;
}

static void n_ptr_array_maybe_expand (NRealPtrArray *array,
                          nuint          len)
{
  /* Detect potential overflow */
  if N_UNLIKELY ((N_MAXUINT - array->len) < len)
    n_error ("adding %u to array would overflow", len);

  if ((array->len + len) > array->alloc)
    {
      nuint old_alloc = array->alloc;
      array->alloc = n_nearest_pow (array->len + len);
      array->alloc = MAX (array->alloc, MIN_ARRAY_SIZE);
      array->pdata = n_realloc (array->pdata, sizeof (npointer) * array->alloc);
      if (N_UNLIKELY (n_mem_gc_friendly))
        for ( ; old_alloc < array->alloc; old_alloc++)
          array->pdata [old_alloc] = NULL;
    }
}


void n_ptr_array_set_size  (NPtrArray *array,
                       nint       length)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;
  nuint length_unsigned;

  n_return_if_fail (rarray);
  n_return_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL));
  n_return_if_fail (length >= 0);

  length_unsigned = (nuint) length;

  if (length_unsigned > rarray->len)
    {
      nuint i;
      n_ptr_array_maybe_expand (rarray, (length_unsigned - rarray->len));
      /* This is not 
       *     memset (array->pdata + array->len, 0,
       *            sizeof (npointer) * (length_unsigned - array->len));
       * to make it really portable. Remember (void*)NULL needn't be
       * bitwise zero. It of course is silly not to use memset (..,0,..).
       */
      for (i = rarray->len; i < length_unsigned; i++)
        rarray->pdata[i] = NULL;
    }
  else if (length_unsigned < rarray->len)
    n_ptr_array_remove_range (array, length_unsigned, rarray->len - length_unsigned);

  rarray->len = length_unsigned;
}

static npointer ptr_array_remove_index (NPtrArray *array,
                        nuint      index_,
                        nboolean   fast,
                        nboolean   free_element)
{
  NRealPtrArray *rarray = (NRealPtrArray *) array;
  npointer result;

  n_return_val_if_fail (rarray, NULL);
  n_return_val_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL), NULL);

  n_return_val_if_fail (index_ < rarray->len, NULL);

  result = rarray->pdata[index_];

  if (rarray->element_free_func != NULL && free_element)
    rarray->element_free_func (rarray->pdata[index_]);

  if (index_ != rarray->len - 1 && !fast)
    memmove (rarray->pdata + index_, rarray->pdata + index_ + 1,
             sizeof (npointer) * (rarray->len - index_ - 1));
  else if (index_ != rarray->len - 1)
    rarray->pdata[index_] = rarray->pdata[rarray->len - 1];

  rarray->len -= 1;

  if (N_UNLIKELY (n_mem_gc_friendly))
    rarray->pdata[rarray->len] = NULL;

  return result;
}


npointer n_ptr_array_remove_index (NPtrArray *array,
                          nuint      index_)
{
  return ptr_array_remove_index (array, index_, FALSE, TRUE);
}


npointer n_ptr_array_remove_index_fast (NPtrArray *array,nuint      index_)
{
  return ptr_array_remove_index (array, index_, TRUE, TRUE);
}


npointer n_ptr_array_steal_index (NPtrArray *array,
                         nuint      index_)
{
  return ptr_array_remove_index (array, index_, FALSE, FALSE);
}


npointer n_ptr_array_steal_index_fast (NPtrArray *array,nuint      index_)
{
  return ptr_array_remove_index (array, index_, TRUE, FALSE);
}


NPtrArray* n_ptr_array_remove_range (NPtrArray *array,nuint      index_, nuint      length)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;
  nuint i;

  n_return_val_if_fail (rarray != NULL, NULL);
  n_return_val_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL), NULL);
  n_return_val_if_fail (index_ <= rarray->len, NULL);
  n_return_val_if_fail (index_ + length <= rarray->len, NULL);

  if (rarray->element_free_func != NULL)
    {
      for (i = index_; i < index_ + length; i++)
        rarray->element_free_func (rarray->pdata[i]);
    }

  if (index_ + length != rarray->len)
    {
      memmove (&rarray->pdata[index_],
               &rarray->pdata[index_ + length],
               (rarray->len - (index_ + length)) * sizeof (npointer));
    }

  rarray->len -= length;
  if (N_UNLIKELY (n_mem_gc_friendly))
    {
      for (i = 0; i < length; i++)
        rarray->pdata[rarray->len + i] = NULL;
    }

  return array;
}


nboolean n_ptr_array_remove (NPtrArray *array,npointer   data)
{
  nuint i;

  n_return_val_if_fail (array, FALSE);
  n_return_val_if_fail (array->len == 0 || (array->len != 0 && array->pdata != NULL), FALSE);

  for (i = 0; i < array->len; i += 1)
    {
      if (array->pdata[i] == data)
        {
          n_ptr_array_remove_index (array, i);
          return TRUE;
        }
    }

  return FALSE;
}


nboolean n_ptr_array_remove_fast (NPtrArray *array,
                         npointer   data)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;
  nuint i;

  n_return_val_if_fail (rarray, FALSE);
  n_return_val_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL), FALSE);

  for (i = 0; i < rarray->len; i += 1)
    {
      if (rarray->pdata[i] == data)
        {
          n_ptr_array_remove_index_fast (array, i);
          return TRUE;
        }
    }

  return FALSE;
}


void n_ptr_array_add (NPtrArray *array,
                 npointer   data)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;

  n_return_if_fail (rarray);
  n_return_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL));

  n_ptr_array_maybe_expand (rarray, 1);

  rarray->pdata[rarray->len++] = data;
}


void n_ptr_array_extend (NPtrArray  *array_to_extend,
                    NPtrArray  *array,
                    NCopyFunc   func,
                    npointer    user_data)
{
  NRealPtrArray *rarray_to_extend = (NRealPtrArray *) array_to_extend;

  n_return_if_fail (array_to_extend != NULL);
  n_return_if_fail (array != NULL);

  n_ptr_array_maybe_expand (rarray_to_extend, array->len);

  if (func != NULL)
    {
      nuint i;

      for (i = 0; i < array->len; i++)
        rarray_to_extend->pdata[i + rarray_to_extend->len] =
          func (array->pdata[i], user_data);
    }
  else if (array->len > 0)
    {
      memcpy (rarray_to_extend->pdata + rarray_to_extend->len, array->pdata,
              array->len * sizeof (*array->pdata));
    }

  rarray_to_extend->len += array->len;
}


void n_ptr_array_extend_and_steal (NPtrArray  *array_to_extend,
                              NPtrArray  *array)
{
  npointer *pdata;

  n_ptr_array_extend (array_to_extend, array, NULL, NULL);

  /* Get rid of @array without triggering the NDestroyNotify attached
   * to the elements moved from @array to @array_to_extend. */
  pdata = n_steal_pointer (&array->pdata);
  array->len = 0;
  ((NRealPtrArray *) array)->alloc = 0;
  n_ptr_array_unref (array);
  n_free (pdata);
}


void n_ptr_array_insert (NPtrArray *array,
                    nint       index_,
                    npointer   data)
{
  NRealPtrArray *rarray = (NRealPtrArray *)array;

  n_return_if_fail (rarray);
  n_return_if_fail (index_ >= -1);
  n_return_if_fail (index_ <= (nint)rarray->len);

  n_ptr_array_maybe_expand (rarray, 1);

  if (index_ < 0)
    index_ = rarray->len;

  if ((nuint) index_ < rarray->len)
    memmove (&(rarray->pdata[index_ + 1]),
             &(rarray->pdata[index_]),
             (rarray->len - index_) * sizeof (npointer));

  rarray->len++;
  rarray->pdata[index_] = data;
}

/* Please keep this doc-comment in sync with pointer_array_sort_example()
 * in glib/tests/array-test.c */
/**
 * n_ptr_array_sort:
 * @array: a #NPtrArray
 * @compare_func: comparison function
 *
 * Sorts the array, using @compare_func which should be a qsort()-style
 * comparison function (returns less than zero for first arg is less
 * than second arg, zero for equal, greater than zero if irst arg is
 * greater than second arg).
 *
 * Note that the comparison function for n_ptr_array_sort() doesn't
 * take the pointers from the array as arguments, it takes pointers to
 * the pointers in the array. Here is a full example of usage:
 *
 * |[<!-- language="C" -->
 * typedef struct
 * {
 *   nchar *name;
 *   nint size;
 * } FileListEntry;
 *
 * static nint
 * sort_filelist (nconstpointer a, nconstpointer b)
 * {
 *   const FileListEntry *entry1 = *((FileListEntry **) a);
 *   const FileListEntry *entry2 = *((FileListEntry **) b);
 *
 *   return n_ascii_strcasecmp (entry1->name, entry2->name);
 * }
 *
 * …
 * n_autoptr (NPtrArray) file_list = NULL;
 *
 * // initialize file_list array and load with many FileListEntry entries
 * ...
 * // now sort it with
 * n_ptr_array_sort (file_list, sort_filelist);
 * ]|
 *
 * This is guaranteed to be a stable sort since version 2.32.
 */
void n_ptr_array_sort (NPtrArray    *array,NCompareFunc  compare_func)
{
  n_return_if_fail (array != NULL);

  /* Don't use qsort as we want a guaranteed stable sort */
  n_qsort_with_data (array->pdata,
                     array->len,
                     sizeof (npointer),
                     (NCompareDataFunc)compare_func,
                     NULL);
}

/* Please keep this doc-comment in sync with
 * pointer_array_sort_with_data_example() in glib/tests/array-test.c */
/**
 * n_ptr_array_sort_with_data:
 * @array: a #NPtrArray
 * @compare_func: comparison function
 * @user_data: data to pass to @compare_func
 *
 * Like n_ptr_array_sort(), but the comparison function has an extra
 * user data argument.
 *
 * Note that the comparison function for n_ptr_array_sort_with_data()
 * doesn't take the pointers from the array as arguments, it takes
 * pointers to the pointers in the array. Here is a full example of use:
 *
 * |[<!-- language="C" -->
 * typedef enum { SORT_NAME, SORT_SIZE } SortMode;
 *
 * typedef struct
 * {
 *   nchar *name;
 *   nint size;
 * } FileListEntry;
 *
 * static nint
 * sort_filelist (nconstpointer a, nconstpointer b, npointer user_data)
 * {
 *   nint order;
 *   const SortMode sort_mode = GPOINTER_TO_INT (user_data);
 *   const FileListEntry *entry1 = *((FileListEntry **) a);
 *   const FileListEntry *entry2 = *((FileListEntry **) b);
 *
 *   switch (sort_mode)
 *     {
 *     case SORT_NAME:
 *       order = n_ascii_strcasecmp (entry1->name, entry2->name);
 *       break;
 *     case SORT_SIZE:
 *       order = entry1->size - entry2->size;
 *       break;
 *     default:
 *       order = 0;
 *       break;
 *     }
 *   return order;
 * }
 *
 * ...
 * n_autoptr (NPtrArray) file_list = NULL;
 * SortMode sort_mode;
 *
 * // initialize file_list array and load with many FileListEntry entries
 * ...
 * // now sort it with
 * sort_mode = SORT_NAME;
 * n_ptr_array_sort_with_data (file_list,
 *                             sort_filelist,
 *                             GINT_TO_POINTER (sort_mode));
 * ]|
 *
 * This is guaranteed to be a stable sort since version 2.32.
 */
void n_ptr_array_sort_with_data (NPtrArray        *array,
                            NCompareDataFunc  compare_func,
                            npointer          user_data)
{
  n_return_if_fail (array != NULL);

  n_qsort_with_data (array->pdata,
                     array->len,
                     sizeof (npointer),
                     compare_func,
                     user_data);
}

/**
 * n_ptr_array_foreach:
 * @array: a #NPtrArray
 * @func: the function to call for each array element
 * @user_data: user data to pass to the function
 * 
 * Calls a function for each element of a #NPtrArray. @func must not
 * add elements to or remove elements from the array.
 *
 * Since: 2.4
 */
void n_ptr_array_foreach (NPtrArray *array,NFunc      func, npointer   user_data)
{
  nuint i;

  n_return_if_fail (array);

  for (i = 0; i < array->len; i++)
    (*func) (array->pdata[i], user_data);
}

/**
 * n_ptr_array_find: (skip)
 * @haystack: pointer array to be searched
 * @needle: pointer to look for
 * @index_: (optional) (out caller-allocates): return location for the index of
 *    the element, if found
 *
 * Checks whether @needle exists in @haystack. If the element is found, %TRUE is
 * returned and the element’s index is returned in @index_ (if non-%NULL).
 * Otherwise, %FALSE is returned and @index_ is undefined. If @needle exists
 * multiple times in @haystack, the index of the first instance is returned.
 *
 * This does pointer comparisons only. If you want to use more complex equality
 * checks, such as string comparisons, use n_ptr_array_find_with_equal_func().
 *
 * Returns: %TRUE if @needle is one of the elements of @haystack
 * Since: 2.54
 */
nboolean n_ptr_array_find (NPtrArray     *haystack,
                  nconstpointer  needle,
                  nuint         *index_)
{
  return n_ptr_array_find_with_equal_func (haystack, needle, NULL, index_);
}

/**
 * n_ptr_array_find_with_equal_func: (skip)
 * @haystack: pointer array to be searched
 * @needle: pointer to look for
 * @equal_func: (nullable): the function to call for each element, which should
 *    return %TRUE when the desired element is found; or %NULL to use pointer
 *    equality
 * @index_: (optional) (out caller-allocates): return location for the index of
 *    the element, if found
 *
 * Checks whether @needle exists in @haystack, using the given @equal_func.
 * If the element is found, %TRUE is returned and the element’s index is
 * returned in @index_ (if non-%NULL). Otherwise, %FALSE is returned and @index_
 * is undefined. If @needle exists multiple times in @haystack, the index of
 * the first instance is returned.
 *
 * @equal_func is called with the element from the array as its first parameter,
 * and @needle as its second parameter. If @equal_func is %NULL, pointer
 * equality is used.
 *
 * Returns: %TRUE if @needle is one of the elements of @haystack
 * Since: 2.54
 */
nboolean n_ptr_array_find_with_equal_func (NPtrArray     *haystack,nconstpointer  needle,NEqualFunc     equal_func,nuint *index_)
{
  nuint i;

  n_return_val_if_fail (haystack != NULL, FALSE);

  if (equal_func == NULL)
    equal_func = n_direct_equal;

  for (i = 0; i < haystack->len; i++)
    {
      if (equal_func (n_ptr_array_index (haystack, i), needle))
        {
          if (index_ != NULL)
            *index_ = i;
          return TRUE;
        }
    }

  return FALSE;
}

