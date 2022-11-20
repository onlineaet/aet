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

#include <locale.h>
#include <errno.h>
#include <ctype.h>              /* For tolower() */
#include <string.h>


#include "nhash.h"

#include "nstrfuncs.h"
#include "nlog.h"
#include "nmem.h"
#include "nprintf.h"
#include "nptrarray.h"
#include "nunicode.h"
#include "nstring.h"
#include "ncharset.h"
#include "nconvert.h"

struct mapping_entry
{
  nuint16 src;
  nuint16 ascii;
};

struct mapping_range
{
  nuint16 start;
  nuint16 length;
};

struct locale_entry
{
  nuint8 name_offset;
  nuint8 item_id;
};

#include "ntranslit-data.h"


#define N_NINT64_MODIFIER "l"
#define N_NINT64_FORMAT "li"
#define N_NUINT64_FORMAT "lu"

static const nuint16 ascii_table_data[256] = {
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

const nuint16 * const n_ascii_table = ascii_table_data;




nchar* n_strdup (const nchar *str)
{
  nchar *new_str;
  nsize length;

  if (str)
    {
      length = strlen (str) + 1;
      new_str = n_new (char, length);
      memcpy (new_str, str, length);
    }
  else
    new_str = NULL;

  return new_str;
}


/**
 * n_strndup:
 * @str: the string to duplicate
 * @n: the maximum number of bytes to copy from @str
 *
 * Duplicates the first @n bytes of a string, returning a newly-allocated
 * buffer @n + 1 bytes long which will always be nul-terminated. If @str
 * is less than @n bytes long the buffer is padded with nuls. If @str is
 * %NULL it returns %NULL. The returned value should be freed when no longer
 * needed.
 *
 * To copy a number of characters from a UTF-8 encoded string,
 * use g_utf8_strncpy() instead.
 *
 * Returns: a newly-allocated buffer containing the first @n bytes
 *     of @str, nul-terminated
 */
nchar* n_strndup (const nchar *str,nsize        n)
{
  nchar *new_str;

  if (str)
    {
      new_str = n_new (nchar, n + 1);
      strncpy (new_str, str, n);
      new_str[n] = '\0';
    }
  else
    new_str = NULL;

  return new_str;
}

/**
 * n_strnfill:
 * @length: the length of the new string
 * @fill_char: the byte to fill the string with
 *
 * Creates a new string @length bytes long filled with @fill_char.
 * The returned string should be freed when no longer needed.
 *
 * Returns: a newly-allocated string filled the @fill_char
 */
nchar* n_strnfill (nsize length,nchar fill_char)
{
  nchar *str;
  str = n_new (nchar, length + 1);
  memset (str, (nuchar)fill_char, length);
  str[length] = '\0';
  return str;
}


nchar *n_stpcpy (nchar *dest,const nchar *src)
{

  nchar *d = dest;
  const nchar *s = src;
  n_return_val_if_fail (dest != NULL, NULL);
  n_return_val_if_fail (src != NULL, NULL);
  do
    *d++ = *s;
  while (*s++ != '\0');
  return d - 1;
}


nchar* n_strdup_vprintf (const nchar *format,va_list      args)
{
  nchar *string = NULL;
  n_vasprintf (&string, format, args);
  return string;
}


nchar* n_strdup_printf (const nchar *format,...)
{
  nchar *buffer;
  va_list args;
  va_start (args, format);
  buffer = n_strdup_vprintf (format, args);
  va_end (args);
  return buffer;
}


nchar* n_strconcat (const nchar *string1, ...)
{
  nsize   l;
  va_list args;
  nchar   *s;
  nchar   *concat;
  nchar   *ptr;

  if (!string1)
    return NULL;

  l = 1 + strlen (string1);
  va_start (args, string1);
  s = va_arg (args, nchar*);
  while (s)
    {
      l += strlen (s);
      s = va_arg (args, nchar*);
    }
  va_end (args);

  concat = n_new (nchar, l);
  ptr = concat;

  ptr = n_stpcpy (ptr, string1);
  va_start (args, string1);
  s = va_arg (args, nchar*);
  while (s)
    {
      ptr = n_stpcpy (ptr, s);
      s = va_arg (args, nchar*);
    }
  va_end (args);

  return concat;
}


ndouble n_strtod (const nchar *nptr, nchar      **endptr)
{
  nchar *fail_pos_1;
  nchar *fail_pos_2;
  ndouble val_1;
  ndouble val_2 = 0;

  n_return_val_if_fail (nptr != NULL, 0);

  fail_pos_1 = NULL;
  fail_pos_2 = NULL;
  val_1 = strtod (nptr, &fail_pos_1);
  if (fail_pos_1 && fail_pos_1[0] != 0)
    val_2 = n_ascii_strtod (nptr, &fail_pos_2);

  if (!fail_pos_1 || fail_pos_1[0] == 0 || fail_pos_1 >= fail_pos_2)
    {
      if (endptr)
        *endptr = fail_pos_1;
      return val_1;
    }
  else
    {
      if (endptr)
        *endptr = fail_pos_2;
      return val_2;
    }
}

/**
 * n_ascii_strtod:
 * @nptr:    the string to convert to a numeric value.
 * @endptr:  (out) (transfer none) (optional): if non-%NULL, it returns the
 *           character after the last character used in the conversion.
 *
 * Converts a string to a #ndouble value.
 *
 * This function behaves like the standard strtod() function
 * does in the C locale. It does this without actually changing
 * the current locale, since that would not be thread-safe.
 * A limitation of the implementation is that this function
 * will still accept localized versions of infinities and NANs.
 *
 * This function is typically used when reading configuration
 * files or other non-user input that should be locale independent.
 * To handle input from the user you should normally use the
 * locale-sensitive system strtod() function.
 *
 * To convert from a #ndouble to a string in a locale-insensitive
 * way, use n_ascii_dtostr().
 *
 * If the correct value would cause overflow, plus or minus %HUGE_VAL
 * is returned (according to the sign of the value), and %ERANGE is
 * stored in %errno. If the correct value would cause underflow,
 * zero is returned and %ERANGE is stored in %errno.
 *
 * This function resets %errno before calling strtod() so that
 * you can reliably detect overflow and underflow.
 *
 * Returns: the #ndouble value.
 */
ndouble n_ascii_strtod (const nchar *nptr,nchar      **endptr)
{
  nchar *fail_pos;
  ndouble val;
  struct lconv *locale_data;
  const char *decimal_point;
  nsize decimal_point_len;
  const char *p, *decimal_point_pos;
  const char *end = NULL; /* Silence gcc */
  int strtod_errno;

  n_return_val_if_fail (nptr != NULL, 0);

  fail_pos = NULL;

  locale_data = localeconv ();
  decimal_point = locale_data->decimal_point;
  decimal_point_len = strlen (decimal_point);


  n_assert (decimal_point_len != 0);

  decimal_point_pos = NULL;
  end = NULL;

  if (decimal_point[0] != '.' ||
      decimal_point[1] != 0)
    {
      p = nptr;
      /* Skip leading space */
      while (n_ascii_isspace (*p))
        p++;

      /* Skip leading optional sign */
      if (*p == '+' || *p == '-')
        p++;

      if (p[0] == '0' &&
          (p[1] == 'x' || p[1] == 'X'))
        {
          p += 2;
          /* HEX - find the (optional) decimal point */

          while (n_ascii_isxdigit (*p))
            p++;

          if (*p == '.')
            decimal_point_pos = p++;

          while (n_ascii_isxdigit (*p))
            p++;

          if (*p == 'p' || *p == 'P')
            p++;
          if (*p == '+' || *p == '-')
            p++;
          while (n_ascii_isdigit (*p))
            p++;

          end = p;
        }
      else if (n_ascii_isdigit (*p) || *p == '.')
        {
          while (n_ascii_isdigit (*p))
            p++;

          if (*p == '.')
            decimal_point_pos = p++;

          while (n_ascii_isdigit (*p))
            p++;

          if (*p == 'e' || *p == 'E')
            p++;
          if (*p == '+' || *p == '-')
            p++;
          while (n_ascii_isdigit (*p))
            p++;

          end = p;
        }
      /* For the other cases, we need not convert the decimal point */
    }

  if (decimal_point_pos)
    {
      char *copy, *c;

      /* We need to convert the '.' to the locale specific decimal point */
      copy = n_malloc (end - nptr + 1 + decimal_point_len);

      c = copy;
      memcpy (c, nptr, decimal_point_pos - nptr);
      c += decimal_point_pos - nptr;
      memcpy (c, decimal_point, decimal_point_len);
      c += decimal_point_len;
      memcpy (c, decimal_point_pos + 1, end - (decimal_point_pos + 1));
      c += end - (decimal_point_pos + 1);
      *c = 0;

      errno = 0;
      val = strtod (copy, &fail_pos);
      strtod_errno = errno;

      if (fail_pos)
        {
          if (fail_pos - copy > decimal_point_pos - nptr)
            fail_pos = (char *)nptr + (fail_pos - copy) - (decimal_point_len - 1);
          else
            fail_pos = (char *)nptr + (fail_pos - copy);
        }

      n_free (copy);

    }
  else if (end)
    {
      char *copy;

      copy = n_malloc (end - (char *)nptr + 1);
      memcpy (copy, nptr, end - nptr);
      *(copy + (end - (char *)nptr)) = 0;

      errno = 0;
      val = strtod (copy, &fail_pos);
      strtod_errno = errno;

      if (fail_pos)
        {
          fail_pos = (char *)nptr + (fail_pos - copy);
        }

      n_free (copy);
    }
  else
    {
      errno = 0;
      val = strtod (nptr, &fail_pos);
      strtod_errno = errno;
    }

  if (endptr)
    *endptr = fail_pos;

  errno = strtod_errno;

  return val;
}



nchar *n_ascii_dtostr (nchar  *buffer, nint buf_len, ndouble      d)
{
  return n_ascii_formatd (buffer, buf_len, "%.17g", d);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"


nchar *n_ascii_formatd (nchar *buffer,nint buf_len,const nchar *format,ndouble d)
{

  struct lconv *locale_data;
  const char *decimal_point;
  nsize decimal_point_len;
  nchar *p;
  int rest_len;
  nchar format_char;

  n_return_val_if_fail (buffer != NULL, NULL);
  n_return_val_if_fail (format[0] == '%', NULL);
  n_return_val_if_fail (strpbrk (format + 1, "'l%") == NULL, NULL);

  format_char = format[strlen (format) - 1];

  n_return_val_if_fail (format_char == 'e' || format_char == 'E' ||
                        format_char == 'f' || format_char == 'F' ||
                        format_char == 'g' || format_char == 'G',
                        NULL);

  if (format[0] != '%')
    return NULL;

  if (strpbrk (format + 1, "'l%"))
    return NULL;

  if (!(format_char == 'e' || format_char == 'E' ||
        format_char == 'f' || format_char == 'F' ||
        format_char == 'g' || format_char == 'G'))
    return NULL;

  _n_snprintf (buffer, buf_len, format, d);

  locale_data = localeconv ();
  decimal_point = locale_data->decimal_point;
  decimal_point_len = strlen (decimal_point);


  n_assert (decimal_point_len != 0);

  if (decimal_point[0] != '.' ||
      decimal_point[1] != 0)
    {
      p = buffer;

      while (n_ascii_isspace (*p))
        p++;

      if (*p == '+' || *p == '-')
        p++;

      while (ISDIGIT ((nuchar)*p))
        p++;

      if (strncmp (p, decimal_point, decimal_point_len) == 0)
        {
          *p = '.';
          p++;
          if (decimal_point_len > 1)
            {
              rest_len = strlen (p + (decimal_point_len - 1));
              memmove (p, p + (decimal_point_len - 1), rest_len);
              p[rest_len] = 0;
            }
        }
    }

  return buffer;
}
#pragma GCC diagnostic pop

#undef  ISSPACE
#define ISSPACE(c)              ((c) == ' ' || (c) == '\f' || (c) == '\n' || \
                                 (c) == '\r' || (c) == '\t' || (c) == '\v')
#define ISUPPER(c)              ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c)              ((c) >= 'a' && (c) <= 'z')
#define ISALPHA(c)              (ISUPPER (c) || ISLOWER (c))
#define TOUPPER(c)              (ISLOWER (c) ? (c) - 'a' + 'A' : (c))
#define TOLOWER(c)              (ISUPPER (c) ? (c) - 'A' + 'a' : (c))


static nuint64 n_parse_long_long (const nchar  *nptr,const nchar **endptr,nuint  base,nboolean     *negative)
{
  /* this code is based on on the strtol(3) code from GNU libc released under
   * the GNU Lesser General Public License.
   *
   * Copyright (C) 1991,92,94,95,96,97,98,99,2000,01,02
   *        Free Software Foundation, Inc.
   */
  nboolean overflow;
  nuint64 cutoff;
  nuint64 cutlim;
  nuint64 ui64;
  const nchar *s, *save;
  nuchar c;

  n_return_val_if_fail (nptr != NULL, 0);

  *negative = FALSE;
  if (base == 1 || base > 36)
    {
      errno = EINVAL;
      if (endptr)
        *endptr = nptr;
      return 0;
    }

  save = s = nptr;

  /* Skip white space.  */
  while (ISSPACE (*s))
    ++s;

  if (N_UNLIKELY (!*s))
    goto noconv;

  /* Check for a sign.  */
  if (*s == '-')
    {
      *negative = TRUE;
      ++s;
    }
  else if (*s == '+')
    ++s;

  /* Recognize number prefix and if BASE is zero, figure it out ourselves.  */
  if (*s == '0')
    {
      if ((base == 0 || base == 16) && TOUPPER (s[1]) == 'X')
        {
          s += 2;
          base = 16;
        }
      else if (base == 0)
        base = 8;
    }
  else if (base == 0)
    base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;
  cutoff = N_MAXUINT64 / base;
  cutlim = N_MAXUINT64 % base;

  overflow = FALSE;
  ui64 = 0;
  c = *s;
  for (; c; c = *++s)
    {
      if (c >= '0' && c <= '9')
        c -= '0';
      else if (ISALPHA (c))
        c = TOUPPER (c) - 'A' + 10;
      else
        break;
      if (c >= base)
        break;
      /* Check for overflow.  */
      if (ui64 > cutoff || (ui64 == cutoff && c > cutlim))
        overflow = TRUE;
      else
        {
          ui64 *= base;
          ui64 += c;
        }
    }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr)
    *endptr = s;

  if (N_UNLIKELY (overflow))
    {
      errno = ERANGE;
      return N_MAXUINT64;
    }

  return ui64;

 noconv:
  /* We must handle a special case here: the base is 0 or 16 and the
     first two characters are '0' and 'x', but the rest are no
     hexadecimal digits.  This is no error case.  We return 0 and
     ENDPTR points to the `x`.  */
  if (endptr)
    {
      if (save - nptr >= 2 && TOUPPER (save[-1]) == 'X'
          && save[-2] == '0')
        *endptr = &save[-1];
      else
        /*  There was no number to convert.  */
        *endptr = nptr;
    }
  return 0;
}

