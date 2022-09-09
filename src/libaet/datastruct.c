
#include "aslice.h"
#include "alog.h"
#include "datastruct.h"
#include "amem.h"
#include "aatomic.h"
#include "aprintf.h"

struct _DSList
{
  apointer data;
  DSList *next;
};

static DSList* d_slist_alloc ()
{
  return (DSList*)a_slice_new(DSList);
}

static void d_slist_free_1 (DSList *list)
{
	a_slice_free (DSList,list);
}

DSList* d_slist_find (DSList  *list,aconstpointer  data)
{
  while (list)
    {
      if (list->data == data)
        break;
      list = list->next;
    }

  return list;
}

DSList* d_slist_prepend (DSList   *list, apointer  data)
{
  DSList *new_list;
  new_list = d_slist_alloc();
  new_list->data = data;
  new_list->next = list;
  return new_list;
}

static DSList* _d_slist_remove_data (DSList        *list,aconstpointer  data,aboolean       all)
{
  DSList *tmp = NULL;
  DSList **previous_ptr = &list;

  while (*previous_ptr)
    {
      tmp = *previous_ptr;
      if (tmp->data == data)
        {
          *previous_ptr = tmp->next;
          d_slist_free_1 (tmp);
          if (!all)
            break;
        }
      else
        {
          previous_ptr = &tmp->next;
        }
    }

  return list;
}


DSList* d_slist_remove (DSList *list, aconstpointer  data)
{
  return _d_slist_remove_data (list, data, FALSE);
}


////////////////////////////////
struct _DArray
{
  auint8 *data;
  auint   len;
  auint   alloc;
  auint   elt_size;
  auint   zero_terminated : 1;
  auint   clear : 1;
  volatile aint   ref_count;
  ADestroyNotify clear_func;
};

typedef enum
{
  FREE_SEGMENT = 1 << 0,
  PRESERVE_WRAPPER = 1 << 1
} ArrayFreeFlags;

#define a_array_elt_len(array,i) ((array)->elt_size * (i))
#define a_array_elt_pos(array,i) ((array)->data + a_array_elt_len((array),(i)))
#define a_array_elt_zero(array, pos, len)                               \
  (memset (a_array_elt_pos ((array), pos), 0,  a_array_elt_len ((array), len)))
#define a_array_zero_terminate(array) A_STMT_START{                     \
  if ((array)->zero_terminated)                                         \
    a_array_elt_zero ((array), (array)->len, 1);                        \
}A_STMT_END

#define MIN_ARRAY_SIZE  16


static auint a_nearest_pow (auint num)
{
  auint n = num - 1;
  if(num<=0){
	  printf("a_nearest_pow num<=0 %d",num);
	  exit(-1);
  }

  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n + 1;
}

static void a_array_maybe_expand (DArray *array,auint       len)
{
  auint want_alloc;

  /* Detect potential overflow */
  if A_UNLIKELY ((A_MAXUINT - array->len) < len)
    a_error ("adding %u to array would overflow", len);

  want_alloc = a_array_elt_len (array, array->len + len +
                                array->zero_terminated);

  if (want_alloc > array->alloc)
    {
      want_alloc = a_nearest_pow (want_alloc);
      want_alloc = MAX (want_alloc, MIN_ARRAY_SIZE);
       printf("a_array_maybe_expand 00\n");
      array->data = a_realloc (array->data, want_alloc);

      if (A_UNLIKELY (a_mem_gc_friendly))
        memset (array->data + array->alloc, 0, want_alloc - array->alloc);

      array->alloc = want_alloc;
    }
}

DArray* d_array_sized_new(aboolean zero_terminated, aboolean clear, auint elt_size,auint reserved_size)
{
	  DArray *array;
	  a_return_val_if_fail (elt_size > 0, NULL);
	  array = a_slice_new (DArray);
	  array->data            = NULL;
	  array->len             = 0;
	  array->alloc           = 0;
	  array->zero_terminated = (zero_terminated ? 1 : 0);
	  array->clear           = (clear ? 1 : 0);
	  array->elt_size        = elt_size;
	  array->clear_func      = NULL;

	  array->ref_count=1;//a_atomic_ref_count_init (&array->ref_count);

	  if (array->zero_terminated || reserved_size != 0)
	    {
	      a_array_maybe_expand (array, reserved_size);
	      a_array_zero_terminate(array);
	    }

	  return array;
}

