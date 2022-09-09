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

#ifndef __N_STRFUNCS_H__
#define __N_STRFUNCS_H__


#include <stdarg.h>

#include "nbase.h"
#include "nerror.h"




/* Functions like the ones in <ctype.h> that are not affected by locale. */
typedef enum {
  N_ASCII_ALNUM  = 1 << 0,
  N_ASCII_ALPHA  = 1 << 1,
  N_ASCII_CNTRL  = 1 << 2,
  N_ASCII_DIGIT  = 1 << 3,
  N_ASCII_GRAPH  = 1 << 4,
  N_ASCII_LOWER  = 1 << 5,
  N_ASCII_PRINT  = 1 << 6,
  N_ASCII_PUNCT  = 1 << 7,
  N_ASCII_SPACE  = 1 << 8,
  N_ASCII_UPPER  = 1 << 9,
  N_ASCII_XDIGIT = 1 << 10
} NAsciiType;

extern const nuint16 * const n_ascii_table;

#define n_ascii_isalnum(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_ALNUM) != 0)

#define n_ascii_isalpha(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_ALPHA) != 0)

#define n_ascii_iscntrl(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_CNTRL) != 0)

#define n_ascii_isdigit(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_DIGIT) != 0)

#define n_ascii_isgraph(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_GRAPH) != 0)

#define n_ascii_islower(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_LOWER) != 0)

#define n_ascii_isprint(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_PRINT) != 0)

#define n_ascii_ispunct(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_PUNCT) != 0)

#define n_ascii_isspace(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_SPACE) != 0)

#define n_ascii_isupper(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_UPPER) != 0)

#define n_ascii_isxdigit(c) \
  ((n_ascii_table[(nuchar) (c)] & N_ASCII_XDIGIT) != 0)


nchar                 n_ascii_tolower  (nchar        c) ;

nchar                 n_ascii_toupper  (nchar        c) ;


nint                  n_ascii_digit_value  (nchar    c) ;

nint                  n_ascii_xdigit_value (nchar    c) ;

/* String utility functions that modify a string argument or
 * return a constant string that must not be freed.
 */
#define	 N_STR_DELIMITERS	"_-|> <."

nchar*	              n_strdelimit (nchar *string,const nchar  *delimiters,	nchar	      new_delimiter);

nchar*	              n_strcanon       (nchar  *string,	const nchar  *valid_chars,nchar         substitutor);

const nchar *         n_strerror       (nint errnum) ;

const nchar *         n_strsignal      (nint signum) ;

nchar *	              n_strreverse     (nchar *string);

nsize	              n_strlcpy	       (nchar *dest,const nchar  *src,nsize dest_size);

nsize	              n_strlcat        (nchar *dest,const nchar  *src,nsize         dest_size);

nchar *               n_strstr_len     (const nchar  *haystack,	nssize        haystack_len,	const nchar  *needle);

nchar *               n_strrstr        (const nchar  *haystack,const nchar  *needle);

nchar *               n_strrstr_len    (const nchar  *haystack,	nssize   haystack_len,const nchar  *needle);


nboolean              n_str_has_suffix (const nchar  *str,const nchar  *suffix);

nboolean              n_str_has_prefix (const nchar  *str,const nchar  *prefix);

/* String to/from double conversion functions */


ndouble	              n_strtod         (const nchar  *nptr,nchar	    **endptr);

ndouble	              n_ascii_strtod   (const nchar  *nptr,nchar	    **endptr);

nuint64		      n_ascii_strtoull (const nchar *nptr,nchar      **endptr,nuint        base);
nint64		      n_ascii_strtoll  (const nchar *nptr,nchar      **endptr,nuint        base);
/* 29 bytes should enough for all possible values that
 * n_ascii_dtostr can produce.
 * Then add 10 for good measure */
#define N_ASCII_DTOSTR_BUF_SIZE (29 + 10)

