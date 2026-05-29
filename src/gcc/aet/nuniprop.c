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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <locale.h>

#include "nmem.h"
#include "nunicode.h"
#include "nunidecomp.h"
#include "nunichartables.h"
#include "nscripttable.h"
#include "nmirroringtable.h"
#include "nstring.h"
#include "nlog.h"




#define N_UNICHAR_FULLWIDTH_A 0xff21
#define N_UNICHAR_FULLWIDTH_I 0xff29
#define N_UNICHAR_FULLWIDTH_J 0xff2a
#define N_UNICHAR_FULLWIDTH_F 0xff26
#define N_UNICHAR_FULLWIDTH_a 0xff41
#define N_UNICHAR_FULLWIDTH_f 0xff46

#define ATTR_TABLE(Page) (((Page) <= N_UNICODE_LAST_PAGE_PART1) \
                          ? attr_table_part1[Page] \
                          : attr_table_part2[(Page) - 0xe00])

#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == N_UNICODE_MAX_TABLE_INDEX) ? 0 : (attr_data[ATTR_TABLE(Page)][Char]))

#define TTYPE_PART1(Page, Char) \
  ((type_table_part1[Page] >= N_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part1[Page] - N_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part1[Page]][Char]))

#define TTYPE_PART2(Page, Char) \
  ((type_table_part2[Page] >= N_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part2[Page] - N_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part2[Page]][Char]))

#define TYPE(Char) \
  (((Char) <= N_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= N_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : N_UNICODE_UNASSIGNED))


#define IS(Type, Class)	(((nuint)1 << (Type)) & (Class))
#define OR(Type, Rest)	(((nuint)1 << (Type)) | (Rest))



#define ISALPHA(Type)	IS ((Type),				\
			    OR (N_UNICODE_LOWERCASE_LETTER,	\
			    OR (N_UNICODE_UPPERCASE_LETTER,	\
			    OR (N_UNICODE_TITLECASE_LETTER,	\
			    OR (N_UNICODE_MODIFIER_LETTER,	\
			    OR (N_UNICODE_OTHER_LETTER,		0))))))

#define ISALDIGIT(Type)	IS ((Type),				\
			    OR (N_UNICODE_DECIMAL_NUMBER,	\
			    OR (N_UNICODE_LETTER_NUMBER,	\
			    OR (N_UNICODE_OTHER_NUMBER,		\
			    OR (N_UNICODE_LOWERCASE_LETTER,	\
			    OR (N_UNICODE_UPPERCASE_LETTER,	\
			    OR (N_UNICODE_TITLECASE_LETTER,	\
			    OR (N_UNICODE_MODIFIER_LETTER,	\
			    OR (N_UNICODE_OTHER_LETTER,		0)))))))))

#define ISMARK(Type)	IS ((Type),				\
			    OR (N_UNICODE_NON_SPACINN_MARK,	\
			    OR (N_UNICODE_SPACINN_MARK,	\
			    OR (N_UNICODE_ENCLOSINN_MARK,	0))))

#define ISZEROWIDTHTYPE(Type)	IS ((Type),			\
			    OR (N_UNICODE_NON_SPACINN_MARK,	\
			    OR (N_UNICODE_ENCLOSINN_MARK,	\
			    OR (N_UNICODE_FORMAT,		0))))

/**
 * n_unichar_isalnum:
 * @c: a Unicode character
 * 
 * Determines whether a character is alphanumeric.
 * Given some UTF-8 text, obtain a character value
 * with n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is an alphanumeric character
 **/
nboolean
n_unichar_isalnum (nunichar c)
{
  return ISALDIGIT (TYPE (c)) ? TRUE : FALSE;
}

/**
 * n_unichar_isalpha:
 * @c: a Unicode character
 * 
 * Determines whether a character is alphabetic (i.e. a letter).
 * Given some UTF-8 text, obtain a character value with
 * n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is an alphabetic character
 **/
nboolean
n_unichar_isalpha (nunichar c)
{
  return ISALPHA (TYPE (c)) ? TRUE : FALSE;
}


/**
 * n_unichar_iscntrl:
 * @c: a Unicode character
 * 
 * Determines whether a character is a control character.
 * Given some UTF-8 text, obtain a character value with
 * n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a control character
 **/
nboolean
n_unichar_iscntrl (nunichar c)
{
  return TYPE (c) == N_UNICODE_CONTROL;
}

/**
 * n_unichar_isdigit:
 * @c: a Unicode character
 * 
 * Determines whether a character is numeric (i.e. a digit).  This
 * covers ASCII 0-9 and also digits in other languages/scripts.  Given
 * some UTF-8 text, obtain a character value with n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a digit
 **/
nboolean
n_unichar_isdigit (nunichar c)
{
  return TYPE (c) == N_UNICODE_DECIMAL_NUMBER;
}


/**
 * n_unichar_isgraph:
 * @c: a Unicode character
 * 
 * Determines whether a character is printable and not a space
 * (returns %FALSE for control characters, format characters, and
 * spaces). n_unichar_isprint() is similar, but returns %TRUE for
 * spaces. Given some UTF-8 text, obtain a character value with
 * n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is printable unless it's a space
 **/
nboolean
n_unichar_isgraph (nunichar c)
{
  return !IS (TYPE(c),
	      OR (N_UNICODE_CONTROL,
	      OR (N_UNICODE_FORMAT,
	      OR (N_UNICODE_UNASSIGNED,
	      OR (N_UNICODE_SURROGATE,
	      OR (N_UNICODE_SPACE_SEPARATOR,
	     0))))));
}

/**
 * n_unichar_islower:
 * @c: a Unicode character
 * 
 * Determines whether a character is a lowercase letter.
 * Given some UTF-8 text, obtain a character value with
 * n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a lowercase letter
 **/
nboolean
n_unichar_islower (nunichar c)
{
  return TYPE (c) == N_UNICODE_LOWERCASE_LETTER;
}


/**
 * n_unichar_isprint:
 * @c: a Unicode character
 * 
 * Determines whether a character is printable.
 * Unlike n_unichar_isgraph(), returns %TRUE for spaces.
 * Given some UTF-8 text, obtain a character value with
 * n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is printable
 **/
nboolean
n_unichar_isprint (nunichar c)
{
  return !IS (TYPE(c),
	      OR (N_UNICODE_CONTROL,
	      OR (N_UNICODE_FORMAT,
	      OR (N_UNICODE_UNASSIGNED,
	      OR (N_UNICODE_SURROGATE,
	     0)))));
}

