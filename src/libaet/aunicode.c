#include <stdlib.h>
#include <langinfo.h>
#include <string.h>
#include <locale.h>


#include "aunicode.h"
#include "amem.h"
#include "alog.h"
#include "aunichartables.h"
#include "aunidecomp.h"
#include "aconvert.h"


#define SBase 0xAC00
#define LBase 0x1100
#define VBase 0x1161
#define TBase 0x11A7
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)

#define UTF8_COMPUTE(Char, Mask, Len)					      \
  if (Char < 128)							      \
    {									      \
      Len = 1;								      \
      Mask = 0x7f;							      \
    }									      \
  else if ((Char & 0xe0) == 0xc0)					      \
    {									      \
      Len = 2;								      \
      Mask = 0x1f;							      \
    }									      \
  else if ((Char & 0xf0) == 0xe0)					      \
    {									      \
      Len = 3;								      \
      Mask = 0x0f;							      \
    }									      \
  else if ((Char & 0xf8) == 0xf0)					      \
    {									      \
      Len = 4;								      \
      Mask = 0x07;							      \
    }									      \
  else if ((Char & 0xfc) == 0xf8)					      \
    {									      \
      Len = 5;								      \
      Mask = 0x03;							      \
    }									      \
  else if ((Char & 0xfe) == 0xfc)					      \
    {									      \
      Len = 6;								      \
      Mask = 0x01;							      \
    }									      \
  else									      \
    Len = -1;

#define UTF8_LENGTH(Char)              \
  ((Char) < 0x80 ? 1 :                 \
   ((Char) < 0x800 ? 2 :               \
    ((Char) < 0x10000 ? 3 :            \
     ((Char) < 0x200000 ? 4 :          \
      ((Char) < 0x4000000 ? 5 : 6)))))
   

#define UTF8_GET(Result, Chars, Count, Mask, Len)			      \
  (Result) = (Chars)[0] & (Mask);					      \
  for ((Count) = 1; (Count) < (Len); ++(Count))				      \
    {									      \
      if (((Chars)[(Count)] & 0xc0) != 0x80)				      \
	{								      \
	  (Result) = -1;						      \
	  break;							      \
	}								      \
      (Result) <<= 6;							      \
      (Result) |= ((Chars)[(Count)] & 0x3f);				      \
    }
    
/*
 * Check whether a Unicode (5.2) char is in a valid range.
 *
 * The first check comes from the Unicode guarantee to never encode
 * a point above 0x0010ffff, since UTF-16 couldn't represent it.
 * 
 * The second check covers surrogate pairs (category Cs).
 *
 * @param Char the character
 */
#define UNICODE_VALID(Char)                   \
    ((Char) < 0x110000 &&                     \
     (((Char) & 0xFFFFF800) != 0xD800))

    