nchar *               n_ascii_dtostr   (nchar        *buffer,nint buf_len,ndouble d);

nchar *               n_ascii_formatd  (nchar        *buffer,nint buf_len,const nchar  *format,ndouble  d);

/* removes leading spaces */

nchar*                n_strchug        (nchar        *string);
/* removes trailing spaces */

nchar*                n_strchomp       (nchar        *string);
/* removes leading & trailing spaces */
#define n_strstrip( string )	n_strchomp (n_strchug (string))


nint                  n_ascii_strcasecmp  (const nchar *s1,   const nchar *s2);
nint                  n_ascii_strncasecmp (const nchar *s1,  const nchar *s2,  nsize        n);
nchar*                n_ascii_strdown     (const nchar *str, nssize       len) ;
nchar*                n_ascii_strup       (const nchar *str, nssize       len) ;
nboolean              n_str_is_ascii      (const nchar *str);




/* String utility functions that return a newly allocated string which
 * ought to be freed with n_free from the caller at some point.
 */

nchar*	              n_strdup	       (const nchar *str) ;
nchar*	              n_strdup_printf  (const nchar *format,...);
nchar*	              n_strdup_vprintf (const nchar *format,va_list args);
nchar*	              n_strndup	       (const nchar *str,nsize        n) ;
nchar*	              n_strnfill       (nsize  length,nchar  fill_char) ;
nchar*	              n_strconcat      (const nchar *string1,...)  ;
nchar*                n_strjoin	       (const nchar  *separator,...)  ;
nchar*                n_strcompress    (const nchar *source) ;
nchar*                n_strescape      (const nchar *source,const nchar *exceptions) ;


typedef nchar** NStrv;

nchar**	          n_strsplit(const nchar  *string,const nchar  *delimiter,nint max_tokens);
nchar*            n_strjoinv       (const nchar  *separator,nchar       **str_array) ;
void              n_strfreev       (nchar       **str_array);
nchar**           n_strdupv        (nchar       **str_array);
nuint             n_strv_length    (nchar       **str_array);
nchar*            n_stpcpy         (nchar        *dest,const char   *src);
nchar *           n_str_to_ascii(const nchar   *str,const nchar   *from_locale);
nchar **          n_str_tokenize_and_fold(const nchar   *string,const nchar   *translit_locale,nchar       ***ascii_alternates);
nboolean          n_str_match_string(const nchar   *search_term,const nchar   *potential_hit,nboolean       accept_alternates);
nboolean          n_strv_contains  (const nchar * const *strv,  const nchar         *str);
nboolean          n_strv_equal     (const nchar * const *strv1,   const nchar * const *strv2);

/* Convenience ASCII string to number API */

/**
 * GNumberParserError:
 * @N_NUMBER_PARSER_ERROR_INVALID: String was not a valid number.
 * @N_NUMBER_PARSER_ERROR_OUT_OF_BOUNDS: String was a number, but out of bounds.
 *
 * Error codes returned by functions converting a string to a number.
 *
 * Since: 2.54
 */
typedef enum
  {
    N_NUMBER_PARSER_ERROR_INVALID,
    N_NUMBER_PARSER_ERROR_OUT_OF_BOUNDS,
  } NNumberParserError;


#define N_NUMBER_PARSER_ERROR 2// (n_number_parser_error_quark ())


//NQuark                n_number_parser_error_quark  (void);


nboolean              n_ascii_string_to_signed     (const nchar  *str,
                                                    nuint         base,
                                                    nint64        min,
                                                    nint64        max,
                                                    nint64       *out_num,
                                                    NError      **error);


nboolean              n_ascii_string_to_unsigned   (const nchar  *str,
                                                    nuint         base,
                                                    nuint64       min,
                                                    nuint64       max,
                                                    nuint64      *out_num,
                                                    NError      **error);

int n_strcmp0 (const char     *str1,const char     *str2);


#endif /* __N_STRFUNCS_H__ */