DArray* d_array_append_vals (DArray *array,aconstpointer data,auint len)
{
  a_return_val_if_fail (array, NULL);
  if (len == 0)
    return array;
  a_array_maybe_expand (array, len);
  memcpy (a_array_elt_pos (array, array->len), data,
          a_array_elt_len (array, len));
  array->len += len;
  a_array_zero_terminate (array);
  return array;
}

DArray* d_array_append_val (DArray *array,aconstpointer data)
{
  return d_array_append_vals(array,data,1);
}

static achar *array_free (DArray     *array,ArrayFreeFlags  flags)
{
  achar *segment;
  if (flags & FREE_SEGMENT)
    {
      if (array->clear_func != NULL)
        {
          auint i;
          for (i = 0; i < array->len; i++)
            array->clear_func (a_array_elt_pos (array, i));
        }

      a_free (array->data);
      segment = NULL;
    }
  else
    segment = (achar*) array->data;

  if (flags & PRESERVE_WRAPPER)
    {
      array->data            = NULL;
      array->len             = 0;
      array->alloc           = 0;
    }
  else
    {
      a_slice_free1 (sizeof (DArray), array);
    }

  return segment;
}

static aboolean atomic_ref_count_dec (aint *arc)
{
    a_return_val_if_fail (arc != NULL, FALSE);
	a_return_val_if_fail (a_atomic_int_get (arc) > 0, FALSE);
    return a_atomic_int_dec_and_test (arc);
}


achar* d_array_free (DArray   *array, aboolean  free_segment)
{
  ArrayFreeFlags flags;
  a_return_val_if_fail (array, NULL);
  flags = (free_segment ? FREE_SEGMENT : 0);
  /* if others are holding a reference, preserve the wrapper but do free/return the data */
  if (!atomic_ref_count_dec (&array->ref_count))
    flags |= PRESERVE_WRAPPER;
  return array_free (array, flags);
}

//////////////////////////////

#define MY_MAXSIZE ((asize)-1)

static inline asize nearest_power (asize base, asize num)
{
  if (num > MY_MAXSIZE / 2){
     return MY_MAXSIZE;
  }else{
     asize n = base;
     while (n < num)
        n <<= 1;
     return n;
  }
}


static void d_string_maybe_expand (DString *string,asize    len)
{
  if (string->len + len >= string->allocated_len)
    {
      string->allocated_len = nearest_power (1, string->len + len + 1);
      string->str = a_realloc (string->str, string->allocated_len);
    }
}

DString *d_string_sized_new (asize dfl_size)
{
	DString *string = a_slice_new (DString);
    string->allocated_len = 0;
    string->len   = 0;
    string->str   = NULL;
    d_string_maybe_expand (string, MAX (dfl_size, 2));
    string->str[0] = 0;
    return string;
}

DString *d_string_insert_len (DString     *string,assize  pos, const achar *val, assize  len)
{
  asize len_unsigned, pos_unsigned;

  a_return_val_if_fail (string != NULL, NULL);
  a_return_val_if_fail (len == 0 || val != NULL, string);

  if (len == 0)
    return string;

  if (len < 0)
    len = strlen (val);
  len_unsigned = len;

  if (pos < 0)
    pos_unsigned = string->len;
  else
    {
      pos_unsigned = pos;
      a_return_val_if_fail (pos_unsigned <= string->len, string);
    }

  /* Check whether val represents a substring of string.
   * This test probably violates chapter and verse of the C standards,
   * since ">=" and "<=" are only valid when val really is a substring.
   * In practice, it will work on modern archs.
   */
  if (A_UNLIKELY (val >= string->str && val <= string->str + string->len))
    {
      asize offset = val - string->str;
      asize precount = 0;

      d_string_maybe_expand (string, len_unsigned);
      val = string->str + offset;
      /* At this point, val is valid again.  */

      /* Open up space where we are going to insert.  */
      if (pos_unsigned < string->len)
        memmove (string->str + pos_unsigned + len_unsigned,
                 string->str + pos_unsigned, string->len - pos_unsigned);

      /* Move the source part before the gap, if any.  */
      if (offset < pos_unsigned)
        {
          precount = MIN (len_unsigned, pos_unsigned - offset);
          memcpy (string->str + pos_unsigned, val, precount);
        }

      /* Move the source part after the gap, if any.  */
      if (len_unsigned > precount)
        memcpy (string->str + pos_unsigned + precount,
                val + /* Already moved: */ precount +
                      /* Space opened up: */ len_unsigned,
                len_unsigned - precount);
    }
  else
    {
      d_string_maybe_expand (string, len_unsigned);

      /* If we aren't appending at the end, move a hunk
       * of the old string to the end, opening up space
       */
      if (pos_unsigned < string->len)
        memmove (string->str + pos_unsigned + len_unsigned,
                 string->str + pos_unsigned, string->len - pos_unsigned);

      /* insert the new string */
      if (len_unsigned == 1)
        string->str[pos_unsigned] = *val;
      else
        memcpy (string->str + pos_unsigned, val, len_unsigned);
    }

  string->len += len_unsigned;
  string->str[string->len] = 0;
  return string;
}