static const achar utf8_skip_data[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

const achar * const a_utf8_skip = utf8_skip_data;


#define VALIDATE_BYTE(mask, expect)                      \
  A_STMT_START {                                         \
    if (A_UNLIKELY((*(auchar *)p & (mask)) != (expect))) \
      goto error;                                        \
  } A_STMT_END


static const achar *fast_validate (const char *str)

{
  const achar *p;

  for (p = str; *p; p++)
    {
      if (*(auchar *)p < 128)
	/* done */;
      else
	{
	  const achar *last;

	  last = p;
	  if (*(auchar *)p < 0xe0) /* 110xxxxx */
	    {
	      if (A_UNLIKELY (*(auchar *)p < 0xc2))
		goto error;
	    }
	  else
	    {
	      if (*(auchar *)p < 0xf0) /* 1110xxxx */
		{
		  switch (*(auchar *)p++ & 0x0f)
		    {
		    case 0:
		      VALIDATE_BYTE(0xe0, 0xa0); /* 0xa0 ... 0xbf */
		      break;
		    case 0x0d:
		      VALIDATE_BYTE(0xe0, 0x80); /* 0x80 ... 0x9f */
		      break;
		    default:
		      VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		    }
		}
	      else if (*(auchar *)p < 0xf5) /* 11110xxx excluding out-of-range */
		{
		  switch (*(auchar *)p++ & 0x07)
		    {
		    case 0:
		      VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		      if (A_UNLIKELY((*(auchar *)p & 0x30) == 0))
			goto error;
		      break;
		    case 4:
		      VALIDATE_BYTE(0xf0, 0x80); /* 0x80 ... 0x8f */
		      break;
		    default:
		      VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		    }
		  p++;
		  VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		}
	      else
		goto error;
	    }

	  p++;
	  VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */

	  continue;

	error:
	  return last;
	}
    }

  return p;
}

static const achar *fast_validate_len (const char *str,assize max_len)

{
  const achar *p;
  if(max_len<0){
	  printf("fast_validate_len error:max_len:%d",max_len);
	  exit(-1);
  }

  for (p = str; ((p - str) < max_len) && *p; p++)
    {
      if (*(auchar *)p < 128)
	/* done */;
      else
	{
	  const achar *last;

	  last = p;
	  if (*(auchar *)p < 0xe0) /* 110xxxxx */
	    {
	      if (A_UNLIKELY (max_len - (p - str) < 2))
		goto error;

	      if (A_UNLIKELY (*(auchar *)p < 0xc2))
		goto error;
	    }
	  else
	    {
	      if (*(auchar *)p < 0xf0) /* 1110xxxx */
		{
		  if (A_UNLIKELY (max_len - (p - str) < 3))
		    goto error;

		  switch (*(auchar *)p++ & 0x0f)
		    {
		    case 0:
		      VALIDATE_BYTE(0xe0, 0xa0); /* 0xa0 ... 0xbf */
		      break;
		    case 0x0d:
		      VALIDATE_BYTE(0xe0, 0x80); /* 0x80 ... 0x9f */
		      break;
		    default:
		      VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		    }
		}
	      else if (*(auchar *)p < 0xf5) /* 11110xxx excluding out-of-range */
		{
		  if (A_UNLIKELY (max_len - (p - str) < 4))
		    goto error;

		  switch (*(auchar *)p++ & 0x07)
		    {
		    case 0:
		      VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		      if (A_UNLIKELY((*(auchar *)p & 0x30) == 0))
			goto error;
		      break;
		    case 4:
		      VALIDATE_BYTE(0xf0, 0x80); /* 0x80 ... 0x8f */
		      break;
		    default:
		      VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		    }
		  p++;
		  VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
		}
	      else
		goto error;
	    }

	  p++;
	  VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */

	  continue;

	error:
	  return last;
	}
    }

  return p;
}

aboolean a_utf8_validate_len (const char *str,asize max_len,const achar **end)
{
  const achar *p;
  p = fast_validate_len (str, max_len);
  if (end)
    *end = p;
  if (p != str + max_len)
    return FALSE;
  else
    return TRUE;
}

aboolean a_utf8_validate (const char *str,assize max_len,const achar **end)
{
  const achar *p;

  if (max_len >= 0)
    return a_utf8_validate_len (str, max_len, end);
  p = fast_validate (str);
  if (end)
    *end = p;
  if (*p != '\0')
    return FALSE;
  else
    return TRUE;
}

static inline aunichar a_utf8_get_char_extended (const achar *p,assize max_len)
{
  auint i, len;
  aunichar min_code;
  aunichar wc = (auchar) *p;
  const aunichar partial_sequence = (aunichar) -2;
  const aunichar malformed_sequence = (aunichar) -1;

  if (wc < 0x80)
    {
      return wc;
    }
  else if (A_UNLIKELY (wc < 0xc0))
    {
      return malformed_sequence;
    }
  else if (wc < 0xe0)
    {
      len = 2;
      wc &= 0x1f;
      min_code = 1 << 7;
    }
  else if (wc < 0xf0)
    {
      len = 3;
      wc &= 0x0f;
      min_code = 1 << 11;
    }
  else if (wc < 0xf8)
    {
      len = 4;
      wc &= 0x07;
      min_code = 1 << 16;
    }
  else if (wc < 0xfc)
    {
      len = 5;
      wc &= 0x03;
      min_code = 1 << 21;
    }
  else if (wc < 0xfe)
    {
      len = 6;
      wc &= 0x01;
      min_code = 1 << 26;
    }
  else
    {
      return malformed_sequence;
    }

  if (A_UNLIKELY (max_len >= 0 && len > max_len))
    {
      for (i = 1; i < max_len; i++)
	{
	  if ((((auchar *)p)[i] & 0xc0) != 0x80)
	    return malformed_sequence;
	}
      return partial_sequence;
    }

  for (i = 1; i < len; ++i)
    {
      aunichar ch = ((auchar *)p)[i];

      if (A_UNLIKELY ((ch & 0xc0) != 0x80))
	{
	  if (ch)
	    return malformed_sequence;
	  else
	    return partial_sequence;
	}

      wc <<= 6;
      wc |= (ch & 0x3f);
    }

  if (A_UNLIKELY (wc < min_code))
    return malformed_sequence;

  return wc;
}

aunichar a_utf8_get_char_validated (const achar *p,assize  max_len)
{
  aunichar result;

  if (max_len == 0)
    return (aunichar)-2;

  result = a_utf8_get_char_extended (p, max_len);

  /* Disallow codepoint U+0000 as it’s a nul byte,
   * and all string handling in GLib is nul-terminated */
  if (result == 0 && max_len > 0)
    return (aunichar) -2;

  if (result & 0x80000000)
    return result;
  else if (!UNICODE_VALID (result))
    return (aunichar)-1;
  else
    return result;
}

aunichar a_utf8_get_char (const achar *p)
{
  int i, mask = 0, len;
  aunichar result;
  unsigned char c = (unsigned char) *p;

  UTF8_COMPUTE (c, mask, len);
  if (len == -1)
    return (aunichar)-1;
  UTF8_GET (result, p, i, mask, len);

  return result;
}

achar *a_utf8_offset_to_pointer  (const achar *str,along offset)
{
  const achar *s = str;

  if (offset > 0)
    while (offset--)
      s = a_utf8_next_char (s);
  else
    {
      const char *s1;

      /* This nice technique for fast backwards stepping
       * through a UTF-8 string was dubbed "stutter stepping"
       * by its inventor, Larry Ewing.
       */
      while (offset)
	{
	  s1 = s;
	  s += offset;
	  while ((*s & 0xc0) == 0x80)
	    s--;

	  offset += a_utf8_pointer_to_offset (s, s1);
	}
    }

  return (achar *)s;
}

along  a_utf8_pointer_to_offset (const achar *str, const achar *pos)
{
  const achar *s = str;
  along offset = 0;

  if (pos < str)
    offset = - a_utf8_pointer_to_offset (pos, str);
  else
    while (s < pos)
      {
	s = a_utf8_next_char (s);
	offset++;
      }

  return offset;
}

along a_utf8_strlen (const achar *p,assize  max)
{
  along len = 0;
  const achar *start = p;
  a_return_val_if_fail (p != NULL || max == 0, 0);
  if (max < 0)
    {
      while (*p)
        {
          p = a_utf8_next_char (p);
          ++len;
        }
    }
  else
    {
      if (max == 0 || !*p)
        return 0;

      p = a_utf8_next_char (p);

      while (p - start < max && *p)
        {
          ++len;
          p = a_utf8_next_char (p);
        }

      /* only do the last len increment if we got a complete
       * char (don't count partial chars)
       */
      if (p - start <= max)
        ++len;
    }

  return len;
}


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


#define A_UNICHAR_FULLWIDTH_A 0xff21
#define A_UNICHAR_FULLWIDTH_I 0xff29
#define A_UNICHAR_FULLWIDTH_J 0xff2a
#define A_UNICHAR_FULLWIDTH_F 0xff26
#define A_UNICHAR_FULLWIDTH_a 0xff41
#define A_UNICHAR_FULLWIDTH_f 0xff46
//
#define ATTR_TABLE(Page) (((Page) <= A_UNICODE_LAST_PAGE_PART1) \
                          ? attr_table_part1[Page] \
                          : attr_table_part2[(Page) - 0xe00])
