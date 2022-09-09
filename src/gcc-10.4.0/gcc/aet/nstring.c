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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "nstring.h"
#include "nprintf.h"
#include "nmem.h"
#include "nslice.h"
#include "nlog.h"
#include "nstrfuncs.h"




#define MY_MAXSIZE ((nsize)-1)

static inline nsize nearest_power (nsize base, nsize num)
{
  if (num > MY_MAXSIZE / 2)
    {
      return MY_MAXSIZE;
    }
  else
    {
      nsize n = base;

      while (n < num)
        n <<= 1;

      return n;
    }
}


static void n_string_maybe_expand (NString *string,nsize len)
{
  if (string->len + len >= string->allocated_len)
    {
      string->allocated_len = nearest_power (1, string->len + len + 1);
      string->str = n_realloc (string->str, string->allocated_len);
    }
}





nchar *n_string_free (NString  *string,nboolean  free_segment)
{
  nchar *segment;
  n_return_val_if_fail (string != NULL, NULL);
  if (free_segment)
    {
      n_free (string->str);
      segment = NULL;
    }
  else
    segment = string->str;

  n_slice_free (NString, string);

  return segment;
}



nboolean n_string_equal (const NString *v,const NString *v2)
{
  nchar *p, *q;
  NString *string1 = (NString *) v;
  NString *string2 = (NString *) v2;
  nsize i = string1->len;

  if (i != string2->len)
    return FALSE;

  p = string1->str;
  q = string2->str;
  while (i)
    {
      if (*p != *q)
        return FALSE;
      p++;
      q++;
      i--;
    }
  return TRUE;
}

/**
 * n_string_hash:
 * @str: a string to hash
 *
 * Creates a hash code for @str; for use with #GHashTable.
 *
 * Returns: hash code for @str
 */
nuint n_string_hash (const NString *str)
{
  const nchar *p = str->str;
  nsize n = str->len;
  nuint h = 0;

  /* 31 bit hash function */
  while (n--)
    {
      h = (h << 5) - h + *p;
      p++;
    }

  return h;
}


NString *n_string_assign (NString     *string,
                 const nchar *rval)
{
  n_return_val_if_fail (string != NULL, NULL);
  n_return_val_if_fail (rval != NULL, string);

  /* Make sure assigning to itself doesn't corrupt the string. */
  if (string->str != rval)
    {
      /* Assigning from substring should be ok, since
       * n_string_truncate() does not reallocate.
       */
      n_string_truncate (string, 0);
      n_string_append (string, rval);
    }

  return string;
}


NString *n_string_truncate (NString *self,nsize    len)
{
  n_return_val_if_fail (self != NULL, NULL);
  self->len = MIN (len, self->len);
  self->str[self->len] = 0;
  return self;
}


NString *n_string_set_size (NString *string,nsize    len)
{
  n_return_val_if_fail (string != NULL, NULL);
  if (len >= string->allocated_len)
    n_string_maybe_expand (string, len - string->len);
  string->len = len;
  string->str[len] = 0;
  return string;
}