DString *d_string_append_len (DString *string,const achar *val,assize len)
{
  return d_string_insert_len (string, -1, val, len);
}

DString *d_string_insert_c (DString *string,assize   pos,   achar    c)
{
  asize pos_unsigned;

  a_return_val_if_fail (string != NULL, NULL);

  d_string_maybe_expand (string, 1);

  if (pos < 0)
    pos = string->len;
  else
    a_return_val_if_fail ((asize) pos <= string->len, string);
  pos_unsigned = pos;

  /* If not just an append, move the old stuff */
  if (pos_unsigned < string->len)
    memmove (string->str + pos_unsigned + 1,
             string->str + pos_unsigned, string->len - pos_unsigned);

  string->str[pos_unsigned] = c;

  string->len += 1;

  string->str[string->len] = 0;
  return string;
}

DString *d_string_append_c (DString *string,achar c)
{
  a_return_val_if_fail (string != NULL, NULL);
  return d_string_insert_c (string, -1, c);
}

void d_string_append_vprintf (DString  *string,const achar *format,va_list args)
{
  achar *buf;
  aint len;

  a_return_if_fail (string != NULL);
  a_return_if_fail (format != NULL);
  len = a_vasprintf (&buf, format, args);
  if (len >= 0)
    {
      d_string_maybe_expand (string, len);
      memcpy (string->str + string->len, buf, len + 1);
      string->len += len;
      a_free (buf);
    }
}

void d_string_append_printf (DString *string,const achar *format,...)
{
  va_list args;
  va_start (args, format);
  d_string_append_vprintf (string, format, args);
  va_end (args);
}

achar *d_string_free (DString  *string,aboolean  free_segment)
{
  achar *segment;
  a_return_val_if_fail (string != NULL, NULL);
  if (free_segment)
    {
      a_free (string->str);
      segment = NULL;
    }
  else
    segment = string->str;

  a_slice_free (DString, string);
  return segment;
}

DString *d_string_append (DString *string,const achar *val)
{
  return d_string_insert_len (string, -1, val, -1);
}

DString *d_string_insert (DString *string,assize pos,const achar *val)
{
  return d_string_insert_len (string, pos, val, -1);
}

DString *d_string_erase (DString *string,assize   pos,assize   len)
{
  asize len_unsigned, pos_unsigned;

  a_return_val_if_fail (string != NULL, NULL);
  a_return_val_if_fail (pos >= 0, string);
  pos_unsigned = pos;

  a_return_val_if_fail (pos_unsigned <= string->len, string);

  if (len < 0)
    len_unsigned = string->len - pos_unsigned;
  else
    {
      len_unsigned = len;
      a_return_val_if_fail (pos_unsigned + len_unsigned <= string->len, string);

      if (pos_unsigned + len_unsigned < string->len)
        memmove (string->str + pos_unsigned,
                 string->str + pos_unsigned + len_unsigned,
                 string->len - (pos_unsigned + len_unsigned));
    }

  string->len -= len_unsigned;
  string->str[string->len] = 0;
  return string;
}

DString *d_string_new (const achar *init)
{
  DString *self;
  if (init == NULL || *init == '\0')
	  self = d_string_sized_new (2);
  else{
      aint len;
      len = strlen (init);
      self = d_string_sized_new (len + 2);
      d_string_append_len (self, init, len);
  }
  return self;
}