//
#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == A_UNICODE_MAX_TABLE_INDEX) ? 0 : (attr_data[ATTR_TABLE(Page)][Char]))
//
#define TTYPE_PART1(Page, Char) \
  ((type_table_part1[Page] >= A_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part1[Page] - A_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part1[Page]][Char]))
//
#define TTYPE_PART2(Page, Char) \
  ((type_table_part2[Page] >= A_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part2[Page] - A_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part2[Page]][Char]))
//
#define TYPE(Char) \
  (((Char) <= A_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= A_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : A_UNICODE_UNASSIGNED))

#define IS(Type, Class)	(((auint)1 << (Type)) & (Class))
#define OR(Type, Rest)	(((auint)1 << (Type)) | (Rest))


#define ISALPHA(Type)	IS ((Type),				\
			    OR (A_UNICODE_LOWERCASE_LETTER,	\
			    OR (A_UNICODE_UPPERCASE_LETTER,	\
			    OR (A_UNICODE_TITLECASE_LETTER,	\
			    OR (A_UNICODE_MODIFIER_LETTER,	\
			    OR (A_UNICODE_OTHER_LETTER,		0))))))

#define ISALDIGIT(Type)	IS ((Type),				\
			    OR (A_UNICODE_DECIMAL_NUMBER,	\
			    OR (A_UNICODE_LETTER_NUMBER,	\
			    OR (A_UNICODE_OTHER_NUMBER,		\
			    OR (A_UNICODE_LOWERCASE_LETTER,	\
			    OR (A_UNICODE_UPPERCASE_LETTER,	\
			    OR (A_UNICODE_TITLECASE_LETTER,	\
			    OR (A_UNICODE_MODIFIER_LETTER,	\
			    OR (A_UNICODE_OTHER_LETTER,		0)))))))))