/**
 * n_unichar_ispunct:
 * @c: a Unicode character
 * 
 * Determines whether a character is punctuation or a symbol.
 * Given some UTF-8 text, obtain a character value with
 * n_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a punctuation or symbol character
 **/
nboolean
n_unichar_ispunct (nunichar c)
{
  return IS (TYPE(c),
	     OR (N_UNICODE_CONNECT_PUNCTUATION,
	     OR (N_UNICODE_DASH_PUNCTUATION,
	     OR (N_UNICODE_CLOSE_PUNCTUATION,
	     OR (N_UNICODE_FINAL_PUNCTUATION,
	     OR (N_UNICODE_INITIAL_PUNCTUATION,
	     OR (N_UNICODE_OTHER_PUNCTUATION,
	     OR (N_UNICODE_OPEN_PUNCTUATION,
	     OR (N_UNICODE_CURRENCY_SYMBOL,
	     OR (N_UNICODE_MODIFIER_SYMBOL,
	     OR (N_UNICODE_MATH_SYMBOL,
	     OR (N_UNICODE_OTHER_SYMBOL,
	    0)))))))))))) ? TRUE : FALSE;
}

/**
 * n_unichar_isspace:
 * @c: a Unicode character
 * 
 * Determines whether a character is a space, tab, or line separator
 * (newline, carriage return, etc.).  Given some UTF-8 text, obtain a
 * character value with n_utf8_get_char().
 *
 * (Note: don't use this to do word breaking; you have to use
 * Pango or equivalent to get word breaking right, the algorithm
 * is fairly complex.)
 *  
 * Returns: %TRUE if @c is a space character
 **/
nboolean n_unichar_isspace (nunichar c)
{
  switch (c)
    {
      /* special-case these since Unicode thinks they are not spaces */
    case '\t':
    case '\n':
    case '\r':
    case '\f':
      return TRUE;
      break;
      
    default:
      {
	return IS (TYPE(c),
	           OR (N_UNICODE_SPACE_SEPARATOR,
	           OR (N_UNICODE_LINE_SEPARATOR,
                   OR (N_UNICODE_PARAGRAPH_SEPARATOR,
		  0)))) ? TRUE : FALSE;
      }
      break;
    }
}

/**
 * n_unichar_ismark:
 * @c: a Unicode character
 *
 * Determines whether a character is a mark (non-spacing mark,
 * combining mark, or enclosing mark in Unicode speak).
 * Given some UTF-8 text, obtain a character value
 * with n_utf8_get_char().
 *
 * Note: in most cases where isalpha characters are allowed,
 * ismark characters should be allowed to as they are essential
 * for writing most European languages as well as many non-Latin
 * scripts.
 *
 * Returns: %TRUE if @c is a mark character
 *
 * Since: 2.14
 **/
nboolean n_unichar_ismark (nunichar c)
{
  return ISMARK (TYPE (c));
}

/**
 * n_unichar_isupper:
 * @c: a Unicode character
 * 
 * Determines if a character is uppercase.
 * 
 * Returns: %TRUE if @c is an uppercase character
 **/
nboolean n_unichar_isupper (nunichar c)
{
  return TYPE (c) == N_UNICODE_UPPERCASE_LETTER;
}

/**
 * n_unichar_istitle:
 * @c: a Unicode character
 * 
 * Determines if a character is titlecase. Some characters in
 * Unicode which are composites, such as the DZ digraph
 * have three case variants instead of just two. The titlecase
 * form is used at the beginning of a word where only the
 * first letter is capitalized. The titlecase form of the DZ
 * digraph is U+01F2 LATIN CAPITAL LETTTER D WITH SMALL LETTER Z.
 * 
 * Returns: %TRUE if the character is titlecase
 **/
nboolean n_unichar_istitle (nunichar c)
{
  unsigned int i;
  for (i = 0; i < N_N_ELEMENTS (title_table); ++i)
    if (title_table[i][0] == c)
      return TRUE;
  return FALSE;
}

/**
 * n_unichar_isxdigit:
 * @c: a Unicode character.
 * 
 * Determines if a character is a hexadecimal digit.
 * 
 * Returns: %TRUE if the character is a hexadecimal digit
 **/
