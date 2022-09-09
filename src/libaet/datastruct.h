

#ifndef __DATA_STRUCT_H__
#define __DATA_STRUCT_H__

#include <stdarg.h>
#include "abase.h"

typedef struct _DSList DSList;


DSList* d_slist_prepend (DSList   *list, apointer  data);
DSList* d_slist_remove (DSList *list, aconstpointer  data);
DSList* d_slist_find (DSList  *list,aconstpointer  data);

typedef struct _DArray DArray;

DArray* d_array_sized_new(aboolean zero_terminated, aboolean clear, auint elt_size,auint reserved_size);
DArray* d_array_append_vals (DArray *array,aconstpointer data,auint len);
DArray* d_array_append_val (DArray *array,aconstpointer data);
achar*  d_array_free (DArray   *array, aboolean  free_segment);

typedef struct _DString DString;

struct _DString
{
  achar  *str;
  asize len;
  asize allocated_len;
};

DString *d_string_new (const achar *init);
DString *d_string_append_c (DString *string,achar c);
void     d_string_append_printf (DString *string,const achar *format,...);
DString *d_string_append (DString *string,const achar *val);
DString *d_string_insert (DString *string,assize pos,const achar *val);
DString *d_string_erase (DString *string,assize   pos,assize   len);
char    *d_string_free(DString *string,aboolean freeSegment);


#endif /* __DATA_STRUCT_H__ */