#define ISMARK(Type)	IS ((Type),				\
			    OR (A_UNICODE_NON_SPACING_MARK,	\
			    OR (A_UNICODE_SPACING_MARK,	\
			    OR (A_UNICODE_ENCLOSING_MARK,	0))))

#define ISZEROWIDTHTYPE(Type)	IS ((Type),			\
			    OR (A_UNICODE_NON_SPACING_MARK,	\
			    OR (A_UNICODE_ENCLOSING_MARK,	\
			    OR (A_UNICODE_FORMAT,		0))))

static aint output_marks (const char **p_inout,char *out_buffer,aboolean remove_dot)
{
  const char *p = *p_inout;
  aint len = 0;

  while (*p)
    {
      aunichar c = a_utf8_get_char (p);

      if (ISMARK (TYPE (c)))
	{
	  if (!remove_dot || c != 0x307 /* COMBINING DOT ABOVE */)
	    len += a_unichar_to_utf8 (c, out_buffer ? out_buffer + len : NULL);
	  p = a_utf8_next_char (p);
	}
      else
	break;
    }

  *p_inout = p;
  return len;
}

static void decompose_hangul (aunichar s,aunichar *r,asize *result_len)
{
  aint SIndex = s - SBase;
  aint TIndex = SIndex % TCount;

  if (r)
    {
      r[0] = LBase + SIndex / NCount;
      r[1] = VBase + (SIndex % NCount) / TCount;
    }

  if (TIndex)
    {
      if (r)
	r[2] = TBase + TIndex;
      *result_len = 3;
    }
  else
    *result_len = 2;
}

static const achar *find_decomposition (aunichar ch,aboolean compat)
{
  int start = 0;
  int end = A_N_ELEMENTS (decomp_table);

  if (ch >= decomp_table[start].ch &&
      ch <= decomp_table[end - 1].ch)
    {
      while (TRUE)
	{
	  int half = (start + end) / 2;
	  if (ch == decomp_table[half].ch)
	    {
	      int offset;

	      if (compat)
		{
		  offset = decomp_table[half].compat_offset;
		  if (offset == A_UNICODE_NOT_PRESENT_OFFSET)
		    offset = decomp_table[half].canon_offset;
		}
	      else
		{
		  offset = decomp_table[half].canon_offset;
		  if (offset == A_UNICODE_NOT_PRESENT_OFFSET)
		    return NULL;
		}

	      return &(decomp_expansion_string[offset]);
	    }
	  else if (half == start)
	    break;
	  else if (ch > decomp_table[half].ch)
	    start = half;
	  else
	    end = half;
	}
    }

  return NULL;
}

static aint output_special_case (achar *out_buffer,int offset,int type,int which)
{
  const achar *p = special_case_table + offset;
  aint len;

  if (type != A_UNICODE_TITLECASE_LETTER)
    p = a_utf8_next_char (p);

  if (which == 1)
    p += strlen (p) + 1;

  len = strlen (p);
  if (out_buffer)
    memcpy (out_buffer, p, len);

  return len;
}