/**
 * n_ascii_strtoull:
 * @nptr:    the string to convert to a numeric value.
 * @endptr:  (out) (transfer none) (optional): if non-%NULL, it returns the
 *           character after the last character used in the conversion.
 * @base:    to be used for the conversion, 2..36 or 0
 *
 * Converts a string to a #nuint64 value.
 * This function behaves like the standard strtoull() function
 * does in the C locale. It does this without actually
 * changing the current locale, since that would not be
 * thread-safe.
 *
 * Note that input with a leading minus sign (`-`) is accepted, and will return
 * the negation of the parsed number, unless that would overflow a #nuint64.
 * Critically, this means you cannot assume that a short fixed length input will
 * never result in a low return value, as the input could have a leading `-`.
 *
 * This function is typically used when reading configuration
 * files or other non-user input that should be locale independent.
 * To handle input from the user you should normally use the
 * locale-sensitive system strtoull() function.
 *
 * If the correct value would cause overflow, %N_MAXUINT64
 * is returned, and `ERANGE` is stored in `errno`.
 * If the base is outside the valid range, zero is returned, and
 * `EINVAL` is stored in `errno`.
 * If the string conversion fails, zero is returned, and @endptr returns
 * @nptr (if @endptr is non-%NULL).
 *
 * Returns: the #nuint64 value or zero on error.
 *
 * Since: 2.2
 */