NString *n_string_insert_len (NString     *string,nssize  pos, const nchar *val, nssize  len)
{
  nsize len_unsigned, pos_unsigned;

  n_return_val_if_fail (string != NULL, NULL);
  n_return_val_if_fail (len == 0 || val != NULL, string);

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
      n_return_val_if_fail (pos_unsigned <= string->len, string);
    }

  /* Check whether val represents a substring of string.
   * This test probably violates chapter and verse of the C standards,
   * since ">=" and "<=" are only valid when val really is a substring.
   * In practice, it will work on modern archs.
   */
  if (N_UNLIKELY (val >= string->str && val <= string->str + string->len))
    {
      nsize offset = val - string->str;
      nsize precount = 0;

      n_string_maybe_expand (string, len_unsigned);
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
      n_string_maybe_expand (string, len_unsigned);

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

nuint n_string_replace (NString *string, const nchar *find,const nchar *replace,int limit)
{
  nsize f_len, r_len, pos;
  nchar *cur, *next;
  nuint n = 0;

  n_return_val_if_fail (string != NULL, 0);
  n_return_val_if_fail (find != NULL, 0);
  n_return_val_if_fail (replace != NULL, 0);

  f_len = strlen (find);
  r_len = strlen (replace);
  cur = string->str;

  while ((next = strstr (cur, find)) != NULL)
    {
      pos = next - string->str;
      n_string_erase (string, pos, f_len);
      n_string_insert (string, pos, replace);
      cur = string->str + pos + r_len;
      n++;
      /* Only match the empty string once at any given position, to
       * avoid infinite loops */
      if (f_len == 0)
        {
          if (cur[0] == '\0')
            break;
          else
            cur++;
        }
      if (n == limit)
        break;
    }

  return n;
}



NString *n_string_append (NString *string,const nchar *val)
{
  return n_string_insert_len (string, -1, val, -1);
}


NString *n_string_append_len (NString *string,const nchar *val,nssize len)
{
  return n_string_insert_len (string, -1, val, len);
}


NString *n_string_append_c (NString *string,nchar c)
{
  n_return_val_if_fail (string != NULL, NULL);

  return n_string_insert_c (string, -1, c);
}


NString *n_string_append_unichar (NString  *string,nunichar  wc)
{
  n_return_val_if_fail (string != NULL, NULL);

  return n_string_insert_unichar (string, -1, wc);
}


NString *n_string_prepend (NString     *string,const nchar *val)
{
  return n_string_insert_len (string, 0, val, -1);
}


NString *n_string_prepend_len (NString     *string,const nchar *val,nssize       len)
{
  return n_string_insert_len (string, 0, val, len);
}


NString *n_string_prepend_c (NString *string,nchar    c)
{
  n_return_val_if_fail (string != NULL, NULL);

  return n_string_insert_c (string, 0, c);
}


NString *n_string_prepend_unichar (NString  *string,nunichar  wc)
{
  n_return_val_if_fail (string != NULL, NULL);

  return n_string_insert_unichar (string, 0, wc);
}


NString *n_string_insert (NString     *string,nssize       pos,const nchar *val)
{
  return n_string_insert_len (string, pos, val, -1);
}


NString *n_string_insert_c (NString *string,nssize   pos,   nchar    c)
{
  nsize pos_unsigned;

  n_return_val_if_fail (string != NULL, NULL);

  n_string_maybe_expand (string, 1);

  if (pos < 0)
    pos = string->len;
  else
    n_return_val_if_fail ((nsize) pos <= string->len, string);
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


NString *n_string_insert_unichar (NString  *string,nssize pos,nunichar  wc)
{
  nint charlen, first, i;
  nchar *dest;

  n_return_val_if_fail (string != NULL, NULL);

  /* Code copied from n_unichar_to_utf() */
  if (wc < 0x80)
    {
      first = 0;
      charlen = 1;
    }
  else if (wc < 0x800)
    {
      first = 0xc0;
      charlen = 2;
    }
  else if (wc < 0x10000)
    {
      first = 0xe0;
      charlen = 3;
    }
   else if (wc < 0x200000)
    {
      first = 0xf0;
      charlen = 4;
    }
  else if (wc < 0x4000000)
    {
      first = 0xf8;
      charlen = 5;
    }
  else
    {
      first = 0xfc;
      charlen = 6;
    }
  /* End of copied code */

  n_string_maybe_expand (string, charlen);

  if (pos < 0)
    pos = string->len;
  else
    n_return_val_if_fail ((nsize) pos <= string->len, string);

  /* If not just an append, move the old stuff */
  if ((nsize) pos < string->len)
    memmove (string->str + pos + charlen, string->str + pos, string->len - pos);

  dest = string->str + pos;
  /* Code copied from g_unichar_to_utf() */
  for (i = charlen - 1; i > 0; --i)
    {
      dest[i] = (wc & 0x3f) | 0x80;
      wc >>= 6;
    }
  dest[0] = wc | first;
  /* End of copied code */
  string->len += charlen;
  string->str[string->len] = 0;
  return string;
}


NString *n_string_overwrite (NString *string,nsize pos,const nchar *val)
{
  n_return_val_if_fail (val != NULL, string);
  return n_string_overwrite_len (string, pos, val, strlen (val));
}

NString *n_string_overwrite_len (NString *string,nsize pos,const nchar *val,nssize       len)
{
  nsize end;

  n_return_val_if_fail (string != NULL, NULL);

  if (!len)
    return string;

  n_return_val_if_fail (val != NULL, string);
  n_return_val_if_fail (pos <= string->len, string);

  if (len < 0)
    len = strlen (val);

  end = pos + len;

  if (end > string->len)
    n_string_maybe_expand (string, end - string->len);

  memcpy (string->str + pos, val, len);

  if (end > string->len)
    {
      string->str[end] = '\0';
      string->len = end;
    }
  return string;
}


NString *n_string_erase (NString *string,nssize   pos,nssize   len)
{
  nsize len_unsigned, pos_unsigned;

  n_return_val_if_fail (string != NULL, NULL);
  n_return_val_if_fail (pos >= 0, string);
  pos_unsigned = pos;

  n_return_val_if_fail (pos_unsigned <= string->len, string);

  if (len < 0)
    len_unsigned = string->len - pos_unsigned;
  else
    {
      len_unsigned = len;
      n_return_val_if_fail (pos_unsigned + len_unsigned <= string->len, string);

      if (pos_unsigned + len_unsigned < string->len)
        memmove (string->str + pos_unsigned,
                 string->str + pos_unsigned + len_unsigned,
                 string->len - (pos_unsigned + len_unsigned));
    }

  string->len -= len_unsigned;

  string->str[string->len] = 0;
  return string;
}


NString *n_string_ascii_down (NString *string)
{
  nchar *s;
  nint n;

  n_return_val_if_fail (string != NULL, NULL);

  n = string->len;
  s = string->str;

  while (n)
    {
      *s = n_ascii_tolower (*s);
      s++;
      n--;
    }

  return string;
}


NString *n_string_ascii_up (NString *string)
{
  nchar *s;
  nint n;

  n_return_val_if_fail (string != NULL, NULL);

  n = string->len;
  s = string->str;

  while (n)
    {
      *s = n_ascii_toupper (*s);
      s++;
      n--;
    }

  return string;
}


void n_string_append_vprintf (NString  *string,const nchar *format,va_list      args)
{
  nchar *buf;
  nint len;

  n_return_if_fail (string != NULL);
  n_return_if_fail (format != NULL);

  len = n_vasprintf (&buf, format, args);

  if (len >= 0)
    {
      n_string_maybe_expand (string, len);
      memcpy (string->str + string->len, buf, len + 1);
      string->len += len;
      n_free (buf);
    }
}


void n_string_vprintf (NString     *string,const nchar *format,va_list      args)
{
  n_string_truncate (string, 0);
  n_string_append_vprintf (string, format, args);
}


void n_string_printf (NString     *string, const nchar *format, ...)
{
  va_list args;
  n_string_truncate (string, 0);
  va_start (args, format);
  n_string_append_vprintf (string, format, args);
  va_end (args);
}


void n_string_append_printf (NString     *string,const nchar *format,...)
{
  va_list args;
  va_start (args, format);
  n_string_append_vprintf (string, format, args);
  va_end (args);
}

nboolean n_string_starts_with_from(NString *self,const char  *prefix, int toffset)
{
	  char *temp=self->str+toffset;
	  char * prefixStr = strstr (temp, prefix);
	  if(!prefixStr)
	   		return FALSE;
	  return (prefixStr-temp)==0;
}

nboolean n_string_starts_with(NString *self,const char  *prefix)
{
   	char * prefixStr = strstr (self->str, prefix);
   	if(!prefixStr)
   		return FALSE;
   	return (prefixStr-self->str)==0;
}

 static int indexofFromNew(NString *self, const char *str,int fromIndex)
 {
	     if(str==NULL)
	    	 return -1;
	     int sourceCount=strlen(self->str);
         if (fromIndex >= sourceCount)
             return (strlen(str) == 0 ? sourceCount : -1);
         if (fromIndex < 0)
             fromIndex = 0;

	    char *temp=self->str+fromIndex;
	    char *ttcc=strstr(temp,str);
	    if(!ttcc)
	    	return -1;
	    return (ttcc-temp)+fromIndex;
 }



 int n_string_indexof_from(NString *self,const char *str,int fromIndex)
 {
     return indexofFromNew(self, str,fromIndex);
 }

 int n_string_indexof(NString *self,const char *str)
 {
	 return indexofFromNew(self, str,0);
 }

 static nchar *lastIndexOf (const nchar *haystack,const nchar *needle,int from)
 {
   nsize i;
   nsize needle_len;
   nsize haystack_len;
   const nchar *p;

   needle_len = strlen (needle);
   haystack_len = strlen (haystack);

   if (needle_len == 0)
     return (nchar *)haystack;

   if (haystack_len < needle_len)
     return NULL;

   p = haystack + haystack_len - needle_len;
   p = haystack + from+1- needle_len;

   while (p >= haystack)
     {
       for (i = 0; i < needle_len; i++)
         if (p[i] != needle[i])
           goto next;

       return (nchar *)p;

     next:
       p--;
     }

   return NULL;
 }

 static int lastIndexofFrom(NString *self,const char *str, int fromIndex)
 {
	    char *res= lastIndexOf(self->str, str, fromIndex);
	    if(res==NULL)
	    	return -1;
	    return  (res-self->str);
 }

 static int lastIndexof(NString *self,const char *str)
 {
     return lastIndexofFrom(self,str, strlen(self->str));

 }

 int n_string_last_indexof(NString *self,const char *str)
 {
        return lastIndexof(self,str);
 }

 int n_string_last_indexof_from(NString *self,const char *str,int from)
 {
       return lastIndexofFrom(self,str,from);
 }

 static NString *substringNew(NString  *self,int beginIndex,int endIndex)
 {
     if (beginIndex < 0) {
         n_warning("给定的位置超出字符的的长度！%d",beginIndex);
         return NULL;
     }
     if (endIndex > strlen(self->str)) {
         n_warning("给定的位置超出字符的的长度！%d",endIndex);
         return NULL;
     }
     if ( endIndex-beginIndex<0) {
         n_warning("给定的位置超出字符的的长度！%d", endIndex-beginIndex);
         return NULL;
     }
     char *out = n_malloc (endIndex - beginIndex+1);
     memcpy (out, self->str+beginIndex, endIndex - beginIndex);
     out[endIndex - beginIndex] = '\0';
     NString *e = n_slice_new (NString);
     e->str=out;
     e->len=endIndex - beginIndex;
     e->allocated_len=e->len;
     return e;
 }


 NString *n_string_substring_from(NString *self,int begin,int end)
 {
 	   return substringNew(self,begin,end);
 }

 NString *n_string_substring(NString *self,int begin)
 {
 	return n_string_substring_from(self,begin,strlen(self->str));
 }

 nboolean n_string_ends_with(NString *self,const char  *suffix)
 {
	 char * res = n_strrstr (self->str, suffix);
	 if(!res)
		 return FALSE;
     return ((res-self->str)+strlen(suffix)==strlen(self->str));
 }

 void n_string_trim(NString *self)
 {
      self->str=n_strstrip(self->str);
      self->len=strlen(self->str);
 }



 NString *n_string_sized_new (nsize dfl_size)
 {
   NString *self = n_slice_new (NString);
   self->allocated_len = 0;
   self->len   = 0;
   self->str   = NULL;
   n_string_maybe_expand (self, MAX (dfl_size, 2));
   self->str[0] = 0;
   return self;
 }


 NString *n_string_new (const nchar *init)
 {
   NString *self;
   if (init == NULL || *init == '\0')
 	  self = n_string_sized_new (2);
   else{
       nint len;
       len = strlen (init);
       self = n_string_sized_new (len + 2);
       n_string_append_len (self, init, len);
   }
   return self;
 }

 int n_string_get_character_count(NString *self)
 {
     return n_utf8_strlen(self->str,-1);
 }


 NString *n_string_new_len (const nchar *init,nssize       len)
 {
   NString *string;
   if (len < 0)
     return n_string_new (init);
   else{
       string = n_string_sized_new (len);
       if (init)
         n_string_append_len (string, init, len);
       return string;
   }
 }