static asize real_toupper (const achar *str,assize max_len,achar *out_buffer,LocaleType   locale_type)
{
  const achar *p = str;
  const char *last = NULL;
  asize len = 0;
  aboolean last_was_i = FALSE;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      aunichar c = a_utf8_get_char (p);
      int t = TYPE (c);
      aunichar val;

      last = p;
      p = a_utf8_next_char (p);

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
		  asize decomp_len, i;
		  aunichar decomp[A_UNICHAR_MAX_DECOMPOSITION_LENGTH];

		  decomp_len = a_unichar_fully_decompose (c, FALSE, decomp, A_N_ELEMENTS (decomp));
		  for (i=0; i < decomp_len; i++)
		    {
		      if (decomp[i] != 0x307 /* COMBINING DOT ABOVE */)
			len += a_unichar_to_utf8 (a_unichar_toupper (decomp[i]), out_buffer ? out_buffer + len : NULL);
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
	  len += a_unichar_to_utf8 (0x130, out_buffer ? out_buffer + len : NULL);
	}
      else if (c == 0x0345)	/* COMBINING GREEK YPOGEGRAMMENI */
	{
	  /* Nasty, need to move it after other combining marks .. this would go away if
	   * we normalized first.
	   */
	  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, FALSE);

	  /* And output as GREEK CAPITAL LETTER IOTA */
	  len += a_unichar_to_utf8 (0x399, out_buffer ? out_buffer + len : NULL);
	}
      else if (IS (t,
		   OR (A_UNICODE_LOWERCASE_LETTER,
		   OR (A_UNICODE_TITLECASE_LETTER,
		  0))))
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t,
					  t == A_UNICODE_LOWERCASE_LETTER ? 0 : 1);
	    }
	  else
	    {
	      if (t == A_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < A_N_ELEMENTS (title_table); ++i)
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
	      len += a_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  asize char_len = a_utf8_skip[*(auchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

achar *a_utf8_strup (const achar *str,assize len)
{
  asize result_len;
  LocaleType locale_type;
  achar *result;

  a_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();

  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_toupper (str, len, NULL, locale_type);
  result = a_malloc (result_len + 1);
  real_toupper (str, len, result, locale_type);
  result[result_len] = '\0';
  return result;
}




asize a_unichar_fully_decompose (aunichar ch,aboolean  compat,aunichar *result,asize     result_len)
{
  const achar *decomp;
  const achar *p;

  /* Hangul syllable */
  if (ch >= SBase && ch < SBase + SCount)
    {
      asize len, i;
      aunichar buffer[3];
      decompose_hangul (ch, result ? buffer : NULL, &len);
      if (result)
        for (i = 0; i < len && i < result_len; i++)
	  result[i] = buffer[i];
      return len;
    }
  else if ((decomp = find_decomposition (ch, compat)) != NULL)
    {
      /* Found it.  */
      asize len, i;

      len = a_utf8_strlen (decomp, -1);

      for (p = decomp, i = 0; i < len && i < result_len; p = a_utf8_next_char (p), i++)
        result[i] = a_utf8_get_char (p);

      return len;
    }

  /* Does not decompose */
  if (result && result_len >= 1)
    *result = ch;
  return 1;
}

int a_unichar_to_utf8 (aunichar c,achar   *outbuf)
{
  /* If this gets modified, also update the copy in n_string_insert_unichar() */
  auint len = 0;
  int first;
  int i;

  if (c < 0x80)
    {
      first = 0;
      len = 1;
    }
  else if (c < 0x800)
    {
      first = 0xc0;
      len = 2;
    }
  else if (c < 0x10000)
    {
      first = 0xe0;
      len = 3;
    }
   else if (c < 0x200000)
    {
      first = 0xf0;
      len = 4;
    }
  else if (c < 0x4000000)
    {
      first = 0xf8;
      len = 5;
    }
  else
    {
      first = 0xfc;
      len = 6;
    }

  if (outbuf)
    {
      for (i = len - 1; i > 0; --i)
	{
	  outbuf[i] = (c & 0x3f) | 0x80;
	  c >>= 6;
	}
      outbuf[0] = c | first;
    }

  return len;
}

aunichar a_unichar_toupper (aunichar c)
{
  int t = TYPE (c);
  if (t == A_UNICODE_LOWERCASE_LETTER)
    {
      aunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const achar *p = special_case_table + val - 0x1000000;
          val = a_utf8_get_char (p);
	}
      /* Some lowercase letters, e.g., U+000AA, FEMININE ORDINAL INDICATOR,
       * do not have an uppercase equivalent, in which case val will be
       * zero.
       */
      return val ? val : c;
    }
  else if (t == A_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < A_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][1] ? title_table[i][1] : c;
	}
    }
  return c;
}

static aboolean has_more_above (const achar *str)
{
  const achar *p = str;
  aint combininn_class;

  while (*p)
    {
      combininn_class = a_unichar_combining_class (a_utf8_get_char (p));
      if (combininn_class == 230)
        return TRUE;
      else if (combininn_class == 0)
        break;

      p = a_utf8_next_char (p);
    }

  return FALSE;
}