nboolean n_unichar_isxdigit (nunichar c)
{
  return ((c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F') ||
          (c >= N_UNICHAR_FULLWIDTH_a && c <= N_UNICHAR_FULLWIDTH_f) ||
          (c >= N_UNICHAR_FULLWIDTH_A && c <= N_UNICHAR_FULLWIDTH_F) ||
          (TYPE (c) == N_UNICODE_DECIMAL_NUMBER));
}

/**
 * n_unichar_isdefined:
 * @c: a Unicode character
 * 
 * Determines if a given character is assigned in the Unicode
 * standard.
 *
 * Returns: %TRUE if the character has an assigned value
 **/
nboolean n_unichar_isdefined (nunichar c)
{
  return !IS (TYPE(c),
	      OR (N_UNICODE_UNASSIGNED,
	      OR (N_UNICODE_SURROGATE,
	     0)));
}

/**
 * n_unichar_iszerowidth:
 * @c: a Unicode character
 * 
 * Determines if a given character typically takes zero width when rendered.
 * The return value is %TRUE for all non-spacing and enclosing marks
 * (e.g., combining accents), format characters, zero-width
 * space, but not U+00AD SOFT HYPHEN.
 *
 * A typical use of this function is with one of n_unichar_iswide() or
 * n_unichar_iswide_cjk() to determine the number of cells a string occupies
 * when displayed on a grid display (terminals).  However, note that not all
 * terminals support zero-width rendering of zero-width marks.
 *
 * Returns: %TRUE if the character has zero width
 *
 * Since: 2.14
 **/
nboolean n_unichar_iszerowidth (nunichar c)
{
  if (N_UNLIKELY (c == 0x00AD))
    return FALSE;

  if (N_UNLIKELY (ISZEROWIDTHTYPE (TYPE (c))))
    return TRUE;

  if (N_UNLIKELY ((c >= 0x1160 && c < 0x1200) ||
		  c == 0x200B))
    return TRUE;

  return FALSE;
}

static int interval_compare (const void *key, const void *elt)
{
  nunichar c = NPOINTER_TO_UINT (key);
  struct Interval *interval = (struct Interval *)elt;

  if (c < interval->start)
    return -1;
  if (c > interval->end)
    return +1;

  return 0;
}

#define N_WIDTH_TABLE_MIDPOINT (N_N_ELEMENTS (n_unicode_width_table_wide) / 2)

static inline nboolean n_unichar_iswide_bsearch (nunichar ch)
{
  int lower = 0;
  int upper = N_N_ELEMENTS (n_unicode_width_table_wide) - 1;
  static int saved_mid = N_WIDTH_TABLE_MIDPOINT;
  int mid = saved_mid;

  do
    {
      if (ch < n_unicode_width_table_wide[mid].start)
	upper = mid - 1;
      else if (ch > n_unicode_width_table_wide[mid].end)
	lower = mid + 1;
      else
	return TRUE;

      mid = (lower + upper) / 2;
    }
  while (lower <= upper);

  return FALSE;
}

/**
 * n_unichar_iswide:
 * @c: a Unicode character
 * 
 * Determines if a character is typically rendered in a double-width
 * cell.
 * 
 * Returns: %TRUE if the character is wide
 **/
nboolean n_unichar_iswide (nunichar c)
{
  if (c < n_unicode_width_table_wide[0].start)
    return FALSE;
  else
    return n_unichar_iswide_bsearch (c);
}


/**
 * n_unichar_iswide_cjk:
 * @c: a Unicode character
 * 
 * Determines if a character is typically rendered in a double-width
 * cell under legacy East Asian locales.  If a character is wide according to
 * n_unichar_iswide(), then it is also reported wide with this function, but
 * the converse is not necessarily true. See the
 * [Unicode Standard Annex #11](http://www.unicode.org/reports/tr11/)
 * for details.
 *
 * If a character passes the n_unichar_iswide() test then it will also pass
 * this test, but not the other way around.  Note that some characters may
 * pass both this test and n_unichar_iszerowidth().
 * 
 * Returns: %TRUE if the character is wide in legacy East Asian locales
 *
 * Since: 2.12
 */
nboolean n_unichar_iswide_cjk (nunichar c)
{
  if (n_unichar_iswide (c))
    return TRUE;

  /* bsearch() is declared attribute(nonnull(1)) so we can't validly search
   * for a NULL key */
  if (c == 0)
    return FALSE;

  if (bsearch (NUINT_TO_POINTER (c),
               n_unicode_width_table_ambiguous,
               N_N_ELEMENTS (n_unicode_width_table_ambiguous),
               sizeof n_unicode_width_table_ambiguous[0],
	       interval_compare))
    return TRUE;

  return FALSE;
}


/**
 * n_unichar_toupper:
 * @c: a Unicode character
 * 
 * Converts a character to uppercase.
 * 
 * Returns: the result of converting @c to uppercase.
 *               If @c is not a lowercase or titlecase character,
 *               or has no upper case equivalent @c is returned unchanged.
 **/
nunichar n_unichar_toupper (nunichar c)
{
  int t = TYPE (c);
  if (t == N_UNICODE_LOWERCASE_LETTER)
    {
      nunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const nchar *p = special_case_table + val - 0x1000000;
          val = n_utf8_get_char (p);
	}
      /* Some lowercase letters, e.g., U+000AA, FEMININE ORDINAL INDICATOR,
       * do not have an uppercase equivalent, in which case val will be
       * zero. 
       */
      return val ? val : c;
    }
  else if (t == N_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < N_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][1] ? title_table[i][1] : c;
	}
    }
  return c;
}

/**
 * n_unichar_tolower:
 * @c: a Unicode character.
 * 
 * Converts a character to lower case.
 * 
 * Returns: the result of converting @c to lower case.
 *               If @c is not an upperlower or titlecase character,
 *               or has no lowercase equivalent @c is returned unchanged.
 **/
nunichar n_unichar_tolower (nunichar c)
{
  int t = TYPE (c);
  if (t == N_UNICODE_UPPERCASE_LETTER)
    {
      nunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const nchar *p = special_case_table + val - 0x1000000;
	  return n_utf8_get_char (p);
	}
      else
	{
	  /* Not all uppercase letters are guaranteed to have a lowercase
	   * equivalent.  If this is the case, val will be zero. */
	  return val ? val : c;
	}
    }
  else if (t == N_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < N_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][2];
	}
    }
  return c;
}

/**
 * n_unichar_totitle:
 * @c: a Unicode character
 * 
 * Converts a character to the titlecase.
 * 
 * Returns: the result of converting @c to titlecase.
 *               If @c is not an uppercase or lowercase character,
 *               @c is returned unchanged.
 **/
nunichar n_unichar_totitle (nunichar c)
{
  unsigned int i;

  /* We handle U+0000 explicitly because some elements in
   * title_table[i][1] may be null. */
  if (c == 0)
    return c;

  for (i = 0; i < N_N_ELEMENTS (title_table); ++i)
    {
      if (title_table[i][0] == c || title_table[i][1] == c
	  || title_table[i][2] == c)
	return title_table[i][0];
    }

  if (TYPE (c) == N_UNICODE_LOWERCASE_LETTER)
    return n_unichar_toupper (c);

  return c;
}

/**
 * n_unichar_digit_value:
 * @c: a Unicode character
 *
 * Determines the numeric value of a character as a decimal
 * digit.
 *
 * Returns: If @c is a decimal digit (according to
 * n_unichar_isdigit()), its numeric value. Otherwise, -1.
 **/
