

#ifndef __A_STRFUNCS_H__
#define __A_STRFUNCS_H__


#include <stdarg.h>
#include "abase.h"




/* Functions like the ones in <ctype.h> that are not affected by locale. */
typedef enum {
  A_ASCII_ALNUM  = 1 << 0,
  A_ASCII_ALPHA  = 1 << 1,
  A_ASCII_CNTRL  = 1 << 2,
  A_ASCII_DIGIT  = 1 << 3,
  A_ASCII_GRAPH  = 1 << 4,
  A_ASCII_LOWER  = 1 << 5,
  A_ASCII_PRINT  = 1 << 6,
  A_ASCII_PUNCT  = 1 << 7,
  A_ASCII_SPACE  = 1 << 8,
  A_ASCII_UPPER  = 1 << 9,
  A_ASCII_XDIGIT = 1 << 10
} AAsciiType;

extern const auint16 * const a_ascii_table;

#define a_ascii_isalnum(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_ALNUM) != 0)

#define a_ascii_isalpha(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_ALPHA) != 0)

#define a_ascii_iscntrl(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_CNTRL) != 0)

#define a_ascii_isdigit(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_DIGIT) != 0)

#define a_ascii_isgraph(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_GRAPH) != 0)

#define a_ascii_islower(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_LOWER) != 0)

#define a_ascii_isprint(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_PRINT) != 0)

#define a_ascii_ispunct(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_PUNCT) != 0)

#define a_ascii_isspace(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_SPACE) != 0)

#define a_ascii_isupper(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_UPPER) != 0)

#define a_ascii_isxdigit(c) \
  ((a_ascii_table[(auchar) (c)] & A_ASCII_XDIGIT) != 0)


achar* a_strdup(const achar *str) ;
achar* a_strndup (const achar *str,asize  n);
achar *a_strcpy (achar *dest,const achar *src);
achar* a_strconcat (const achar *string1, ...);
achar* a_strdup_vprintf (const achar *format,va_list args);
achar* a_strdup_printf  (const achar *format,...);
asize  a_strlcpy (achar *dest,const achar *src,asize dest_size);
int    a_strcmp0 (const char *str1,const char *str2);
asize  a_strerror(aint errnum,char *buffer) ;
int    a_str_last_indexof(char *source, int sourceOffset, int sourceCount,
           char *target, int targetOffset, int targetCount, int fromIndex);
char*  a_str_substring(char *source,int beginIndex,int endIndex);
achar *a_strrstr (const achar *haystack,const achar *needle);

achar* a_strchug(achar *string);
achar* a_strchomp(achar *string);
#define a_strstrip( string )	a_strchomp (a_strchug (string))
char** a_strsplit (const char *string,const char *delimiter,int max_tokens);
void    a_strfreev(achar **str_array);
auint   a_strv_length(achar **str_array);
char   *a_strstr_len (const char *haystack,assize haystack_len, const char *needle);
int     a_ascii_strcasecmp (const char *s1,const char *s2);
int     a_ascii_digit_value(char    c);


#endif /* __A_STRFUNCS_H__ */