static asize real_tolower (const achar *str,assize max_len,achar *out_buffer,LocaleType   locale_type)
{
  const achar *p = str;
  const char *last = NULL;
  asize len = 0;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      aunichar c = a_utf8_get_char (p);
      int t = TYPE (c);
      aunichar val;

      last = p;
      p = a_utf8_next_char (p);

      if (locale_type == LOCALE_TURKIC && (c == 'I' ||
                                           c == A_UNICHAR_FULLWIDTH_I))
	{
          if (a_utf8_get_char (p) == 0x0307)
            {
              /* I + COMBINING DOT ABOVE => i (U+0069) */
              len += a_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL);
              p = a_utf8_next_char (p);
            }
          else
            {
              /* I => LATIN SMALL LETTER DOTLESS I */
              len += a_unichar_to_utf8 (0x131, out_buffer ? out_buffer + len : NULL);
            }
        }
      /* Introduce an explicit dot above when lowercasing capital I's and J's
       * whenever there are more accents above. [SpecialCasing.txt] */
      else if (locale_type == LOCALE_LITHUANIAN &&
               (c == 0x00cc || c == 0x00cd || c == 0x0128))
        {
          len += a_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL);
          len += a_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL);

          switch (c)
            {
            case 0x00cc:
              len += a_unichar_to_utf8 (0x0300, out_buffer ? out_buffer + len : NULL);
              break;
            case 0x00cd:
              len += a_unichar_to_utf8 (0x0301, out_buffer ? out_buffer + len : NULL);
              break;
            case 0x0128:
              len += a_unichar_to_utf8 (0x0303, out_buffer ? out_buffer + len : NULL);
              break;
            }
        }
      else if (locale_type == LOCALE_LITHUANIAN &&
               (c == 'I' || c == A_UNICHAR_FULLWIDTH_I ||
                c == 'J' || c == A_UNICHAR_FULLWIDTH_J || c == 0x012e) &&
               has_more_above (p))
        {
          len += a_unichar_to_utf8 (a_unichar_tolower (c), out_buffer ? out_buffer + len : NULL);
          len += a_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL);
        }
      else if (c == 0x03A3)	/* GREEK CAPITAL LETTER SIGMA */
	{
	  if ((max_len < 0 || p < str + max_len) && *p)
	    {
	      aunichar next_c = a_utf8_get_char (p);
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

	  len += a_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
	}
      else if (IS (t,
		   OR (A_UNICODE_UPPERCASE_LETTER,
		   OR (A_UNICODE_TITLECASE_LETTER,
		  0))))
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t, 0);
	    }
	  else
	    {
	      if (t == A_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < A_N_ELEMENTS (title_table); ++i)
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
	      len += a_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  asize char_len = a_utf8_skip[*(auchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

achar *a_utf8_strdown (const achar *str,assize len)
{
  asize result_len;
  LocaleType locale_type;
  achar *result;
  a_return_val_if_fail (str != NULL, NULL);
  locale_type = get_locale_type ();

  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_tolower (str, len, NULL, locale_type);
  result = a_malloc (result_len + 1);
  real_tolower (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

aunichar a_unichar_tolower (aunichar c)
{
  int t = TYPE (c);
  if (t == A_UNICODE_UPPERCASE_LETTER)
    {
      aunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const achar *p = special_case_table + val - 0x1000000;
	  return a_utf8_get_char (p);
	}
      else
	{
	  /* Not all uppercase letters are guaranteed to have a lowercase
	   * equivalent.  If this is the case, val will be zero. */
	  return val ? val : c;
	}
    }
  else if (t == A_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < A_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][2];
	}
    }
  return c;
}

#define CC_PART1(Page, Char) \
  ((combining_class_table_part1[Page] >= A_UNICODE_MAX_TABLE_INDEX) \
   ? (combining_class_table_part1[Page] - A_UNICODE_MAX_TABLE_INDEX) \
   : (cclass_data[combining_class_table_part1[Page]][Char]))

#define CC_PART2(Page, Char) \
  ((combining_class_table_part2[Page] >= A_UNICODE_MAX_TABLE_INDEX) \
   ? (combining_class_table_part2[Page] - A_UNICODE_MAX_TABLE_INDEX) \
   : (cclass_data[combining_class_table_part2[Page]][Char]))

#define COMBINING_CLASS(Char) \
  (((Char) <= A_UNICODE_LAST_CHAR_PART1) \
   ? CC_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= A_UNICODE_LAST_CHAR) \
      ? CC_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : 0))

aint a_unichar_combining_class (aunichar uc)
{
  return COMBINING_CLASS (uc);
}

static apointer try_malloc_n (asize n_blocks, asize n_block_bytes, AError **error)
{
    apointer ptr = a_try_malloc_n (n_blocks, n_block_bytes);
    if (ptr == NULL)
    	a_error_set(error, A_CONVERT_ERROR, A_CONVERT_ERROR_NO_MEMORY,"Failed to allocate memory");
    return ptr;
}