nuint64 n_ascii_strtoull (const nchar *nptr,nchar **endptr,nuint base)
{

  nboolean negative;
  nuint64 result;
  result = n_parse_long_long (nptr, (const nchar **) endptr, base, &negative);
  /* Return the result of the appropriate sign.  */
  return negative ? -result : result;
}


nint64 n_ascii_strtoll (const nchar *nptr,nchar **endptr,nuint        base)
{
  nboolean negative;
  nuint64 result;

  result = n_parse_long_long (nptr, (const nchar **) endptr, base, &negative);

  if (negative && result > (nuint64) N_MININT64)
    {
      errno = ERANGE;
      return N_MININT64;
    }
  else if (!negative && result > (nuint64) N_MAXINT64)
    {
      errno = ERANGE;
      return N_MAXINT64;
    }
  else if (negative)
    return - (nint64) result;
  else
    return (nint64) result;
}

/**
 * n_strerror:
 * @errnum: the system error number. See the standard C %errno
 *     documentation
 *
 * Returns a string corresponding to the given error code, e.g. "no
 * such process". Unlike strerror(), this always returns a string in
 * UTF-8 encoding, and the pointer is guaranteed to remain valid for
 * the lifetime of the process.
 *
 * Note that the string may be translated according to the current locale.
 *
 * The value of %errno will not be changed by this function. However, it may
 * be changed by intermediate function calls, so you should save its value
 * as soon as the call returns:
 * |[
 *   int saved_errno;
 *
 *   ret = read (blah);
 *   saved_errno = errno;
 *
 *   n_strerror (saved_errno);
 * ]|
 *
 * Returns: a UTF-8 string describing the error code. If the error code
 *     is unknown, it returns a string like "unknown error (<code>)".
 */
const nchar *n_strerror (nint errnum)
{
  static NHashTable *errors;
  //N_LOCK_DEFINE_STATIC (errors);
  const nchar *msg;
  nint saved_errno = errno;

 // N_LOCK (errors);
  if (errors)
    msg = n_hash_table_lookup (errors, NINT_TO_POINTER (errnum));
  else
    {
      errors = n_hash_table_new (NULL, NULL);
      msg = NULL;
    }

  if (!msg)
    {
      nchar buf[1024];
      NError *error = NULL;


      n_strlcpy (buf, xstrerror (errnum), sizeof (buf));
      msg = buf;
      if (!n_get_console_charset (NULL))
        {
          msg = n_locale_to_utf8 (msg, -1, NULL, NULL, &error);
          if (error)
            n_printf ("%s\n", error->message);
        }
      else if (msg == (const nchar *)buf)
        msg = n_strdup (buf);

      n_hash_table_insert (errors, NINT_TO_POINTER (errnum), (char *) msg);
    }
 // N_UNLOCK (errors);

  errno = saved_errno;
  return msg;
}

/**
 * n_strsignal:
 * @signum: the signal number. See the `signal` documentation
 *
 * Returns a string describing the given signal, e.g. "Segmentation fault".
 * You should use this function in preference to strsignal(), because it
 * returns a string in UTF-8 encoding, and since not all platforms support
 * the strsignal() function.
 *
 * Returns: a UTF-8 string describing the signal. If the signal is unknown,
 *     it returns "unknown signal (<signum>)".
 */
const nchar *
n_strsignal (nint signum)
{
  nchar *msg;
  nchar *tofree;
  const nchar *ret;

  msg = tofree = NULL;

#ifdef HAVE_STRSIGNAL
  msg = strsignal (signum);
  if (!n_get_console_charset (NULL))
    msg = tofree = n_locale_to_utf8 (msg, -1, NULL, NULL, NULL);
#endif

  if (!msg)
    msg = tofree = n_strdup_printf ("unknown signal (%d)", signum);
  //ret = n_intern_string (msg);
  ret=msg;
  n_free (tofree);

  return ret;
}



/**
 * n_strlcpy:
 * @dest: destination buffer
 * @src: source buffer
 * @dest_size: length of @dest in bytes
 *
 * Portability wrapper that calls strlcpy() on systems which have it,
 * and emulates strlcpy() otherwise. Copies @src to @dest; @dest is
 * guaranteed to be nul-terminated; @src must be nul-terminated;
 * @dest_size is the buffer size, not the number of bytes to copy.
 *
 * At most @dest_size - 1 characters will be copied. Always nul-terminates
 * (unless @dest_size is 0). This function does not allocate memory. Unlike
 * strncpy(), this function doesn't pad @dest (so it's often faster). It
 * returns the size of the attempted result, strlen (src), so if
 * @retval >= @dest_size, truncation occurred.
 *
 * Caveat: strlcpy() is supposedly more secure than strcpy() or strncpy(),
 * but if you really want to avoid screwups, n_strdup() is an even better
 * idea.
 *
 * Returns: length of @src
 */
