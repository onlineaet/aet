#include <locale.h>
#include <errno.h>
#include <ctype.h>              /* For tolower() */
#include <string.h>
#include "astrfuncs.h"
#include "alog.h"
#include "amem.h"
#include "aprintf.h"
#include "aunicode.h"


struct mapping_entry
{
  auint16 src;
  auint16 ascii;
};

struct mapping_range
{
  auint16 start;
  auint16 length;
};

struct locale_entry
{
  auint8 name_offset;
  auint8 item_id;
};

#define ISSPACE(c)              ((c) == ' ' || (c) == '\f' || (c) == '\n' || \
                                 (c) == '\r' || (c) == '\t' || (c) == '\v')
#define ISUPPER(c)              ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c)              ((c) >= 'a' && (c) <= 'z')
#define ISALPHA(c)              (ISUPPER (c) || ISLOWER (c))
#define TOUPPER(c)              (ISLOWER (c) ? (c) - 'a' + 'A' : (c))
#define TOLOWER(c)              (ISUPPER (c) ? (c) - 'A' + 'a' : (c))

#define A_NINT64_MODIFIER "l"
#define A_NINT64_FORMAT "li"
#define A_NUINT64_FORMAT "lu"

static const auint16 ascii_table_data[256] = {
  0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
  0x004, 0x104, 0x104, 0x004, 0x104, 0x104, 0x004, 0x004,
  0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
  0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
  0x140, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x459, 0x459, 0x459, 0x459, 0x459, 0x459, 0x459, 0x459,
  0x459, 0x459, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x0d0, 0x653, 0x653, 0x653, 0x653, 0x653, 0x653, 0x253,
  0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253,
  0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253,
  0x253, 0x253, 0x253, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
  0x0d0, 0x473, 0x473, 0x473, 0x473, 0x473, 0x473, 0x073,
  0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073,
  0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073,
  0x073, 0x073, 0x073, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x004
  /* the upper 128 are all zeroes */
};

const auint16 * const a_ascii_table = ascii_table_data;

achar* a_strdup (const achar *str)
{
  achar *new_str;
  asize length;

  if (str)
    {
      length = strlen (str) + 1;
      new_str = a_new (char, length);
      memcpy (new_str, str, length);
    }
  else
    new_str = NULL;
  return new_str;
}

achar *a_strcpy (achar *dest,const achar *src)
{

  achar *d = dest;
  const achar *s = src;
  a_return_val_if_fail (dest != NULL, NULL);
  a_return_val_if_fail (src != NULL, NULL);
  do
    *d++ = *s;
  while (*s++ != '\0');
  return d - 1;
}

achar* a_strconcat (const achar *string1, ...)
{
  asize   l;
  va_list args;
  achar   *s;
  achar   *concat;
  achar   *ptr;

  if (!string1)
    return NULL;

  l = 1 + strlen (string1);
  va_start (args, string1);
  s = va_arg (args, achar*);
  while (s)
    {
      l += strlen (s);
      s = va_arg (args, achar*);
    }
  va_end (args);

  concat = a_new (achar, l);
  ptr = concat;

  ptr = a_strcpy (ptr, string1);
  va_start (args, string1);
  s = va_arg (args, achar*);
  while (s)
    {
      ptr = a_strcpy (ptr, s);
      s = va_arg (args, achar*);
    }
  va_end (args);

  return concat;
}

achar* a_strdup_vprintf (const achar *format,va_list args)
{
  achar *string = NULL;
  a_vasprintf (&string, format, args);
  return string;
}


achar* a_strdup_printf (const achar *format,...)
{
  achar *buffer;
  va_list args;
  va_start (args, format);
  buffer = a_strdup_vprintf (format, args);
  va_end (args);
  return buffer;
}

asize a_strlcpy (achar *dest,const achar *src,asize dest_size)
{
  achar *d = dest;
  const achar *s = src;
  asize n = dest_size;

  a_return_val_if_fail (dest != NULL, 0);
  a_return_val_if_fail (src  != NULL, 0);

  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0)
    do
      {
        achar c = *s++;

        *d++ = c;
        if (c == 0)
          break;
      }
    while (--n != 0);

  /* If not enough room in dest, add NUL and traverse rest of src */
  if (n == 0)
    {
      if (dest_size != 0)
        *d = 0;
      while (*s++)
        ;
    }

  return s - src - 1;  /* count does not include NUL */
}

int a_strcmp0 (const char  *str1,const char *str2)
{
  if (!str1)
    return -(str1 != str2);
  if (!str2)
    return str1 != str2;
  return strcmp (str1, str2);
}

asize a_strerror(aint errnum,char *buffer)
{
    return a_strlcpy (buffer, xstrerror (errnum), sizeof (buffer));
}

static auint32 getValue(char *str,int index)
{
    char *re=a_utf8_offset_to_pointer  (str,index);
    auint32  c = a_utf8_get_char (re);
    return c;
}

int a_str_last_indexof(char *source, int sourceOffset, int sourceCount,
           char *target, int targetOffset, int targetCount, int fromIndex)
{
       int rightIndex = sourceCount - targetCount;
       if (fromIndex < 0) {
           return -1;
       }
       if (fromIndex > rightIndex) {
           fromIndex = rightIndex;
       }
       /* 空字符总是匹配。 */
       if (targetCount == 0) {
           return fromIndex;
       }
       int strLastIndex = targetOffset + targetCount - 1;
       auint32  strLastChar=getValue(target,strLastIndex);
      // char strLastChar = target[strLastIndex];
       int min = sourceOffset + targetCount - 1;
       int i = min + fromIndex;

startSearchForLastChar:
       while (TRUE) {
           while (i >= min && getValue(source,i) != strLastChar) {
               i--;
           }
           if (i < min) {
               return -1;
           }
           int j = i - 1;
           int start = j - (targetCount - 1);
           int k = strLastIndex - 1;

           while (j > start) {
               if (getValue(source,j--) != getValue(target,k--)) {
                   i--;
                   goto startSearchForLastChar;
               }
           }
           return start - sourceOffset + 1;
       }
       return -1;
}