aunichar2 *a_utf8_to_utf16 (const achar *str,along  len,along  *items_read,along *items_written,AError  **error)
{
	  aunichar2 *result = NULL;
	  aint n16;
	  const achar *in;
	  aint i;

	  a_return_val_if_fail (str != NULL, NULL);

	  in = str;
	  n16 = 0;
	  while ((len < 0 || str + len - in > 0) && *in)
	    {
	      aunichar wc = a_utf8_get_char_extended (in, len < 0 ? 6 : str + len - in);
	      if (wc & 0x80000000)
		{
		  if (wc == (aunichar)-2)
		    {
		      if (items_read)
			      break;
		      else
		    	  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_PARTIAL_INPUT,
	                                     "Partial character sequence at end of input");
		    }
		  else
			  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
	                                 "Invalid byte sequence in conversion input");

		  goto err_out;
		}

	      if (wc < 0xd800)
		     n16 += 1;
	      else if (wc < 0xe000){
	    	  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
	                               "Invalid sequence in conversion input");

		  goto err_out;
		}
	      else if (wc < 0x10000)
		n16 += 1;
	      else if (wc < 0x110000)
		n16 += 2;
	      else{
	    	  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
	                               "Character out of range for UTF-16");

		  goto err_out;
		}

	      in = a_utf8_next_char (in);
	    }

	  result = try_malloc_n (n16 + 1, sizeof (aunichar2), error);
	  if (result == NULL)
	      goto err_out;

	  in = str;
	  for (i = 0; i < n16;)
	    {
	      aunichar wc = a_utf8_get_char (in);

	      if (wc < 0x10000)
		{
		  result[i++] = wc;
		}
	      else
		{
		  result[i++] = (wc - 0x10000) / 0x400 + 0xd800;
		  result[i++] = (wc - 0x10000) % 0x400 + 0xdc00;
		}

	      in = a_utf8_next_char (in);
	    }

	  result[i] = 0;

	  if (items_written)
	    *items_written = n16;

	 err_out:
	  if (items_read)
	    *items_read = in - str;

	  return result;
}

#define SURROGATE_VALUE(h,l) (((h) - 0xd800) * 0x400 + (l) - 0xdc00 + 0x10000)


achar *a_utf16_to_utf8 (const aunichar2  *str,along len,along *items_read,along *items_written,AError  **error)
{
  /* This function and n_utf16_to_ucs4 are almost exactly identical -
   * The lines that differ are marked.
   */
  const aunichar2 *in;
  achar *out;
  achar *result = NULL;
  aint n_bytes;
  aunichar high_surrogate;

  a_return_val_if_fail (str != NULL, NULL);

  n_bytes = 0;
  in = str;
  high_surrogate = 0;
  while ((len < 0 || in - str < len) && *in)
    {
      aunichar2 c = *in;
      aunichar wc;

      if (c >= 0xdc00 && c < 0xe000) /* low surrogate */
	{
	  if (high_surrogate)
	    {
	      wc = SURROGATE_VALUE (high_surrogate, c);
	      high_surrogate = 0;
	    }
	  else
	    {
		  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                   "Invalid sequence in conversion input");
	      goto err_out;
	    }
	}
      else
	{
	  if (high_surrogate)
	    {
		  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                   "Invalid sequence in conversion input");
	      goto err_out;
	    }

	  if (c >= 0xd800 && c < 0xdc00) /* high surrogate */
	    {
	      high_surrogate = c;
	      goto next1;
	    }
	  else
	    wc = c;
	}

      /********** DIFFERENT for UTF8/UCS4 **********/
      n_bytes += UTF8_LENGTH (wc);

    next1:
      in++;
    }

  if (high_surrogate && !items_read)
    {
	  a_error_set (error, A_CONVERT_ERROR, A_CONVERT_ERROR_PARTIAL_INPUT,
                           "Partial character sequence at end of input");
      goto err_out;
    }

  /* At this point, everything is valid, and we just need to convert
   */
  /********** DIFFERENT for UTF8/UCS4 **********/
  result = try_malloc_n (n_bytes + 1, 1, error);
  if (result == NULL)
      goto err_out;

  high_surrogate = 0;
  out = result;
  in = str;
  while (out < result + n_bytes)
    {
      aunichar2 c = *in;
      aunichar wc;

      if (c >= 0xdc00 && c < 0xe000) /* low surrogate */
	{
	  wc = SURROGATE_VALUE (high_surrogate, c);
	  high_surrogate = 0;
	}
      else if (c >= 0xd800 && c < 0xdc00) /* high surrogate */
	{
	  high_surrogate = c;
	  goto next2;
	}
      else
	wc = c;

      /********** DIFFERENT for UTF8/UCS4 **********/
      out += a_unichar_to_utf8 (wc, out);

    next2:
      in++;
    }

  /********** DIFFERENT for UTF8/UCS4 **********/
  *out = '\0';

  if (items_written)
    /********** DIFFERENT for UTF8/UCS4 **********/
    *items_written = out - result;

 err_out:
  if (items_read)
    *items_read = in - str;

  return result;
}

achar *a_utf8_find_next_char (const achar *p,const achar *end)
{
  if (end)
    {
      for (++p; p < end && (*p & 0xc0) == 0x80; ++p)
        ;
      return (p >= end) ? NULL : (char *)p;
    }
  else
    {
      for (++p; (*p & 0xc0) == 0x80; ++p)
        ;
      return (char *)p;
    }
}