nsize n_strlcpy (nchar       *dest,
           const nchar *src,
           nsize        dest_size)
{
  nchar *d = dest;
  const nchar *s = src;
  nsize n = dest_size;

  n_return_val_if_fail (dest != NULL, 0);
  n_return_val_if_fail (src  != NULL, 0);

  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0)
    do
      {
        nchar c = *s++;

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

/**
 * n_strlcat:
 * @dest: destination buffer, already containing one nul-terminated string
 * @src: source buffer
 * @dest_size: length of @dest buffer in bytes (not length of existing string
 *     inside @dest)
 *
 * Portability wrapper that calls strlcat() on systems which have it,
 * and emulates it otherwise. Appends nul-terminated @src string to @dest,
 * guaranteeing nul-termination for @dest. The total size of @dest won't
 * exceed @dest_size.
 *
 * At most @dest_size - 1 characters will be copied. Unlike strncat(),
 * @dest_size is the full size of dest, not the space left over. This
 * function does not allocate memory. It always nul-terminates (unless
 * @dest_size == 0 or there were no nul characters in the @dest_size
 * characters of dest to start with).
 *
 * Caveat: this is supposedly a more secure alternative to strcat() or
 * strncat(), but for real security n_strconcat() is harder to mess up.
 *
 * Returns: size of attempted result, which is MIN (dest_size, strlen
 *     (original dest)) + strlen (src), so if retval >= dest_size,
 *     truncation occurred.
 */
nsize n_strlcat (nchar       *dest,
           const nchar *src,
           nsize        dest_size)
{
  nchar *d = dest;
  const nchar *s = src;
  nsize bytes_left = dest_size;
  nsize dlength;  /* Logically, MIN (strlen (d), dest_size) */

  n_return_val_if_fail (dest != NULL, 0);
  n_return_val_if_fail (src  != NULL, 0);

  /* Find the end of dst and adjust bytes left but don't go past end */
  while (*d != 0 && bytes_left-- != 0)
    d++;
  dlength = d - dest;
  bytes_left = dest_size - dlength;

  if (bytes_left == 0)
    return dlength + strlen (s);

  while (*s != 0)
    {
      if (bytes_left != 1)
        {
          *d++ = *s;
          bytes_left--;
        }
      s++;
    }
  *d = 0;

  return dlength + (s - src);  /* count does not include NUL */
}


nchar* n_ascii_strdown (const nchar *str,nssize       len)
{
  nchar *result, *s;

  n_return_val_if_fail (str != NULL, NULL);

  if (len < 0)
    len = (nssize) strlen (str);

  result = n_strndup (str, (nsize) len);
  for (s = result; *s; s++)
    *s = n_ascii_tolower (*s);

  return result;
}


nchar* n_ascii_strup (const nchar *str,nssize       len)
{
  nchar *result, *s;

  n_return_val_if_fail (str != NULL, NULL);

  if (len < 0)
    len = (nssize) strlen (str);

  result = n_strndup (str, (nsize) len);
  for (s = result; *s; s++)
    *s = n_ascii_toupper (*s);

  return result;
}

/**
 * n_str_is_ascii:
 * @str: a string
 *
 * Determines if a string is pure ASCII. A string is pure ASCII if it
 * contains no bytes with the high bit set.
 *
 * Returns: %TRUE if @str is ASCII
 *
 * Since: 2.40
 */
nboolean n_str_is_ascii (const nchar *str)
{
  nsize i;

  for (i = 0; str[i]; i++)
    if (str[i] & 0x80)
      return FALSE;

  return TRUE;
}




/**
 * n_strreverse:
 * @string: the string to reverse
 *
 * Reverses all of the bytes in a string. For example,
 * `n_strreverse ("abcdef")` will result in "fedcba".
 *
 * Note that n_strreverse() doesn't work on UTF-8 strings
 * containing multibyte characters. For that purpose, use
 * g_utf8_strreverse().
 *
 * Returns: the same pointer passed in as @string
 */
nchar* n_strreverse (nchar *string)
{
  n_return_val_if_fail (string != NULL, NULL);

  if (*string)
    {
      nchar *h, *t;

      h = string;
      t = string + strlen (string) - 1;

      while (h < t)
        {
          nchar c;

          c = *h;
          *h = *t;
          h++;
          *t = c;
          t--;
        }
    }

  return string;
}

/**
 * n_ascii_tolower:
 * @c: any character
 *
 * Convert a character to ASCII lower case.
 *
 * Unlike the standard C library tolower() function, this only
 * recognizes standard ASCII letters and ignores the locale, returning
 * all non-ASCII characters unchanged, even if they are lower case
 * letters in a particular character set. Also unlike the standard
 * library function, this takes and returns a char, not an int, so
 * don't call it on %EOF but no need to worry about casting to #nuchar
 * before passing a possibly non-ASCII character in.
 *
 * Returns: the result of converting @c to lower case. If @c is
 *     not an ASCII upper case letter, @c is returned unchanged.
 */
nchar n_ascii_tolower (nchar c)
{
  return n_ascii_isupper (c) ? c - 'A' + 'a' : c;
}

/**
 * n_ascii_toupper:
 * @c: any character
 *
 * Convert a character to ASCII upper case.
 *
 * Unlike the standard C library toupper() function, this only
 * recognizes standard ASCII letters and ignores the locale, returning
 * all non-ASCII characters unchanged, even if they are upper case
 * letters in a particular character set. Also unlike the standard
 * library function, this takes and returns a char, not an int, so
 * don't call it on %EOF but no need to worry about casting to #nuchar
 * before passing a possibly non-ASCII character in.
 *
 * Returns: the result of converting @c to upper case. If @c is not
 *    an ASCII lower case letter, @c is returned unchanged.
 */
nchar n_ascii_toupper (nchar c)
{
  return n_ascii_islower (c) ? c - 'a' + 'A' : c;
}

/**
 * n_ascii_digit_value:
 * @c: an ASCII character
 *
 * Determines the numeric value of a character as a decimal digit.
 * Differs from g_unichar_digit_value() because it takes a char, so
 * there's no worry about sign extension if characters are signed.
 *
 * Returns: If @c is a decimal digit (according to n_ascii_isdigit()),
 *    its numeric value. Otherwise, -1.
 */
int n_ascii_digit_value (nchar c)
{
  if (n_ascii_isdigit (c))
    return c - '0';
  return -1;
}

/**
 * n_ascii_xdigit_value:
 * @c: an ASCII character.
 *
 * Determines the numeric value of a character as a hexadecimal
 * digit. Differs from g_unichar_xdigit_value() because it takes
 * a char, so there's no worry about sign extension if characters
 * are signed.
 *
 * Returns: If @c is a hex digit (according to n_ascii_isxdigit()),
 *     its numeric value. Otherwise, -1.
 */
int n_ascii_xdigit_value (nchar c)
{
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return n_ascii_digit_value (c);
}

/**
 * n_ascii_strcasecmp:
 * @s1: string to compare with @s2
 * @s2: string to compare with @s1
 *
 * Compare two strings, ignoring the case of ASCII characters.
 *
 * Unlike the BSD strcasecmp() function, this only recognizes standard
 * ASCII letters and ignores the locale, treating all non-ASCII
 * bytes as if they are not letters.
 *
 * This function should be used only on strings that are known to be
 * in encodings where the bytes corresponding to ASCII letters always
 * represent themselves. This includes UTF-8 and the ISO-8859-*
 * charsets, but not for instance double-byte encodings like the
 * Windows Codepage 932, where the trailing bytes of double-byte
 * characters include all ASCII letters. If you compare two CP932
 * strings using this function, you will get false matches.
 *
 * Both @s1 and @s2 must be non-%NULL.
 *
 * Returns: 0 if the strings match, a negative value if @s1 < @s2,
 *     or a positive value if @s1 > @s2.
 */
nint n_ascii_strcasecmp (const nchar *s1,const nchar *s2)
{
  nint c1, c2;

  n_return_val_if_fail (s1 != NULL, 0);
  n_return_val_if_fail (s2 != NULL, 0);

  while (*s1 && *s2)
    {
      c1 = (nint)(nuchar) TOLOWER (*s1);
      c2 = (nint)(nuchar) TOLOWER (*s2);
      if (c1 != c2)
        return (c1 - c2);
      s1++; s2++;
    }

  return (((nint)(nuchar) *s1) - ((nint)(nuchar) *s2));
}

/**
 * n_ascii_strncasecmp:
 * @s1: string to compare with @s2
 * @s2: string to compare with @s1
 * @n: number of characters to compare
 *
 * Compare @s1 and @s2, ignoring the case of ASCII characters and any
 * characters after the first @n in each string.
 *
 * Unlike the BSD strcasecmp() function, this only recognizes standard
 * ASCII letters and ignores the locale, treating all non-ASCII
 * characters as if they are not letters.
 *
 * The same warning as in n_ascii_strcasecmp() applies: Use this
 * function only on strings known to be in encodings where bytes
 * corresponding to ASCII letters always represent themselves.
 *
 * Returns: 0 if the strings match, a negative value if @s1 < @s2,
 *     or a positive value if @s1 > @s2.
 */
nint n_ascii_strncasecmp (const nchar *s1, const nchar *s2, nsize        n)
{
  nint c1, c2;

  n_return_val_if_fail (s1 != NULL, 0);
  n_return_val_if_fail (s2 != NULL, 0);

  while (n && *s1 && *s2)
    {
      n -= 1;
      c1 = (nint)(nuchar) TOLOWER (*s1);
      c2 = (nint)(nuchar) TOLOWER (*s2);
      if (c1 != c2)
        return (c1 - c2);
      s1++; s2++;
    }

  if (n)
    return (((nint) (nuchar) *s1) - ((nint) (nuchar) *s2));
  else
    return 0;
}




/**
 * n_strdelimit:
 * @string: the string to convert
 * @delimiters: (nullable): a string containing the current delimiters,
 *     or %NULL to use the standard delimiters defined in #N_STR_DELIMITERS
 * @new_delimiter: the new delimiter character
 *
 * Converts any delimiter characters in @string to @new_delimiter.
 * Any characters in @string which are found in @delimiters are
 * changed to the @new_delimiter character. Modifies @string in place,
 * and returns @string itself, not a copy. The return value is to
 * allow nesting such as
 * |[<!-- language="C" -->
 *   n_ascii_strup (n_strdelimit (str, "abc", '?'))
 * ]|
 *
 * In order to modify a copy, you may use `n_strdup()`:
 * |[<!-- language="C" -->
 *   reformatted = n_strdelimit (n_strdup (const_str), "abc", '?');
 *   ...
 *   g_free (reformatted);
 * ]|
 *
 * Returns: @string
 */
nchar *n_strdelimit (nchar *string,const nchar *delimiters,nchar  new_delim)
{
  nchar *c;

  n_return_val_if_fail (string != NULL, NULL);

  if (!delimiters)
    delimiters = N_STR_DELIMITERS;

  for (c = string; *c; c++)
    {
      if (strchr (delimiters, *c))
        *c = new_delim;
    }

  return string;
}

/**
 * n_strcanon:
 * @string: a nul-terminated array of bytes
 * @valid_chars: bytes permitted in @string
 * @substitutor: replacement character for disallowed bytes
 *
 * For each character in @string, if the character is not in @valid_chars,
 * replaces the character with @substitutor. Modifies @string in place,
 * and return @string itself, not a copy. The return value is to allow
 * nesting such as
 * |[<!-- language="C" -->
 *   n_ascii_strup (n_strcanon (str, "abc", '?'))
 * ]|
 *
 * In order to modify a copy, you may use `n_strdup()`:
 * |[<!-- language="C" -->
 *   reformatted = n_strcanon (n_strdup (const_str), "abc", '?');
 *   ...
 *   g_free (reformatted);
 * ]|
 *
 * Returns: @string
 */
nchar *n_strcanon (nchar *string,const nchar *valid_chars,nchar        substitutor)
{
  nchar *c;

  n_return_val_if_fail (string != NULL, NULL);
  n_return_val_if_fail (valid_chars != NULL, NULL);

  for (c = string; *c; c++)
    {
      if (!strchr (valid_chars, *c))
        *c = substitutor;
    }

  return string;
}

/**
 * n_strcompress:
 * @source: a string to compress
 *
 * Replaces all escaped characters with their one byte equivalent.
 *
 * This function does the reverse conversion of n_strescape().
 *
 * Returns: a newly-allocated copy of @source with all escaped
 *     character compressed
 */
nchar *n_strcompress (const nchar *source)
{
  const nchar *p = source, *octal;
  nchar *dest;
  nchar *q;

  n_return_val_if_fail (source != NULL, NULL);

  dest = n_malloc (strlen (source) + 1);
  q = dest;

  while (*p)
    {
      if (*p == '\\')
        {
          p++;
          switch (*p)
            {
            case '\0':
              n_warning ("n_strcompress: trailing \\");
              goto out;
            case '0':  case '1':  case '2':  case '3':  case '4':
            case '5':  case '6':  case '7':
              *q = 0;
              octal = p;
              while ((p < octal + 3) && (*p >= '0') && (*p <= '7'))
                {
                  *q = (*q * 8) + (*p - '0');
                  p++;
                }
              q++;
              p--;
              break;
            case 'b':
              *q++ = '\b';
              break;
            case 'f':
              *q++ = '\f';
              break;
            case 'n':
              *q++ = '\n';
              break;
            case 'r':
              *q++ = '\r';
              break;
            case 't':
              *q++ = '\t';
              break;
            case 'v':
              *q++ = '\v';
              break;
            default:            /* Also handles \" and \\ */
              *q++ = *p;
              break;
            }
        }
      else
        *q++ = *p;
      p++;
    }
out:
  *q = 0;

  return dest;
}

/**
 * n_strescape:
 * @source: a string to escape
 * @exceptions: (nullable): a string of characters not to escape in @source
 *
 * Escapes the special characters '\b', '\f', '\n', '\r', '\t', '\v', '\'
 * and '"' in the string @source by inserting a '\' before
 * them. Additionally all characters in the range 0x01-0x1F (everything
 * below SPACE) and in the range 0x7F-0xFF (all non-ASCII chars) are
 * replaced with a '\' followed by their octal representation.
 * Characters supplied in @exceptions are not escaped.
 *
 * n_strcompress() does the reverse conversion.
 *
 * Returns: a newly-allocated copy of @source with certain
 *     characters escaped. See above.
 */
nchar *n_strescape (const nchar *source,const nchar *exceptions)
{
  const nuchar *p;
  nchar *dest;
  nchar *q;
  nuchar excmap[256];

  n_return_val_if_fail (source != NULL, NULL);

  p = (nuchar *) source;
  /* Each source byte needs maximally four destination chars (\777) */
  q = dest = n_malloc (strlen (source) * 4 + 1);

  memset (excmap, 0, 256);
  if (exceptions)
    {
      nuchar *e = (nuchar *) exceptions;

      while (*e)
        {
          excmap[*e] = 1;
          e++;
        }
    }

  while (*p)
    {
      if (excmap[*p])
        *q++ = *p;
      else
        {
          switch (*p)
            {
            case '\b':
              *q++ = '\\';
              *q++ = 'b';
              break;
            case '\f':
              *q++ = '\\';
              *q++ = 'f';
              break;
            case '\n':
              *q++ = '\\';
              *q++ = 'n';
              break;
            case '\r':
              *q++ = '\\';
              *q++ = 'r';
              break;
            case '\t':
              *q++ = '\\';
              *q++ = 't';
              break;
            case '\v':
              *q++ = '\\';
              *q++ = 'v';
              break;
            case '\\':
              *q++ = '\\';
              *q++ = '\\';
              break;
            case '"':
              *q++ = '\\';
              *q++ = '"';
              break;
            default:
              if ((*p < ' ') || (*p >= 0177))
                {
                  *q++ = '\\';
                  *q++ = '0' + (((*p) >> 6) & 07);
                  *q++ = '0' + (((*p) >> 3) & 07);
                  *q++ = '0' + ((*p) & 07);
                }
              else
                *q++ = *p;
              break;
            }
        }
      p++;
    }
  *q = 0;
  return dest;
}

/**
 * n_strchug:
 * @string: a string to remove the leading whitespace from
 *
 * Removes leading whitespace from a string, by moving the rest
 * of the characters forward.
 *
 * This function doesn't allocate or reallocate any memory;
 * it modifies @string in place. Therefore, it cannot be used on
 * statically allocated strings.
 *
 * The pointer to @string is returned to allow the nesting of functions.
 *
 * Also see n_strchomp() and n_strstrip().
 *
 * Returns: @string
 */
nchar *n_strchug (nchar *string)
{
  nuchar *start;

  n_return_val_if_fail (string != NULL, NULL);

  for (start = (nuchar*) string; *start && n_ascii_isspace (*start); start++)
    ;

  memmove (string, start, strlen ((nchar *) start) + 1);

  return string;
}

/**
 * n_strchomp:
 * @string: a string to remove the trailing whitespace from
 *
 * Removes trailing whitespace from a string.
 *
 * This function doesn't allocate or reallocate any memory;
 * it modifies @string in place. Therefore, it cannot be used
 * on statically allocated strings.
 *
 * The pointer to @string is returned to allow the nesting of functions.
 *
 * Also see n_strchug() and n_strstrip().
 *
 * Returns: @string
 */
nchar *n_strchomp (nchar *string)
{
  nsize len;

  n_return_val_if_fail (string != NULL, NULL);

  len = strlen (string);
  while (len--)
    {
      if (n_ascii_isspace ((nuchar) string[len]))
        string[len] = '\0';
      else
        break;
    }

  return string;
}

/**
 * n_strsplit:
 * @string: a string to split
 * @delimiter: a string which specifies the places at which to split
 *     the string. The delimiter is not included in any of the resulting
 *     strings, unless @max_tokens is reached.
 * @max_tokens: the maximum number of pieces to split @string into.
 *     If this is less than 1, the string is split completely.
 *
 * Splits a string into a maximum of @max_tokens pieces, using the given
 * @delimiter. If @max_tokens is reached, the remainder of @string is
 * appended to the last token.
 *
 * As an example, the result of n_strsplit (":a:bc::d:", ":", -1) is a
 * %NULL-terminated vector containing the six strings "", "a", "bc", "", "d"
 * and "".
 *
 * As a special case, the result of splitting the empty string "" is an empty
 * vector, not a vector containing a single string. The reason for this
 * special case is that being able to represent an empty vector is typically
 * more useful than consistent handling of empty elements. If you do need
 * to represent empty elements, you'll need to check for the empty string
 * before calling n_strsplit().
 *
 * Returns: a newly-allocated %NULL-terminated array of strings. Use
 *    n_strfreev() to free it.
 */
nchar** n_strsplit (const nchar *string,const nchar *delimiter,nint  max_tokens)
{
  char *s;
  const nchar *remainder;
  NPtrArray *string_list;

  n_return_val_if_fail (string != NULL, NULL);
  n_return_val_if_fail (delimiter != NULL, NULL);
  n_return_val_if_fail (delimiter[0] != '\0', NULL);

  if (max_tokens < 1)
    max_tokens = N_MAXINT;

  string_list = n_ptr_array_new ();
  remainder = string;
  s = strstr (remainder, delimiter);
  if (s)
    {
      nsize delimiter_len = strlen (delimiter);

      while (--max_tokens && s)
        {
          nsize len;

          len = s - remainder;
          n_ptr_array_add (string_list, n_strndup (remainder, len));
          remainder = s + delimiter_len;
          s = strstr (remainder, delimiter);
        }
    }
  if (*string)
    n_ptr_array_add (string_list, n_strdup (remainder));

  n_ptr_array_add (string_list, NULL);

  return (char **) n_ptr_array_free (string_list, FALSE);
}



void n_strfreev (nchar **str_array)
{
  if (str_array)
    {
      nsize i;

      for (i = 0; str_array[i] != NULL; i++)
        n_free (str_array[i]);

      n_free (str_array);
    }
}

/**
 * n_strdupv:
 * @str_array: (nullable): a %NULL-terminated array of strings
 *
 * Copies %NULL-terminated array of strings. The copy is a deep copy;
 * the new array should be freed by first freeing each string, then
 * the array itself. n_strfreev() does this for you. If called
 * on a %NULL value, n_strdupv() simply returns %NULL.
 *
 * Returns: (nullable): a new %NULL-terminated array of strings.
 */
nchar** n_strdupv (nchar **str_array)
{
  if (str_array)
    {
      nsize i;
      nchar **retval;

      i = 0;
      while (str_array[i])
        ++i;

      retval = n_new (nchar*, i + 1);

      i = 0;
      while (str_array[i])
        {
          retval[i] = n_strdup (str_array[i]);
          ++i;
        }
      retval[i] = NULL;

      return retval;
    }
  else
    return NULL;
}

/**
 * n_strjoinv:
 * @separator: (nullable): a string to insert between each of the
 *     strings, or %NULL
 * @str_array: a %NULL-terminated array of strings to join
 *
 * Joins a number of strings together to form one long string, with the
 * optional @separator inserted between each of them. The returned string
 * should be freed with g_free().
 *
 * If @str_array has no items, the return value will be an
 * empty string. If @str_array contains a single item, @separator will not
 * appear in the resulting string.
 *
 * Returns: a newly-allocated string containing all of the strings joined
 *     together, with @separator between them
 */
nchar* n_strjoinv (const nchar  *separator,nchar       **str_array)
{
  nchar *string;
  nchar *ptr;

  n_return_val_if_fail (str_array != NULL, NULL);

  if (separator == NULL)
    separator = "";

  if (*str_array)
    {
      nsize i;
      nsize len;
      nsize separator_len;

      separator_len = strlen (separator);
      /* First part, getting length */
      len = 1 + strlen (str_array[0]);
      for (i = 1; str_array[i] != NULL; i++)
        len += strlen (str_array[i]);
      len += separator_len * (i - 1);

      /* Second part, building string */
      string = n_new (nchar, len);
      ptr = n_stpcpy (string, *str_array);
      for (i = 1; str_array[i] != NULL; i++)
        {
          ptr = n_stpcpy (ptr, separator);
          ptr = n_stpcpy (ptr, str_array[i]);
        }
      }
  else
    string = n_strdup ("");

  return string;
}

/**
 * n_strjoin:
 * @separator: (nullable): a string to insert between each of the
 *     strings, or %NULL
 * @...: a %NULL-terminated list of strings to join
 *
 * Joins a number of strings together to form one long string, with the
 * optional @separator inserted between each of them. The returned string
 * should be freed with g_free().
 *
 * Returns: a newly-allocated string containing all of the strings joined
 *     together, with @separator between them
 */
nchar* n_strjoin (const nchar *separator,...)
{
  nchar *string, *s;
  va_list args;
  nsize len;
  nsize separator_len;
  nchar *ptr;

  if (separator == NULL)
    separator = "";

  separator_len = strlen (separator);

  va_start (args, separator);

  s = va_arg (args, nchar*);

  if (s)
    {
      /* First part, getting length */
      len = 1 + strlen (s);

      s = va_arg (args, nchar*);
      while (s)
        {
          len += separator_len + strlen (s);
          s = va_arg (args, nchar*);
        }
      va_end (args);

      /* Second part, building string */
      string = n_new (nchar, len);

      va_start (args, separator);

      s = va_arg (args, nchar*);
      ptr = n_stpcpy (string, s);

      s = va_arg (args, nchar*);
      while (s)
        {
          ptr = n_stpcpy (ptr, separator);
          ptr = n_stpcpy (ptr, s);
          s = va_arg (args, nchar*);
        }
    }
  else
    string = n_strdup ("");

  va_end (args);

  return string;
}


/**
 * n_strstr_len:
 * @haystack: a string
 * @haystack_len: the maximum length of @haystack. Note that -1 is
 *     a valid length, if @haystack is nul-terminated, meaning it will
 *     search through the whole string.
 * @needle: the string to search for
 *
 * Searches the string @haystack for the first occurrence
 * of the string @needle, limiting the length of the search
 * to @haystack_len.
 *
 * Returns: a pointer to the found occurrence, or
 *    %NULL if not found.
 */
nchar *n_strstr_len (const nchar *haystack,nssize haystack_len,const nchar *needle)
{
  n_return_val_if_fail (haystack != NULL, NULL);
  n_return_val_if_fail (needle != NULL, NULL);

  if (haystack_len < 0)
    return strstr (haystack, needle);
  else
    {
      const nchar *p = haystack;
      nsize needle_len = strlen (needle);
      nsize haystack_len_unsigned = haystack_len;
      const nchar *end;
      nsize i;

      if (needle_len == 0)
        return (nchar *)haystack;

      if (haystack_len_unsigned < needle_len)
        return NULL;

      end = haystack + haystack_len - needle_len;

      while (p <= end && *p)
        {
          for (i = 0; i < needle_len; i++)
            if (p[i] != needle[i])
              goto next;

          return (nchar *)p;

        next:
          p++;
        }

      return NULL;
    }
}

/**
 * n_strrstr:
 * @haystack: a nul-terminated string
 * @needle: the nul-terminated string to search for
 *
 * Searches the string @haystack for the last occurrence
 * of the string @needle.
 *
 * Returns: a pointer to the found occurrence, or
 *    %NULL if not found.
 */
nchar *n_strrstr (const nchar *haystack,const nchar *needle)
{
  nsize i;
  nsize needle_len;
  nsize haystack_len;
  const nchar *p;

  n_return_val_if_fail (haystack != NULL, NULL);
  n_return_val_if_fail (needle != NULL, NULL);

  needle_len = strlen (needle);
  haystack_len = strlen (haystack);

  if (needle_len == 0)
    return (nchar *)haystack;

  if (haystack_len < needle_len)
    return NULL;

  p = haystack + haystack_len - needle_len;

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

/**
 * n_strrstr_len:
 * @haystack: a nul-terminated string
 * @haystack_len: the maximum length of @haystack
 * @needle: the nul-terminated string to search for
 *
 * Searches the string @haystack for the last occurrence
 * of the string @needle, limiting the length of the search
 * to @haystack_len.
 *
 * Returns: a pointer to the found occurrence, or
 *    %NULL if not found.
 */
nchar *n_strrstr_len (const nchar *haystack,nssize haystack_len,const nchar *needle)
{
  n_return_val_if_fail (haystack != NULL, NULL);
  n_return_val_if_fail (needle != NULL, NULL);

  if (haystack_len < 0)
    return n_strrstr (haystack, needle);
  else
    {
      nsize needle_len = strlen (needle);
      const nchar *haystack_max = haystack + haystack_len;
      const nchar *p = haystack;
      nsize i;

      while (p < haystack_max && *p)
        p++;

      if (p < haystack + needle_len)
        return NULL;

      p -= needle_len;

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
}


/**
 * n_str_has_suffix:
 * @str: a nul-terminated string
 * @suffix: the nul-terminated suffix to look for
 *
 * Looks whether the string @str ends with @suffix.
 *
 * Returns: %TRUE if @str end with @suffix, %FALSE otherwise.
 *
 * Since: 2.2
 */
nboolean n_str_has_suffix (const nchar *str,const nchar *suffix)
{
  nsize str_len;
  nsize suffix_len;

  n_return_val_if_fail (str != NULL, FALSE);
  n_return_val_if_fail (suffix != NULL, FALSE);

  str_len = strlen (str);
  suffix_len = strlen (suffix);

  if (str_len < suffix_len)
    return FALSE;

  return strcmp (str + str_len - suffix_len, suffix) == 0;
}

/**
 * n_str_has_prefix:
 * @str: a nul-terminated string
 * @prefix: the nul-terminated prefix to look for
 *
 * Looks whether the string @str begins with @prefix.
 *
 * Returns: %TRUE if @str begins with @prefix, %FALSE otherwise.
 *
 * Since: 2.2
 */
nboolean n_str_has_prefix (const nchar *str,const nchar *prefix)
{
  n_return_val_if_fail (str != NULL, FALSE);
  n_return_val_if_fail (prefix != NULL, FALSE);

  return strncmp (str, prefix, strlen (prefix)) == 0;
}

/**
 * n_strv_length:
 * @str_array: a %NULL-terminated array of strings
 *
 * Returns the length of the given %NULL-terminated
 * string array @str_array. @str_array must not be %NULL.
 *
 * Returns: length of @str_array.
 *
 * Since: 2.6
 */
nuint n_strv_length (nchar **str_array)
{
  nuint i = 0;

  n_return_val_if_fail (str_array != NULL, 0);

  while (str_array[i])
    ++i;

  return i;
}

static void index_add_folded (NPtrArray   *array,const nchar *start,const nchar *end)
{
  nchar *normal;

  normal = n_utf8_normalize (start, end - start, N_NORMALIZE_ALL_COMPOSE);

  /* TODO: Invent time machine.  Converse with Mustafa Ataturk... */
  if (strstr (normal, "ı") || strstr (normal, "İ"))
    {
      nchar *s = normal;
      NString *tmp;

      tmp = n_string_new (NULL);

      while (*s)
        {
          nchar *i, *I, *e;

          i = strstr (s, "ı");
          I = strstr (s, "İ");

          if (!i && !I)
            break;
          else if (i && !I)
            e = i;
          else if (I && !i)
            e = I;
          else if (i < I)
            e = i;
          else
            e = I;

          n_string_append_len (tmp, s, e - s);
          n_string_append_c (tmp, 'i');
          s = n_utf8_next_char (e);
        }

      n_string_append (tmp, s);
      n_free (normal);
      normal = n_string_free (tmp, FALSE);
    }

  n_ptr_array_add (array, n_utf8_casefold (normal, -1));
  n_free (normal);
}

static nchar **split_words (const nchar *value)
{
  const nchar *start = NULL;
  NPtrArray *result;
  const nchar *s;

  result = n_ptr_array_new ();

  for (s = value; *s; s = n_utf8_next_char (s))
    {
      nunichar c = n_utf8_get_char (s);

      if (start == NULL)
        {
          if (n_unichar_isalnum (c) || n_unichar_ismark (c))
            start = s;
        }
      else
        {
          if (!n_unichar_isalnum (c) && !n_unichar_ismark (c))
            {
              index_add_folded (result, start, s);
              start = NULL;
            }
        }
    }

  if (start)
    index_add_folded (result, start, s);

  n_ptr_array_add (result, NULL);

  return (nchar **) n_ptr_array_free (result, FALSE);
}

/**
 * n_str_tokenize_and_fold:
 * @string: a string
 * @translit_locale: (nullable): the language code (like 'de' or
 *   'en_GB') from which @string originates
 * @ascii_alternates: (out) (transfer full) (array zero-terminated=1): a
 *   return location for ASCII alternates
 *
 * Tokenises @string and performs folding on each token.
 *
 * A token is a non-empty sequence of alphanumeric characters in the
 * source string, separated by non-alphanumeric characters.  An
 * "alphanumeric" character for this purpose is one that matches
 * g_unichar_isalnum() or g_unichar_ismark().
 *
 * Each token is then (Unicode) normalised and case-folded.  If
 * @ascii_alternates is non-%NULL and some of the returned tokens
 * contain non-ASCII characters, ASCII alternatives will be generated.
 *
 * The number of ASCII alternatives that are generated and the method
 * for doing so is unspecified, but @translit_locale (if specified) may
 * improve the transliteration if the language of the source string is
 * known.
 *
 * Returns: (transfer full) (array zero-terminated=1): the folded tokens
 *
 * Since: 2.40
 **/
nchar **n_str_tokenize_and_fold (const nchar   *string,const nchar   *translit_locale, nchar  ***ascii_alternates)
{
  nchar **result;

  n_return_val_if_fail (string != NULL, NULL);

  if (ascii_alternates && n_str_is_ascii (string))
    {
      *ascii_alternates = n_new0 (nchar *, 0 + 1);
      ascii_alternates = NULL;
    }

  result = split_words (string);

  if (ascii_alternates)
    {
      nint i, j, n;

      n = n_strv_length (result);
      *ascii_alternates = n_new (nchar *, n + 1);
      j = 0;

      for (i = 0; i < n; i++)
        {
          if (!n_str_is_ascii (result[i]))
            {
              nchar *composed;
              nchar *ascii;
              nint k;

              composed = n_utf8_normalize (result[i], -1, N_NORMALIZE_ALL_COMPOSE);

              ascii = n_str_to_ascii (composed, translit_locale);

              /* Only accept strings that are now entirely alnums */
              for (k = 0; ascii[k]; k++)
                if (!n_ascii_isalnum (ascii[k]))
                  break;

              if (ascii[k] == '\0')
                /* Made it to the end... */
                (*ascii_alternates)[j++] = ascii;
              else
                n_free (ascii);

              n_free (composed);
            }
        }

      (*ascii_alternates)[j] = NULL;
    }

  return result;
}

/**
 * n_str_match_string:
 * @search_term: the search term from the user
 * @potential_hit: the text that may be a hit
 * @accept_alternates: %TRUE to accept ASCII alternates
 *
 * Checks if a search conducted for @search_term should match
 * @potential_hit.
 *
 * This function calls n_str_tokenize_and_fold() on both
 * @search_term and @potential_hit.  ASCII alternates are never taken
 * for @search_term but will be taken for @potential_hit according to
 * the value of @accept_alternates.
 *
 * A hit occurs when each folded token in @search_term is a prefix of a
 * folded token from @potential_hit.
 *
 * Depending on how you're performing the search, it will typically be
 * faster to call n_str_tokenize_and_fold() on each string in
 * your corpus and build an index on the returned folded tokens, then
 * call n_str_tokenize_and_fold() on the search term and
 * perform lookups into that index.
 *
 * As some examples, searching for ‘fred’ would match the potential hit
 * ‘Smith, Fred’ and also ‘Frédéric’.  Searching for ‘Fréd’ would match
 * ‘Frédéric’ but not ‘Frederic’ (due to the one-directional nature of
 * accent matching).  Searching ‘fo’ would match ‘Foo’ and ‘Bar Foo
 * Baz’, but not ‘SFO’ (because no word has ‘fo’ as a prefix).
 *
 * Returns: %TRUE if @potential_hit is a hit
 *
 * Since: 2.40
 **/
nboolean n_str_match_string (const nchar *search_term,const nchar *potential_hit,nboolean     accept_alternates)
{
  nchar **alternates = NULL;
  nchar **term_tokens;
  nchar **hit_tokens;
  nboolean matched;
  nint i, j;

  n_return_val_if_fail (search_term != NULL, FALSE);
  n_return_val_if_fail (potential_hit != NULL, FALSE);

  term_tokens = n_str_tokenize_and_fold (search_term, NULL, NULL);
  hit_tokens = n_str_tokenize_and_fold (potential_hit, NULL, accept_alternates ? &alternates : NULL);

  matched = TRUE;

  for (i = 0; term_tokens[i]; i++)
    {
      for (j = 0; hit_tokens[j]; j++)
        if (n_str_has_prefix (hit_tokens[j], term_tokens[i]))
          goto one_matched;

      if (accept_alternates)
        for (j = 0; alternates[j]; j++)
          if (n_str_has_prefix (alternates[j], term_tokens[i]))
            goto one_matched;

      matched = FALSE;
      break;

one_matched:
      continue;
    }

  n_strfreev (term_tokens);
  n_strfreev (hit_tokens);
  n_strfreev (alternates);

  return matched;
}

/**
 * n_strv_contains:
 * @strv: a %NULL-terminated array of strings
 * @str: a string
 *
 * Checks if @strv contains @str. @strv must not be %NULL.
 *
 * Returns: %TRUE if @str is an element of @strv, according to n_str_equal().
 *
 * Since: 2.44
 */
nboolean n_strv_contains (const nchar * const *strv, const nchar         *str)
{
  n_return_val_if_fail (strv != NULL, FALSE);
  n_return_val_if_fail (str != NULL, FALSE);

  for (; *strv != NULL; strv++)
    {
      if (n_str_equal (str, *strv))
        return TRUE;
    }

  return FALSE;
}


nboolean n_strv_equal (const nchar * const *strv1,const nchar * const *strv2)
{
  n_return_val_if_fail (strv1 != NULL, FALSE);
  n_return_val_if_fail (strv2 != NULL, FALSE);

  if (strv1 == strv2)
    return TRUE;

  for (; *strv1 != NULL && *strv2 != NULL; strv1++, strv2++)
    {
      if (!n_str_equal (*strv1, *strv2))
        return FALSE;
    }

  return (*strv1 == NULL && *strv2 == NULL);
}

static nboolean str_has_sign (const nchar *str)
{
  return str[0] == '-' || str[0] == '+';
}

static nboolean str_has_hex_prefix (const nchar *str)
{
  return str[0] == '0' && n_ascii_tolower (str[1]) == 'x';
}


nboolean n_ascii_string_to_signed (const nchar  *str,nuint base,nint64 min, nint64  max, nint64  *out_num, NError **error)
{
  nint64 number;
  const nchar *end_ptr = NULL;
  nint saved_errno = 0;

  n_return_val_if_fail (str != NULL, FALSE);
  n_return_val_if_fail (base >= 2 && base <= 36, FALSE);
  n_return_val_if_fail (min <= max, FALSE);
  n_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (str[0] == '\0'){
      n_set_error_literal (error,
                           N_NUMBER_PARSER_ERROR, N_NUMBER_PARSER_ERROR_INVALID,
                           "Empty string is not a number");
      return FALSE;
  }

  errno = 0;
  number = n_ascii_strtoll (str, (nchar **)&end_ptr, base);
  saved_errno = errno;

  if (/* We do not allow leading whitespace, but n_ascii_strtoll
       * accepts it and just skips it, so we need to check for it
       * ourselves.
       */
      n_ascii_isspace (str[0]) ||
      /* We don't support hexadecimal numbers prefixed with 0x or
       * 0X.
       */
      (base == 16 &&
       (str_has_sign (str) ? str_has_hex_prefix (str + 1) : str_has_hex_prefix (str))) ||
      (saved_errno != 0 && saved_errno != ERANGE) ||
      end_ptr == NULL ||
      *end_ptr != '\0')
    {
      n_set_error (error,
                   N_NUMBER_PARSER_ERROR, N_NUMBER_PARSER_ERROR_INVALID,
                   "“%s” is not a signed number", str);
      return FALSE;
    }
  if (saved_errno == ERANGE || number < min || number > max)
    {
      nchar *min_str = n_strdup_printf ("%" N_NINT64_FORMAT, min);
      nchar *max_str = n_strdup_printf ("%" N_NINT64_FORMAT, max);

      n_set_error (error,
                   N_NUMBER_PARSER_ERROR, N_NUMBER_PARSER_ERROR_OUT_OF_BOUNDS,
                   "Number “%s” is out of bounds [%s, %s]",
                   str, min_str, max_str);
      n_free (min_str);
      n_free (max_str);
      return FALSE;
    }
  if (out_num != NULL)
    *out_num = number;
  return TRUE;
}


nboolean n_ascii_string_to_unsigned (const nchar  *str,
                            nuint         base,
                            nuint64       min,
                            nuint64       max,
                            nuint64      *out_num,
                            NError      **error)
{
  nuint64 number;
  const nchar *end_ptr = NULL;
  nint saved_errno = 0;

  n_return_val_if_fail (str != NULL, FALSE);
  n_return_val_if_fail (base >= 2 && base <= 36, FALSE);
  n_return_val_if_fail (min <= max, FALSE);
  n_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (str[0] == '\0')
    {
      n_set_error_literal (error,
                           N_NUMBER_PARSER_ERROR, N_NUMBER_PARSER_ERROR_INVALID,
                           "Empty string is not a number");
      return FALSE;
    }

  errno = 0;
  number = n_ascii_strtoull (str, (nchar **)&end_ptr, base);
  saved_errno = errno;

  if (/* We do not allow leading whitespace, but n_ascii_strtoull
       * accepts it and just skips it, so we need to check for it
       * ourselves.
       */
      n_ascii_isspace (str[0]) ||
      /* Unsigned number should have no sign.
       */
      str_has_sign (str) ||
      /* We don't support hexadecimal numbers prefixed with 0x or
       * 0X.
       */
      (base == 16 && str_has_hex_prefix (str)) ||
      (saved_errno != 0 && saved_errno != ERANGE) ||
      end_ptr == NULL ||
      *end_ptr != '\0')
    {
      n_set_error (error,
                   N_NUMBER_PARSER_ERROR, N_NUMBER_PARSER_ERROR_INVALID,
                   "“%s” is not an unsigned number", str);
      return FALSE;
    }
  if (saved_errno == ERANGE || number < min || number > max)
    {
      nchar *min_str = n_strdup_printf ("%" N_NUINT64_FORMAT, min);
      nchar *max_str = n_strdup_printf ("%" N_NUINT64_FORMAT, max);

      n_set_error (error,
                   N_NUMBER_PARSER_ERROR, N_NUMBER_PARSER_ERROR_OUT_OF_BOUNDS,
                   "Number “%s” is out of bounds [%s, %s]",
                   str, min_str, max_str);
      n_free (min_str);
      n_free (max_str);
      return FALSE;
    }
  if (out_num != NULL)
    *out_num = number;
  return TRUE;
}

int n_strcmp0 (const char     *str1,const char     *str2)
{
  if (!str1)
    return -(str1 != str2);
  if (!str2)
    return str1 != str2;
  return strcmp (str1, str2);
}

nchar *n_str_to_ascii (const nchar *str,
                const nchar *from_locale)
{
  NString *result;
  nuint item_id;

  n_return_val_if_fail (str != NULL, NULL);

  if (n_str_is_ascii (str))
    return n_strdup (str);

  if (from_locale)
    item_id = 0;//lookup_item_id_for_locale (from_locale);
  else
    item_id = 0;//get_default_item_id ();

  result = n_string_sized_new (strlen (str));

  while (*str)
    {
      /* We only need to transliterate non-ASCII values... */
      if (*str & 0x80)
        {
          nunichar key[MAX_KEY_SIZE];
          const nchar *r;
          nint consumed;
          nint r_len;
          nunichar c;

          N_STATIC_ASSERT(MAX_KEY_SIZE == 2);

          c = n_utf8_get_char (str);

          /* This is where it gets evil...
           *
           * We know that MAX_KEY_SIZE is 2.  We also know that we
           * only want to try another character if it's non-ascii.
           */
          str = n_utf8_next_char (str);

          key[0] = c;
          if (*str & 0x80)
            key[1] = n_utf8_get_char (str);
          else
            key[1] = 0;

          r = NULL;//lookup_in_item (item_id, key, &r_len, &consumed);

          /* If we failed to map two characters, try again with one.
           *
           * gconv behaviour is a bit weird here -- it seems to
           * depend in the randomness of the binary search and the
           * size of the input buffer as to what result we get here.
           *
           * Doing it this way is more work, but should be
           * more-correct.
           */
          if (r == NULL && key[1])
            {
              key[1] = 0;
              r = 0;//lookup_in_item (item_id, key, &r_len, &consumed);
            }

          if (r != NULL)
            {
              n_string_append_len (result, r, r_len);
              if (consumed == 2)
                /* If it took both then skip again */
                str = n_utf8_next_char (str);
            }
          else /* no match found */
            n_string_append_c (result, '?');
        }
      else /* ASCII case */
        n_string_append_c (result, *str++);
    }

  return n_string_free (result, FALSE);
}