int n_unichar_digit_value (nunichar c)
{
  if (TYPE (c) == N_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * n_unichar_xdigit_value:
 * @c: a Unicode character
 *
 * Determines the numeric value of a character as a hexadecimal
 * digit.
 *
 * Returns: If @c is a hex digit (according to
 * n_unichar_isxdigit()), its numeric value. Otherwise, -1.
 **/
int n_unichar_xdigit_value (nunichar c)
{
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= N_UNICHAR_FULLWIDTH_A && c <= N_UNICHAR_FULLWIDTH_F)
    return c - N_UNICHAR_FULLWIDTH_A + 10;
  if (c >= N_UNICHAR_FULLWIDTH_a && c <= N_UNICHAR_FULLWIDTH_f)
    return c - N_UNICHAR_FULLWIDTH_a + 10;
  if (TYPE (c) == N_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * n_unichar_type:
 * @c: a Unicode character
 * 
 * Classifies a Unicode character by type.
 * 
 * Returns: the type of the character.
 **/
NUnicodeType n_unichar_type (nunichar c)
{
  return TYPE (c);
}

/*
 * Case mapping functions
 */

typedef enum {
  LOCALE_NORMAL,
  LOCALE_TURKIC,
  LOCALE_LITHUANIAN
} LocaleType;

static LocaleType get_locale_type (void)
{

  const char *locale = setlocale (LC_CTYPE, NULL);

  if (locale == NULL)
    return LOCALE_NORMAL;

  switch (locale[0])
    {
   case 'a':
      if (locale[1] == 'z')
	return LOCALE_TURKIC;
      break;
    case 'l':
      if (locale[1] == 't')
	return LOCALE_LITHUANIAN;
      break;
    case 't':
      if (locale[1] == 'r')
	return LOCALE_TURKIC;
      break;
    }

  return LOCALE_NORMAL;
}

static nint output_marks (const char **p_inout,
	      char        *out_buffer,
	      nboolean     remove_dot)
{
  const char *p = *p_inout;
  nint len = 0;
  
  while (*p)
    {
      nunichar c = n_utf8_get_char (p);
      
      if (ISMARK (TYPE (c)))
	{
	  if (!remove_dot || c != 0x307 /* COMBINING DOT ABOVE */)
	    len += n_unichar_to_utf8 (c, out_buffer ? out_buffer + len : NULL);
	  p = n_utf8_next_char (p);
	}
      else
	break;
    }

  *p_inout = p;
  return len;
}

static nint output_special_case (nchar *out_buffer,
		     int    offset,
		     int    type,
		     int    which)
{
  const nchar *p = special_case_table + offset;
  nint len;

  if (type != N_UNICODE_TITLECASE_LETTER)
    p = n_utf8_next_char (p);

  if (which == 1)
    p += strlen (p) + 1;

  len = strlen (p);
  if (out_buffer)
    memcpy (out_buffer, p, len);

  return len;
}

static nsize
real_toupper (const nchar *str,
	      nssize       max_len,
	      nchar       *out_buffer,
	      LocaleType   locale_type)
{
  const nchar *p = str;
  const char *last = NULL;
  nsize len = 0;
  nboolean last_was_i = FALSE;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      nunichar c = n_utf8_get_char (p);
      int t = TYPE (c);
      nunichar val;

      last = p;
      p = n_utf8_next_char (p);

      if (locale_type == LOCALE_LITHUANIAN)
	{
	  if (c == 'i')
	    last_was_i = TRUE;
	  else 
	    {
	      if (last_was_i)
		{
		  /* Nasty, need to remove any dot above. Though
		   * I think only E WITH DOT ABOVE occurs in practice
		   * which could simplify this considerably.
		   */
		  nsize decomp_len, i;
		  nunichar decomp[N_UNICHAR_MAX_DECOMPOSITION_LENGTH];

		  decomp_len = n_unichar_fully_decompose (c, FALSE, decomp, N_N_ELEMENTS (decomp));
		  for (i=0; i < decomp_len; i++)
		    {
		      if (decomp[i] != 0x307 /* COMBINING DOT ABOVE */)
			len += n_unichar_to_utf8 (n_unichar_toupper (decomp[i]), out_buffer ? out_buffer + len : NULL);
		    }
		  
		  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, TRUE);

		  continue;
		}

	      if (!ISMARK (t))
		last_was_i = FALSE;
	    }
	}

      if (locale_type == LOCALE_TURKIC && c == 'i')
	{
	  /* i => LATIN CAPITAL LETTER I WITH DOT ABOVE */
	  len += n_unichar_to_utf8 (0x130, out_buffer ? out_buffer + len : NULL);
	}
      else if (c == 0x0345)	/* COMBINING GREEK YPOGEGRAMMENI */
	{
	  /* Nasty, need to move it after other combining marks .. this would go away if
	   * we normalized first.
	   */
	  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, FALSE);

	  /* And output as GREEK CAPITAL LETTER IOTA */
	  len += n_unichar_to_utf8 (0x399, out_buffer ? out_buffer + len : NULL);
	}
      else if (IS (t,
		   OR (N_UNICODE_LOWERCASE_LETTER,
		   OR (N_UNICODE_TITLECASE_LETTER,
		  0))))
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t,
					  t == N_UNICODE_LOWERCASE_LETTER ? 0 : 1);
	    }
	  else
	    {
	      if (t == N_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < N_N_ELEMENTS (title_table); ++i)
		    {
		      if (title_table[i][0] == c)
			{
			  val = title_table[i][1];
			  break;
			}
		    }
		}

	      /* Some lowercase letters, e.g., U+000AA, FEMININE ORDINAL INDICATOR,
	       * do not have an uppercase equivalent, in which case val will be
	       * zero. */
	      len += n_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  nsize char_len = n_utf8_skip[*(nuchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

/**
 * n_utf8_strup:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts all Unicode characters in the string that have a case
 * to uppercase. The exact manner that this is done depends
 * on the current locale, and may result in the number of
 * characters in the string increasing. (For instance, the
 * German ess-zet will be changed to SS.)
 * 
 * Returns: a newly allocated string, with all characters
 *    converted to uppercase.  
 **/
nchar *
n_utf8_strup (const nchar *str,
	      nssize       len)
{
  nsize result_len;
  LocaleType locale_type;
  nchar *result;

  n_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();
  
  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_toupper (str, len, NULL, locale_type);
  result = n_malloc (result_len + 1);
  real_toupper (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

/* traverses the string checking for characters with combining class == 230
 * until a base character is found */
static nboolean
has_more_above (const nchar *str)
{
  const nchar *p = str;
  nint combininn_class;

  while (*p)
    {
      combininn_class = n_unichar_combining_class (n_utf8_get_char (p));
      if (combininn_class == 230)
        return TRUE;
      else if (combininn_class == 0)
        break;

      p = n_utf8_next_char (p);
    }

  return FALSE;
}

static nsize
real_tolower (const nchar *str,
	      nssize       max_len,
	      nchar       *out_buffer,
	      LocaleType   locale_type)
{
  const nchar *p = str;
  const char *last = NULL;
  nsize len = 0;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      nunichar c = n_utf8_get_char (p);
      int t = TYPE (c);
      nunichar val;

      last = p;
      p = n_utf8_next_char (p);

      if (locale_type == LOCALE_TURKIC && (c == 'I' ||
                                           c == N_UNICHAR_FULLWIDTH_I))
	{
          if (n_utf8_get_char (p) == 0x0307)
            {
              /* I + COMBINING DOT ABOVE => i (U+0069) */
              len += n_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL);
              p = n_utf8_next_char (p);
            }
          else
            {
              /* I => LATIN SMALL LETTER DOTLESS I */
              len += n_unichar_to_utf8 (0x131, out_buffer ? out_buffer + len : NULL);
            }
        }
      /* Introduce an explicit dot above when lowercasing capital I's and J's
       * whenever there are more accents above. [SpecialCasing.txt] */
      else if (locale_type == LOCALE_LITHUANIAN && 
               (c == 0x00cc || c == 0x00cd || c == 0x0128))
        {
          len += n_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL);
          len += n_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL);

          switch (c)
            {
            case 0x00cc: 
              len += n_unichar_to_utf8 (0x0300, out_buffer ? out_buffer + len : NULL);
              break;
            case 0x00cd: 
              len += n_unichar_to_utf8 (0x0301, out_buffer ? out_buffer + len : NULL);
              break;
            case 0x0128: 
              len += n_unichar_to_utf8 (0x0303, out_buffer ? out_buffer + len : NULL);
              break;
            }
        }
      else if (locale_type == LOCALE_LITHUANIAN && 
               (c == 'I' || c == N_UNICHAR_FULLWIDTH_I ||
                c == 'J' || c == N_UNICHAR_FULLWIDTH_J || c == 0x012e) &&
               has_more_above (p))
        {
          len += n_unichar_to_utf8 (n_unichar_tolower (c), out_buffer ? out_buffer + len : NULL);
          len += n_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL);
        }
      else if (c == 0x03A3)	/* GREEK CAPITAL LETTER SIGMA */
	{
	  if ((max_len < 0 || p < str + max_len) && *p)
	    {
	      nunichar next_c = n_utf8_get_char (p);
	      int next_type = TYPE(next_c);

	      /* SIGMA mapps differently depending on whether it is
	       * final or not. The following simplified test would
	       * fail in the case of combining marks following the
	       * sigma, but I don't think that occurs in real text.
	       * The test here matches that in ICU.
	       */
	      if (ISALPHA (next_type)) /* Lu,Ll,Lt,Lm,Lo */
		val = 0x3c3;	/* GREEK SMALL SIGMA */
	      else
		val = 0x3c2;	/* GREEK SMALL FINAL SIGMA */
	    }
	  else
	    val = 0x3c2;	/* GREEK SMALL FINAL SIGMA */

	  len += n_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
	}
      else if (IS (t,
		   OR (N_UNICODE_UPPERCASE_LETTER,
		   OR (N_UNICODE_TITLECASE_LETTER,
		  0))))
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t, 0);
	    }
	  else
	    {
	      if (t == N_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < N_N_ELEMENTS (title_table); ++i)
		    {
		      if (title_table[i][0] == c)
			{
			  val = title_table[i][2];
			  break;
			}
		    }
		}

	      /* Not all uppercase letters are guaranteed to have a lowercase
	       * equivalent.  If this is the case, val will be zero. */
	      len += n_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  nsize char_len = n_utf8_skip[*(nuchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

/**
 * n_utf8_strdown:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts all Unicode characters in the string that have a case
 * to lowercase. The exact manner that this is done depends
 * on the current locale, and may result in the number of
 * characters in the string changing.
 * 
 * Returns: a newly allocated string, with all characters
 *    converted to lowercase.  
 **/
nchar *
n_utf8_strdown (const nchar *str,
		nssize       len)
{
  nsize result_len;
  LocaleType locale_type;
  nchar *result;

  n_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();
  
  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_tolower (str, len, NULL, locale_type);
  result = n_malloc (result_len + 1);
  real_tolower (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

/**
 * n_utf8_casefold:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts a string into a form that is independent of case. The
 * result will not correspond to any particular case, but can be
 * compared for equality or ordered with the results of calling
 * n_utf8_casefold() on other strings.
 * 
 * Note that calling n_utf8_casefold() followed by n_utf8_collate() is
 * only an approximation to the correct linguistic case insensitive
 * ordering, though it is a fairly good one. Getting this exactly
 * right would require a more sophisticated collation function that
 * takes case sensitivity into account. GLib does not currently
 * provide such a function.
 * 
 * Returns: a newly allocated string, that is a
 *   case independent form of @str.
 **/
nchar *n_utf8_casefold (const nchar *str,
		 nssize       len)
{
  NString *result;
  const char *p;

  n_return_val_if_fail (str != NULL, NULL);

  result = n_string_new (NULL);
  p = str;
  while ((len < 0 || p < str + len) && *p)
    {
      nunichar ch = n_utf8_get_char (p);

      int start = 0;
      int end = N_N_ELEMENTS (casefold_table);

      if (ch >= casefold_table[start].ch &&
          ch <= casefold_table[end - 1].ch)
	{
	  while (TRUE)
	    {
	      int half = (start + end) / 2;
	      if (ch == casefold_table[half].ch)
		{
		  n_string_append (result, casefold_table[half].data);
		  goto next;
		}
	      else if (half == start)
		break;
	      else if (ch > casefold_table[half].ch)
		start = half;
	      else
		end = half;
	    }
	}

      n_string_append_unichar (result, n_unichar_tolower (ch));
      
    next:
      p = n_utf8_next_char (p);
    }

  return n_string_free (result, FALSE);
}

/**
 * n_unichar_get_mirror_char:
 * @ch: a Unicode character
 * @mirrored_ch: location to store the mirrored character
 * 
 * In Unicode, some characters are "mirrored". This means that their
 * images are mirrored horizontally in text that is laid out from right
 * to left. For instance, "(" would become its mirror image, ")", in
 * right-to-left text.
 *
 * If @ch has the Unicode mirrored property and there is another unicode
 * character that typically has a glyph that is the mirror image of @ch's
 * glyph and @mirrored_ch is set, it puts that character in the address
 * pointed to by @mirrored_ch.  Otherwise the original character is put.
 *
 * Returns: %TRUE if @ch has a mirrored character, %FALSE otherwise
 *
 * Since: 2.4
 **/
nboolean
n_unichar_get_mirror_char (nunichar ch,
                           nunichar *mirrored_ch)
{
  nboolean found;
  nunichar mirrored;

  mirrored = NLIB_GET_MIRRORING(ch);

  found = ch != mirrored;
  if (mirrored_ch)
    *mirrored_ch = mirrored;

  return found;

}

#define N_SCRIPT_TABLE_MIDPOINT (N_N_ELEMENTS (n_script_table) / 2)

static inline NUnicodeScript
n_unichar_get_script_bsearch (nunichar ch)
{
  int lower = 0;
  int upper = N_N_ELEMENTS (n_script_table) - 1;
  static int saved_mid = N_SCRIPT_TABLE_MIDPOINT;
  int mid = saved_mid;


  do 
    {
      if (ch < n_script_table[mid].start)
	upper = mid - 1;
      else if (ch >= n_script_table[mid].start + n_script_table[mid].chars)
	lower = mid + 1;
      else
	return n_script_table[saved_mid = mid].script;

      mid = (lower + upper) / 2;
    }
  while (lower <= upper);

  return N_UNICODE_SCRIPT_UNKNOWN;
}

/**
 * n_unichar_get_script:
 * @ch: a Unicode character
 * 
 * Looks up the #NUnicodeScript for a particular character (as defined
 * by Unicode Standard Annex \#24). No check is made for @ch being a
 * valid Unicode character; if you pass in invalid character, the
 * result is undefined.
 *
 * This function is equivalent to pango_script_for_unichar() and the
 * two are interchangeable.
 * 
 * Returns: the #NUnicodeScript for the character.
 *
 * Since: 2.14
 */
NUnicodeScript
n_unichar_get_script (nunichar ch)
{
  if (ch < N_EASY_SCRIPTS_RANGE)
    return n_script_easy_table[ch];
  else 
    return n_unichar_get_script_bsearch (ch);
}


/* http://unicode.org/iso15924/ */
static const nuint32 iso15924_tags[] =
{
#define PACK(a,b,c,d) ((nuint32)((((nuint8)(a))<<24)|(((nuint8)(b))<<16)|(((nuint8)(c))<<8)|((nuint8)(d))))

    PACK ('Z','y','y','y'), /* N_UNICODE_SCRIPT_COMMON */
    PACK ('Z','i','n','h'), /* N_UNICODE_SCRIPT_INHERITED */
    PACK ('A','r','a','b'), /* N_UNICODE_SCRIPT_ARABIC */
    PACK ('A','r','m','n'), /* N_UNICODE_SCRIPT_ARMENIAN */
    PACK ('B','e','n','g'), /* N_UNICODE_SCRIPT_BENGALI */
    PACK ('B','o','p','o'), /* N_UNICODE_SCRIPT_BOPOMOFO */
    PACK ('C','h','e','r'), /* N_UNICODE_SCRIPT_CHEROKEE */
    PACK ('C','o','p','t'), /* N_UNICODE_SCRIPT_COPTIC */
    PACK ('C','y','r','l'), /* N_UNICODE_SCRIPT_CYRILLIC */
    PACK ('D','s','r','t'), /* N_UNICODE_SCRIPT_DESERET */
    PACK ('D','e','v','a'), /* N_UNICODE_SCRIPT_DEVANAGARI */
    PACK ('E','t','h','i'), /* N_UNICODE_SCRIPT_ETHIOPIC */
    PACK ('G','e','o','r'), /* N_UNICODE_SCRIPT_GEORGIAN */
    PACK ('G','o','t','h'), /* N_UNICODE_SCRIPT_GOTHIC */
    PACK ('G','r','e','k'), /* N_UNICODE_SCRIPT_GREEK */
    PACK ('G','u','j','r'), /* N_UNICODE_SCRIPT_GUJARATI */
    PACK ('G','u','r','u'), /* N_UNICODE_SCRIPT_GURMUKHI */
    PACK ('H','a','n','i'), /* N_UNICODE_SCRIPT_HAN */
    PACK ('H','a','n','g'), /* N_UNICODE_SCRIPT_HANGUL */
    PACK ('H','e','b','r'), /* N_UNICODE_SCRIPT_HEBREW */
    PACK ('H','i','r','a'), /* N_UNICODE_SCRIPT_HIRAGANA */
    PACK ('K','n','d','a'), /* N_UNICODE_SCRIPT_KANNADA */
    PACK ('K','a','n','a'), /* N_UNICODE_SCRIPT_KATAKANA */
    PACK ('K','h','m','r'), /* N_UNICODE_SCRIPT_KHMER */
    PACK ('L','a','o','o'), /* N_UNICODE_SCRIPT_LAO */
    PACK ('L','a','t','n'), /* N_UNICODE_SCRIPT_LATIN */
    PACK ('M','l','y','m'), /* N_UNICODE_SCRIPT_MALAYALAM */
    PACK ('M','o','n','g'), /* N_UNICODE_SCRIPT_MONGOLIAN */
    PACK ('M','y','m','r'), /* N_UNICODE_SCRIPT_MYANMAR */
    PACK ('O','g','a','m'), /* N_UNICODE_SCRIPT_OGHAM */
    PACK ('I','t','a','l'), /* N_UNICODE_SCRIPT_OLD_ITALIC */
    PACK ('O','r','y','a'), /* N_UNICODE_SCRIPT_ORIYA */
    PACK ('R','u','n','r'), /* N_UNICODE_SCRIPT_RUNIC */
    PACK ('S','i','n','h'), /* N_UNICODE_SCRIPT_SINHALA */
    PACK ('S','y','r','c'), /* N_UNICODE_SCRIPT_SYRIAC */
    PACK ('T','a','m','l'), /* N_UNICODE_SCRIPT_TAMIL */
    PACK ('T','e','l','u'), /* N_UNICODE_SCRIPT_TELUGU */
    PACK ('T','h','a','a'), /* N_UNICODE_SCRIPT_THAANA */
    PACK ('T','h','a','i'), /* N_UNICODE_SCRIPT_THAI */
    PACK ('T','i','b','t'), /* N_UNICODE_SCRIPT_TIBETAN */
    PACK ('C','a','n','s'), /* N_UNICODE_SCRIPT_CANADIAN_ABORIGINAL */
    PACK ('Y','i','i','i'), /* N_UNICODE_SCRIPT_YI */
    PACK ('T','g','l','g'), /* N_UNICODE_SCRIPT_TAGALOG */
    PACK ('H','a','n','o'), /* N_UNICODE_SCRIPT_HANUNOO */
    PACK ('B','u','h','d'), /* N_UNICODE_SCRIPT_BUHID */
    PACK ('T','a','g','b'), /* N_UNICODE_SCRIPT_TAGBANWA */

  /* Unicode-4.0 additions */
    PACK ('B','r','a','i'), /* N_UNICODE_SCRIPT_BRAILLE */
    PACK ('C','p','r','t'), /* N_UNICODE_SCRIPT_CYPRIOT */
    PACK ('L','i','m','b'), /* N_UNICODE_SCRIPT_LIMBU */
    PACK ('O','s','m','a'), /* N_UNICODE_SCRIPT_OSMANYA */
    PACK ('S','h','a','w'), /* N_UNICODE_SCRIPT_SHAVIAN */
    PACK ('L','i','n','b'), /* N_UNICODE_SCRIPT_LINEAR_B */
    PACK ('T','a','l','e'), /* N_UNICODE_SCRIPT_TAI_LE */
    PACK ('U','g','a','r'), /* N_UNICODE_SCRIPT_UGARITIC */

  /* Unicode-4.1 additions */
    PACK ('T','a','l','u'), /* N_UNICODE_SCRIPT_NEW_TAI_LUE */
    PACK ('B','u','g','i'), /* N_UNICODE_SCRIPT_BUGINESE */
    PACK ('G','l','a','g'), /* N_UNICODE_SCRIPT_GLAGOLITIC */
    PACK ('T','f','n','g'), /* N_UNICODE_SCRIPT_TIFINAGH */
    PACK ('S','y','l','o'), /* N_UNICODE_SCRIPT_SYLOTI_NAGRI */
    PACK ('X','p','e','o'), /* N_UNICODE_SCRIPT_OLD_PERSIAN */
    PACK ('K','h','a','r'), /* N_UNICODE_SCRIPT_KHAROSHTHI */

  /* Unicode-5.0 additions */
    PACK ('Z','z','z','z'), /* N_UNICODE_SCRIPT_UNKNOWN */
    PACK ('B','a','l','i'), /* N_UNICODE_SCRIPT_BALINESE */
    PACK ('X','s','u','x'), /* N_UNICODE_SCRIPT_CUNEIFORM */
    PACK ('P','h','n','x'), /* N_UNICODE_SCRIPT_PHOENICIAN */
    PACK ('P','h','a','g'), /* N_UNICODE_SCRIPT_PHAGS_PA */
    PACK ('N','k','o','o'), /* N_UNICODE_SCRIPT_NKO */

  /* Unicode-5.1 additions */
    PACK ('K','a','l','i'), /* N_UNICODE_SCRIPT_KAYAH_LI */
    PACK ('L','e','p','c'), /* N_UNICODE_SCRIPT_LEPCHA */
    PACK ('R','j','n','g'), /* N_UNICODE_SCRIPT_REJANG */
    PACK ('S','u','n','d'), /* N_UNICODE_SCRIPT_SUNDANESE */
    PACK ('S','a','u','r'), /* N_UNICODE_SCRIPT_SAURASHTRA */
    PACK ('C','h','a','m'), /* N_UNICODE_SCRIPT_CHAM */
    PACK ('O','l','c','k'), /* N_UNICODE_SCRIPT_OL_CHIKI */
    PACK ('V','a','i','i'), /* N_UNICODE_SCRIPT_VAI */
    PACK ('C','a','r','i'), /* N_UNICODE_SCRIPT_CARIAN */
    PACK ('L','y','c','i'), /* N_UNICODE_SCRIPT_LYCIAN */
    PACK ('L','y','d','i'), /* N_UNICODE_SCRIPT_LYDIAN */

  /* Unicode-5.2 additions */
    PACK ('A','v','s','t'), /* N_UNICODE_SCRIPT_AVESTAN */
    PACK ('B','a','m','u'), /* N_UNICODE_SCRIPT_BAMUM */
    PACK ('E','g','y','p'), /* N_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS */
    PACK ('A','r','m','i'), /* N_UNICODE_SCRIPT_IMPERIAL_ARAMAIC */
    PACK ('P','h','l','i'), /* N_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI */
    PACK ('P','r','t','i'), /* N_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN */
    PACK ('J','a','v','a'), /* N_UNICODE_SCRIPT_JAVANESE */
    PACK ('K','t','h','i'), /* N_UNICODE_SCRIPT_KAITHI */
    PACK ('L','i','s','u'), /* N_UNICODE_SCRIPT_LISU */
    PACK ('M','t','e','i'), /* N_UNICODE_SCRIPT_MEETEI_MAYEK */
    PACK ('S','a','r','b'), /* N_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN */
    PACK ('O','r','k','h'), /* N_UNICODE_SCRIPT_OLD_TURKIC */
    PACK ('S','a','m','r'), /* N_UNICODE_SCRIPT_SAMARITAN */
    PACK ('L','a','n','a'), /* N_UNICODE_SCRIPT_TAI_THAM */
    PACK ('T','a','v','t'), /* N_UNICODE_SCRIPT_TAI_VIET */

  /* Unicode-6.0 additions */
    PACK ('B','a','t','k'), /* N_UNICODE_SCRIPT_BATAK */
    PACK ('B','r','a','h'), /* N_UNICODE_SCRIPT_BRAHMI */
    PACK ('M','a','n','d'), /* N_UNICODE_SCRIPT_MANDAIC */

  /* Unicode-6.1 additions */
    PACK ('C','a','k','m'), /* N_UNICODE_SCRIPT_CHAKMA */
    PACK ('M','e','r','c'), /* N_UNICODE_SCRIPT_MEROITIC_CURSIVE */
    PACK ('M','e','r','o'), /* N_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS */
    PACK ('P','l','r','d'), /* N_UNICODE_SCRIPT_MIAO */
    PACK ('S','h','r','d'), /* N_UNICODE_SCRIPT_SHARADA */
    PACK ('S','o','r','a'), /* N_UNICODE_SCRIPT_SORA_SOMPENG */
    PACK ('T','a','k','r'), /* N_UNICODE_SCRIPT_TAKRI */

  /* Unicode 7.0 additions */
    PACK ('B','a','s','s'), /* N_UNICODE_SCRIPT_BASSA_VAH */
    PACK ('A','g','h','b'), /* N_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN */
    PACK ('D','u','p','l'), /* N_UNICODE_SCRIPT_DUPLOYAN */
    PACK ('E','l','b','a'), /* N_UNICODE_SCRIPT_ELBASAN */
    PACK ('G','r','a','n'), /* N_UNICODE_SCRIPT_GRANTHA */
    PACK ('K','h','o','j'), /* N_UNICODE_SCRIPT_KHOJKI*/
    PACK ('S','i','n','d'), /* N_UNICODE_SCRIPT_KHUDAWADI */
    PACK ('L','i','n','a'), /* N_UNICODE_SCRIPT_LINEAR_A */
    PACK ('M','a','h','j'), /* N_UNICODE_SCRIPT_MAHAJANI */
    PACK ('M','a','n','i'), /* N_UNICODE_SCRIPT_MANICHAEAN */
    PACK ('M','e','n','d'), /* N_UNICODE_SCRIPT_MENDE_KIKAKUI */
    PACK ('M','o','d','i'), /* N_UNICODE_SCRIPT_MODI */
    PACK ('M','r','o','o'), /* N_UNICODE_SCRIPT_MRO */
    PACK ('N','b','a','t'), /* N_UNICODE_SCRIPT_NABATAEAN */
    PACK ('N','a','r','b'), /* N_UNICODE_SCRIPT_OLD_NORTH_ARABIAN */
    PACK ('P','e','r','m'), /* N_UNICODE_SCRIPT_OLD_PERMIC */
    PACK ('H','m','n','g'), /* N_UNICODE_SCRIPT_PAHAWH_HMONG */
    PACK ('P','a','l','m'), /* N_UNICODE_SCRIPT_PALMYRENE */
    PACK ('P','a','u','c'), /* N_UNICODE_SCRIPT_PAU_CIN_HAU */
    PACK ('P','h','l','p'), /* N_UNICODE_SCRIPT_PSALTER_PAHLAVI */
    PACK ('S','i','d','d'), /* N_UNICODE_SCRIPT_SIDDHAM */
    PACK ('T','i','r','h'), /* N_UNICODE_SCRIPT_TIRHUTA */
    PACK ('W','a','r','a'), /* N_UNICODE_SCRIPT_WARANN_CITI */

  /* Unicode 8.0 additions */
    PACK ('A','h','o','m'), /* N_UNICODE_SCRIPT_AHOM */
    PACK ('H','l','u','w'), /* N_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS */
    PACK ('H','a','t','r'), /* N_UNICODE_SCRIPT_HATRAN */
    PACK ('M','u','l','t'), /* N_UNICODE_SCRIPT_MULTANI */
    PACK ('H','u','n','g'), /* N_UNICODE_SCRIPT_OLD_HUNGARIAN */
    PACK ('S','g','n','w'), /* N_UNICODE_SCRIPT_SIGNWRITING */

  /* Unicode 9.0 additions */
    PACK ('A','d','l','m'), /* N_UNICODE_SCRIPT_ADLAM */
    PACK ('B','h','k','s'), /* N_UNICODE_SCRIPT_BHAIKSUKI */
    PACK ('M','a','r','c'), /* N_UNICODE_SCRIPT_MARCHEN */
    PACK ('N','e','w','a'), /* N_UNICODE_SCRIPT_NEWA */
    PACK ('O','s','g','e'), /* N_UNICODE_SCRIPT_OSAGE */
    PACK ('T','a','n','g'), /* N_UNICODE_SCRIPT_TANGUT */

  /* Unicode 10.0 additions */
    PACK ('G','o','n','m'), /* N_UNICODE_SCRIPT_MASARAM_GONDI */
    PACK ('N','s','h','u'), /* N_UNICODE_SCRIPT_NUSHU */
    PACK ('S','o','y','o'), /* N_UNICODE_SCRIPT_SOYOMBO */
    PACK ('Z','a','n','b'), /* N_UNICODE_SCRIPT_ZANABAZAR_SQUARE */

  /* Unicode 11.0 additions */
    PACK ('D','o','g','r'), /* N_UNICODE_SCRIPT_DOGRA */
    PACK ('G','o','n','g'), /* N_UNICODE_SCRIPT_GUNJALA_GONDI */
    PACK ('R','o','h','g'), /* N_UNICODE_SCRIPT_HANIFI_ROHINGYA */
    PACK ('M','a','k','a'), /* N_UNICODE_SCRIPT_MAKASAR */
    PACK ('M','e','d','f'), /* N_UNICODE_SCRIPT_MEDEFAIDRIN */
    PACK ('S','o','g','o'), /* N_UNICODE_SCRIPT_OLD_SOGDIAN */
    PACK ('S','o','g','d'), /* N_UNICODE_SCRIPT_SOGDIAN */

  /* Unicode 12.0 additions */
    PACK ('E','l','y','m'), /* N_UNICODE_SCRIPT_ELYMAIC */
    PACK ('N','a','n','d'), /* N_UNICODE_SCRIPT_NANDINAGARI */
    PACK ('H','m','n','p'), /* N_UNICODE_SCRIPT_NYIAKENN_PUACHUE_HMONG */
    PACK ('W','c','h','o'), /* N_UNICODE_SCRIPT_WANCHO */

  /* Unicode 13.0 additions */
    PACK ('C', 'h', 'r', 's'), /* N_UNICODE_SCRIPT_CHORASMIAN */
    PACK ('D', 'i', 'a', 'k'), /* N_UNICODE_SCRIPT_DIVES_AKURU */
    PACK ('K', 'i', 't', 's'), /* N_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT */
    PACK ('Y', 'e', 'z', 'i'), /* N_UNICODE_SCRIPT_YEZIDI */
#undef PACK
};

/**
 * n_unicode_script_to_iso15924:
 * @script: a Unicode script
 *
 * Looks up the ISO 15924 code for @script.  ISO 15924 assigns four-letter
 * codes to scripts.  For example, the code for Arabic is 'Arab'.  The
 * four letter codes are encoded as a @nuint32 by this function in a
 * big-endian fashion.  That is, the code returned for Arabic is
 * 0x41726162 (0x41 is ASCII code for 'A', 0x72 is ASCII code for 'r', etc).
 *
 * See
 * [Codes for the representation of names of scripts](http://unicode.org/iso15924/codelists.html)
 * for details.
 *
 * Returns: the ISO 15924 code for @script, encoded as an integer,
 *   of zero if @script is %N_UNICODE_SCRIPT_INVALID_CODE or
 *   ISO 15924 code 'Zzzz' (script code for UNKNOWN) if @script is not understood.
 *
 * Since: 2.30
 */
nuint32 n_unicode_script_to_iso15924 (NUnicodeScript script)
{
  if (N_UNLIKELY (script == N_UNICODE_SCRIPT_INVALID_CODE))
    return 0;

  if (N_UNLIKELY (script < 0 || script >= (int) N_N_ELEMENTS (iso15924_tags)))
    return 0x5A7A7A7A;

  return iso15924_tags[script];
}

/**
 * n_unicode_script_from_iso15924:
 * @iso15924: a Unicode script
 *
 * Looks up the Unicode script for @iso15924.  ISO 15924 assigns four-letter
 * codes to scripts.  For example, the code for Arabic is 'Arab'.
 * This function accepts four letter codes encoded as a @nuint32 in a
 * big-endian fashion.  That is, the code expected for Arabic is
 * 0x41726162 (0x41 is ASCII code for 'A', 0x72 is ASCII code for 'r', etc).
 *
 * See
 * [Codes for the representation of names of scripts](http://unicode.org/iso15924/codelists.html)
 * for details.
 *
 * Returns: the Unicode script for @iso15924, or
 *   of %N_UNICODE_SCRIPT_INVALID_CODE if @iso15924 is zero and
 *   %N_UNICODE_SCRIPT_UNKNOWN if @iso15924 is unknown.
 *
 * Since: 2.30
 */
NUnicodeScript n_unicode_script_from_iso15924 (nuint32 iso15924)
{
  unsigned int i;

   if (!iso15924)
     return N_UNICODE_SCRIPT_INVALID_CODE;

  for (i = 0; i < N_N_ELEMENTS (iso15924_tags); i++)
    if (iso15924_tags[i] == iso15924)
      return (NUnicodeScript) i;

  return N_UNICODE_SCRIPT_UNKNOWN;
}