achar *a_utf8_strchr (const char *p,assize len,aunichar c)
{
    char ch[10];
    int charlen = a_unichar_to_utf8 (c, ch);
    ch[charlen] = '\0';
    return a_strstr_len (p, len, ch);
}

char *a_utf8_make_valid (const char *str,assize len)
{
  const char *remainder, *invalid;
  asize remaining_bytes, valid_bytes;

  a_return_val_if_fail (str != NULL, NULL);

  if (len < 0)
    len = strlen (str);

  remainder = str;
  remaining_bytes = len;
  char string[1024];
  int count=0;
  while (remaining_bytes != 0){
      if (a_utf8_validate (remainder, remaining_bytes, &invalid))
	      break;
      valid_bytes = invalid - remainder;
      //if (string == NULL)
	     //string = g_string_sized_new (remaining_bytes);
      memcpy(string+count,remainder,valid_bytes);
      count+=valid_bytes;
      //g_string_append_len (string, remainder, valid_bytes);
      /* append U+FFFD REPLACEMENT CHARACTER */
      //g_string_append (string, "\357\277\275");
      memcpy(string+count,"\357\277\275",strlen("\357\277\275"));
      count+=strlen("\357\277\275");

      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
  }

  if (count == NULL)
    return a_strndup (str, len);
  memcpy(string+count,remainder,remaining_bytes);
  count+=remaining_bytes;

 // g_string_append_len (string, remainder, remaining_bytes);
 // g_string_append_c (string, '\0');
  string[count]='\0';
  if(a_utf8_validate (string, -1, NULL)){
	  a_error("出错了 a_utf8_make_valid");
  }
  return a_strndup (string, count);
}


#define G_UNICHAR_FULLWIDTH_A 0xff21
#define G_UNICHAR_FULLWIDTH_I 0xff29
#define G_UNICHAR_FULLWIDTH_J 0xff2a
#define G_UNICHAR_FULLWIDTH_F 0xff26
#define G_UNICHAR_FULLWIDTH_a 0xff41
#define G_UNICHAR_FULLWIDTH_f 0xff46

#define ATTR_TABLE(Page) (((Page) <= G_UNICODE_LAST_PAGE_PART1) \
                          ? attr_table_part1[Page] \
                          : attr_table_part2[(Page) - 0xe00])

#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == G_UNICODE_MAX_TABLE_INDEX) ? 0 : (attr_data[ATTR_TABLE(Page)][Char]))

#define TTYPE_PART1(Page, Char) \
  ((type_table_part1[Page] >= A_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part1[Page] - A_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part1[Page]][Char]))

#define TTYPE_PART2(Page, Char) \
  ((type_table_part2[Page] >= A_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part2[Page] - A_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part2[Page]][Char]))

#define TYPE(Char) \
  (((Char) <= A_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= A_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : A_UNICODE_UNASSIGNED))


#define IS(Type, Class) (((auint)1 << (Type)) & (Class))
#define OR(Type, Rest)  (((auint)1 << (Type)) | (Rest))



#define ISALPHA(Type)   IS ((Type),             \
                OR (A_UNICODE_LOWERCASE_LETTER, \
                OR (A_UNICODE_UPPERCASE_LETTER, \
                OR (A_UNICODE_TITLECASE_LETTER, \
                OR (A_UNICODE_MODIFIER_LETTER,  \
                OR (A_UNICODE_OTHER_LETTER,     0))))))

#define ISALDIGIT(Type) IS ((Type),             \
                OR (A_UNICODE_DECIMAL_NUMBER,   \
                OR (A_UNICODE_LETTER_NUMBER,    \
                OR (A_UNICODE_OTHER_NUMBER,     \
                OR (A_UNICODE_LOWERCASE_LETTER, \
                OR (A_UNICODE_UPPERCASE_LETTER, \
                OR (A_UNICODE_TITLECASE_LETTER, \
                OR (A_UNICODE_MODIFIER_LETTER,  \
                OR (A_UNICODE_OTHER_LETTER,     0)))))))))

#define ISMARK(Type)    IS ((Type),             \
                OR (G_UNICODE_NON_SPACING_MARK, \
                OR (G_UNICODE_SPACING_MARK, \
                OR (G_UNICODE_ENCLOSING_MARK,   0))))

#define ISZEROWIDTHTYPE(Type)   IS ((Type),         \
                OR (G_UNICODE_NON_SPACING_MARK, \
                OR (G_UNICODE_ENCLOSING_MARK,   \
                OR (G_UNICODE_FORMAT,       0))))


aboolean a_unichar_isalnum (aunichar c)
{
  return ISALDIGIT (TYPE (c)) ? TRUE : FALSE;
}