char* a_str_substring(char *source,int beginIndex,int endIndex)
{
	int len=strlen(source);
    if (beginIndex < 0) {
        return NULL;
    }
    if (endIndex > len) {
        return NULL;
    }
    if(endIndex-beginIndex<0) {
        return NULL;
    }
   // char *out=g_utf8_substring(priv->str,beginIndex,endIndex);
    achar *start, *end, *out;
    start = a_utf8_offset_to_pointer (source, beginIndex);
    end = a_utf8_offset_to_pointer (start, endIndex - beginIndex);
    out = a_malloc (end - start+1);
    memcpy (out, start, end - start);
    out[end - start] = '\0';
   	return out;
}

achar *a_strchug (achar *string)
{
  auchar *start;
  a_return_val_if_fail (string != NULL, NULL);
  for (start = (auchar*) string; *start && a_ascii_isspace (*start); start++)
    ;
  memmove (string, start, strlen ((achar *) start) + 1);
  return string;
}

achar *a_strchomp (achar *string)
{
  asize len;
  a_return_val_if_fail (string != NULL, NULL);
  len = strlen (string);
  while (len--){
      if (a_ascii_isspace ((auchar) string[len]))
        string[len] = '\0';
      else
        break;
  }
  return string;
}

achar* a_strndup (const achar *str,asize  n)
{
  achar *new_str;
  if (str){
      new_str = a_new (achar, n + 1);
      strncpy (new_str, str, n);
      new_str[n] = '\0';
  }else
    new_str = NULL;
  return new_str;
}

achar *a_strrstr (const achar *haystack,const achar *needle)
{
  asize i;
  asize needle_len;
  asize haystack_len;
  const achar *p;

  a_return_val_if_fail (haystack != NULL, NULL);
  a_return_val_if_fail (needle != NULL, NULL);

  needle_len = strlen (needle);
  haystack_len = strlen (haystack);

  if (needle_len == 0)
    return (achar *)haystack;

  if (haystack_len < needle_len)
    return NULL;

  p = haystack + haystack_len - needle_len;

  while (p >= haystack)
    {
      for (i = 0; i < needle_len; i++)
        if (p[i] != needle[i])
          goto next;

      return (achar *)p;

    next:
      p--;
    }

  return NULL;
}

void    a_strfreev(achar **str_array)
{
  if (str_array){
	  asize i;
	  for (i = 0; str_array[i] != NULL; i++)
		a_free (str_array[i]);
	  a_free (str_array);
  }
}

auint   a_strv_length(achar **str_array)
{
	  auint i = 0;
	  a_return_val_if_fail (str_array != NULL, 0);
	  while (str_array[i])
	    ++i;
	  return i;
}

char *a_strstr_len (const char *haystack,assize haystack_len, const char *needle)
{
  a_return_val_if_fail (haystack != NULL, NULL);
  a_return_val_if_fail (needle != NULL, NULL);

  if (haystack_len < 0)
    return strstr (haystack, needle);
  else{
      const char *p = haystack;
      asize needle_len = strlen (needle);
      asize haystack_len_unsigned = haystack_len;
      const achar *end;
      asize i;
      if (needle_len == 0)
        return (achar *)haystack;

      if (haystack_len_unsigned < needle_len)
        return NULL;

      end = haystack + haystack_len - needle_len;

      while (p <= end && *p)
        {
          for (i = 0; i < needle_len; i++)
            if (p[i] != needle[i])
              goto next;

          return (achar *)p;

        next:
          p++;
        }

      return NULL;
    }
}

int a_ascii_strcasecmp (const char *s1,const char *s2)
{
  int c1, c2;

  a_return_val_if_fail (s1 != NULL, 0);
  a_return_val_if_fail (s2 != NULL, 0);

  while (*s1 && *s2)
    {
      c1 = (int)(auchar) TOLOWER (*s1);
      c2 = (int)(auchar) TOLOWER (*s2);
      if (c1 != c2)
        return (c1 - c2);
      s1++; s2++;
    }

  return (((int)(auchar) *s1) - ((int)(auchar) *s2));
}

char** a_strsplit (const char *string,const char *delimiter,int max_tokens)
{
  char *s;
  const char *remainder;
  int size=12;
  char **array=a_new(char *,size);

  a_return_val_if_fail (string != NULL, NULL);
  a_return_val_if_fail (delimiter != NULL, NULL);
  a_return_val_if_fail (delimiter[0] != '\0', NULL);

  if (max_tokens < 1)
    max_tokens = A_MAXINT;
  int count=0;
  remainder = string;
  s = strstr (remainder, delimiter);
  if (s){
      asize delimiter_len = strlen (delimiter);
      while (--max_tokens && s){
          asize len;
          len = s - remainder;
          array[count++]= a_strndup (remainder, len);
          if(count==size-1){
              array=a_mem_expand (array,&size,sizeof(char *),12);
          }
          remainder = s + delimiter_len;
          s = strstr (remainder, delimiter);
      }
  }
  if (*string)
      array[count++]= a_strdup (remainder);
  array[count++]=NULL;
  return (char **) array;
}
