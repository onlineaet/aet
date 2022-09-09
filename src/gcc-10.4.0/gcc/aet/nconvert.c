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

#include <iconv.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>




#include "nconvert.h"
#include "nlog.h"
#include "nstrfuncs.h"
#include "nunicode.h"
#include "nptrarray.h"
#include "ncharset.h"
#include "nerror.h"
#include "nfileutils.h"
#include "nmem.h"



#define NUL_TERMINATOR_LENGTH 4

//N_DEFINE_QUARK (n_convert_error, n_convert_error)

static nboolean try_conversion (const char *to_codeset,
		const char *from_codeset,
		iconv_t    *cd)
{
  *cd = iconv_open (to_codeset, from_codeset);

  if (*cd == (iconv_t)-1 && errno == EINVAL)
    return FALSE;
  else
    return TRUE;
}

static nboolean try_to_aliases (const char **to_aliases,
		const char  *from_codeset,
		iconv_t     *cd)
{
  if (to_aliases)
    {
      const char **p = to_aliases;
      while (*p)
	{
	  if (try_conversion (*p, from_codeset, cd))
	    return TRUE;

	  p++;
	}
    }

  return FALSE;
}


NIConv n_iconv_open (const nchar  *to_codeset,const nchar  *from_codeset)
{
  iconv_t cd;

  if (!try_conversion (to_codeset, from_codeset, &cd))
    {
      const char **to_aliases = _n_charset_get_aliases (to_codeset);
      const char **from_aliases = _n_charset_get_aliases (from_codeset);

      if (from_aliases)
	{
	  const char **p = from_aliases;
	  while (*p)
	    {
	      if (try_conversion (to_codeset, *p, &cd))
		goto out;

	      if (try_to_aliases (to_aliases, *p, &cd))
		goto out;

	      p++;
	    }
	}

      if (try_to_aliases (to_aliases, from_codeset, &cd))
	goto out;
    }

 out:
  return (cd == (iconv_t)-1) ? (NIConv)-1 : (NIConv)cd;
}


nsize n_iconv (NIConv   converter,
	 nchar  **inbuf,
	 nsize   *inbytes_left,
	 nchar  **outbuf,
	 nsize   *outbytes_left)
{
  iconv_t cd = (iconv_t)converter;

  return iconv (cd, inbuf, inbytes_left, outbuf, outbytes_left);
}

/**
 * n_iconv_close: (skip)
 * @converter: a conversion descriptor from n_iconv_open()
 *
 * Same as the standard UNIX routine iconv_close(), but
 * may be implemented via libiconv on UNIX flavors that lack
 * a native implementation. Should be called to clean up
 * the conversion descriptor from n_iconv_open() when
 * you are done converting things.
 *
 * GLib provides n_convert() and n_locale_to_utf8() which are likely
 * more convenient than the raw iconv wrappers.
 *
 * Returns: -1 on error, 0 on success
 **/
nint n_iconv_close (NIConv converter)
{
  iconv_t cd = (iconv_t)converter;

  return iconv_close (cd);
}

static NIConv open_converter (const nchar *to_codeset,const nchar *from_codeset,NError     **error)
{
  NIConv cd;

  cd = n_iconv_open (to_codeset, from_codeset);

  if (cd == (NIConv) -1)
    {
      /* Something went wrong.  */
      if (error)
	{
	  if (errno == EINVAL)
	    n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_NO_CONVERSION,
			 "Conversion from character set “%s” to “%s” is not supported",
			 from_codeset, to_codeset);
	  else
	    n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_FAILED,
			 "Could not open converter from “%s” to “%s”",
			 from_codeset, to_codeset);
	}
    }

  return cd;
}

static int close_converter (NIConv cd)
{
  if (cd == (NIConv) -1)
    return 0;

  return n_iconv_close (cd);
}


nchar* n_convert_with_iconv (const nchar *str,
		      nssize       len,
		      NIConv       converter,
		      nsize       *bytes_read,
		      nsize       *bytes_written,
		      NError     **error)
{
  nchar *dest;
  nchar *outp;
  const nchar *p;
  nsize inbytes_remaining;
  nsize outbytes_remaining;
  nsize err;
  nsize outbuf_size;
  nboolean have_error = FALSE;
  nboolean done = FALSE;
  nboolean reset = FALSE;

  n_return_val_if_fail (converter != (NIConv) -1, NULL);

  if (len < 0)
    len = strlen (str);

  p = str;
  inbytes_remaining = len;
  outbuf_size = len + NUL_TERMINATOR_LENGTH;

  outbytes_remaining = outbuf_size - NUL_TERMINATOR_LENGTH;
  outp = dest = n_malloc (outbuf_size);

  while (!done && !have_error){
      if (reset)
        err = n_iconv (converter, NULL, &inbytes_remaining, &outp, &outbytes_remaining);
      else
        err = n_iconv (converter, (char **)&p, &inbytes_remaining, &outp, &outbytes_remaining);

      if (err == (nsize) -1){
	  switch (errno)
	    {
	    case EINVAL:
	      /* Incomplete text, do not report an error */
	      done = TRUE;
	      break;
	    case E2BIG:
	      {
		    nsize used = outp - dest;
		    outbuf_size *= 2;
		    dest = n_realloc (dest, outbuf_size);
		    outp = dest + used;
		    outbytes_remaining = outbuf_size - used - NUL_TERMINATOR_LENGTH;
	      }
	      break;
	    case EILSEQ:
          n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE,"Invalid byte sequence in conversion input");
	      have_error = TRUE;
	      break;
	    default:
              {
                int errsv = errno;
                n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_FAILED,
                             "Error during conversion: %s",
                             n_strerror (errsv));
              }
	      have_error = TRUE;
	      break;
	    }
	}
      else if (err > 0)
        {
          /* @err gives the number of replacement characters used. */
          n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                               "Unrepresentable character in conversion input");
          have_error = TRUE;
        }
      else
	{
	  if (!reset)
	    {
	      /* call n_iconv with NULL inbuf to cleanup shift state */
	      reset = TRUE;
	      inbytes_remaining = 0;
	    }
	  else
	    done = TRUE;
	}
    }

  memset (outp, 0, NUL_TERMINATOR_LENGTH);
  if (bytes_read)
    *bytes_read = p - str;
  else
    {
      if ((p - str) != len)
	{
          if (!have_error)
            {
              n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_PARTIAL_INPUT,
                                   "Partial character sequence at end of input");
              have_error = TRUE;
            }
	}
    }

  if (bytes_written)
    *bytes_written = outp - dest;	/* Doesn't include '\0' */

  if (have_error)
    {
      n_free (dest);
      return NULL;
    }
  else
    return dest;
}

/**
 * n_convert:
 * @str:           (array length=len) (element-type guint8):
 *                 the string to convert.
 * @len:           the length of the string in bytes, or -1 if the string is
 *                 nul-terminated (Note that some encodings may allow nul
 *                 bytes to occur inside strings. In that case, using -1
 *                 for the @len parameter is unsafe)
 * @to_codeset:    name of character set into which to convert @str
 * @from_codeset:  character set of @str.
 * @bytes_read:    (out) (optional): location to store the number of bytes in
 *                 the input string that were successfully converted, or %NULL.
 *                 Even if the conversion was successful, this may be
 *                 less than @len if there were partial characters
 *                 at the end of the input. If the error
 *                 #N_CONVERT_ERROR_ILLEGAL_SEQUENCE occurs, the value
 *                 stored will be the byte offset after the last valid
 *                 input sequence.
 * @bytes_written: (out) (optional): the number of bytes stored in
 *                 the output buffer (not including the terminating nul).
 * @error:         location to store the error occurring, or %NULL to ignore
 *                 errors. Any of the errors in #NConvertError may occur.
 *
 * Converts a string from one character set to another.
 *
 * Note that you should use n_iconv() for streaming conversions.
 * Despite the fact that @bytes_read can return information about partial
 * characters, the n_convert_... functions are not generally suitable
 * for streaming. If the underlying converter maintains internal state,
 * then this won't be preserved across successive calls to n_convert(),
 * n_convert_with_iconv() or n_convert_with_fallback(). (An example of
 * this is the GNU C converter for CP1255 which does not emit a base
 * character until it knows that the next character is not a mark that
 * could combine with the base character.)
 *
 * Using extensions such as "//TRANSLIT" may not work (or may not work
 * well) on many platforms.  Consider using n_str_to_ascii() instead.
 *
 * Returns: (array length=bytes_written) (element-type guint8) (transfer full):
 *          If the conversion was successful, a newly allocated buffer
 *          containing the converted string, which must be freed with n_free().
 *          Otherwise %NULL and @error will be set.
 **/
nchar* n_convert (const nchar *str,nssize len,const nchar *to_codeset,const nchar *from_codeset,
           nsize *bytes_read,nsize *bytes_written, NError **error)
{
  nchar *res;
  NIConv cd;
  printf("n_convert--- 00 %s %s %s\n",str,to_codeset,from_codeset);

  n_return_val_if_fail (str != NULL, NULL);
  n_return_val_if_fail (to_codeset != NULL, NULL);
  n_return_val_if_fail (from_codeset != NULL, NULL);
  printf("n_convert--- 11 %s %s %s\n",str,to_codeset,from_codeset);

  cd = open_converter (to_codeset, from_codeset, error);
  printf("n_convert--- 22 %s %s %s %p\n",str,to_codeset,from_codeset,cd);

  if (cd == (NIConv) -1)
    {
      if (bytes_read)
        *bytes_read = 0;

      if (bytes_written)
        *bytes_written = 0;
      printf("n_convert--- 33 %s %s %s %p\n",str,to_codeset,from_codeset,cd);

      return NULL;
    }

  res = n_convert_with_iconv (str, len, cd,bytes_read, bytes_written,error);
  printf("n_convert--- 44 %s %s %s %p\n",str,to_codeset,from_codeset,cd);

  close_converter (cd);
  printf("n_convert--- 55 %s %s %s %p\n",str,to_codeset,from_codeset,cd);

  return res;
}

/**
 * n_convert_with_fallback:
 * @str:          (array length=len) (element-type guint8):
 *                the string to convert.
 * @len:          the length of the string in bytes, or -1 if the string is
 *                 nul-terminated (Note that some encodings may allow nul
 *                 bytes to occur inside strings. In that case, using -1
 *                 for the @len parameter is unsafe)
 * @to_codeset:   name of character set into which to convert @str
 * @from_codeset: character set of @str.
 * @fallback:     UTF-8 string to use in place of characters not
 *                present in the target encoding. (The string must be
 *                representable in the target encoding).
 *                If %NULL, characters not in the target encoding will
 *                be represented as Unicode escapes \uxxxx or \Uxxxxyyyy.
 * @bytes_read:   (out) (optional): location to store the number of bytes in
 *                the input string that were successfully converted, or %NULL.
 *                Even if the conversion was successful, this may be
 *                less than @len if there were partial characters
 *                at the end of the input.
 * @bytes_written: (out) (optional): the number of bytes stored in
 *                 the output buffer (not including the terminating nul).
 * @error:        location to store the error occurring, or %NULL to ignore
 *                errors. Any of the errors in #NConvertError may occur.
 *
 * Converts a string from one character set to another, possibly
 * including fallback sequences for characters not representable
 * in the output. Note that it is not guaranteed that the specification
 * for the fallback sequences in @fallback will be honored. Some
 * systems may do an approximate conversion from @from_codeset
 * to @to_codeset in their iconv() functions,
 * in which case GLib will simply return that approximate conversion.
 *
 * Note that you should use n_iconv() for streaming conversions.
 * Despite the fact that @bytes_read can return information about partial
 * characters, the n_convert_... functions are not generally suitable
 * for streaming. If the underlying converter maintains internal state,
 * then this won't be preserved across successive calls to n_convert(),
 * n_convert_with_iconv() or n_convert_with_fallback(). (An example of
 * this is the GNU C converter for CP1255 which does not emit a base
 * character until it knows that the next character is not a mark that
 * could combine with the base character.)
 *
 * Returns: (array length=bytes_written) (element-type guint8) (transfer full):
 *          If the conversion was successful, a newly allocated buffer
 *          containing the converted string, which must be freed with n_free().
 *          Otherwise %NULL and @error will be set.
 **/
nchar* n_convert_with_fallback (const nchar *str,
			 nssize       len,
			 const nchar *to_codeset,
			 const nchar *from_codeset,
			 const nchar *fallback,
			 nsize       *bytes_read,
			 nsize       *bytes_written,
			 NError     **error)
{
  nchar *utf8;
  nchar *dest;
  nchar *outp;
  const nchar *insert_str = NULL;
  const nchar *p;
  nsize inbytes_remaining;
  const nchar *save_p = NULL;
  nsize save_inbytes = 0;
  nsize outbytes_remaining;
  nsize err;
  NIConv cd;
  nsize outbuf_size;
  nboolean have_error = FALSE;
  nboolean done = FALSE;

  NError *local_error = NULL;
   printf("n_convert_with_fallback--- 00 %s %s %s\n",str,to_codeset,from_codeset);
  n_return_val_if_fail (str != NULL, NULL);
  n_return_val_if_fail (to_codeset != NULL, NULL);
  n_return_val_if_fail (from_codeset != NULL, NULL);
  printf("n_convert_with_fallback--- 11 %s %s %s\n",str,to_codeset,from_codeset);

  if (len < 0)
    len = strlen (str);
  printf("n_convert_with_fallback--- 22 %s %s %s\n",str,to_codeset,from_codeset);

  /* Try an exact conversion; we only proceed if this fails
   * due to an illegal sequence in the input string.
   */
  dest = n_convert (str, len, to_codeset, from_codeset,bytes_read, bytes_written, &local_error);
  printf("n_convert_with_fallback--- 33 %s %s %s\n",str,to_codeset,from_codeset);

  if (!local_error)
    return dest;

  if (!n_error_matches (local_error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE))
    {
      n_propagate_error (error, local_error);
      return NULL;
    }
  else
    n_error_free (local_error);

  local_error = NULL;

  /* No go; to proceed, we need a converter from "UTF-8" to
   * to_codeset, and the string as UTF-8.
   */
  cd = open_converter (to_codeset, "UTF-8", error);
  if (cd == (NIConv) -1)
    {
      if (bytes_read)
        *bytes_read = 0;

      if (bytes_written)
        *bytes_written = 0;

      return NULL;
    }

  utf8 = n_convert (str, len, "UTF-8", from_codeset,bytes_read, &inbytes_remaining, error);
  if (!utf8){
      close_converter (cd);
      if (bytes_written)
        *bytes_written = 0;
      return NULL;
  }

  /* Now the heart of the code. We loop through the UTF-8 string, and
   * whenever we hit an offending character, we form fallback, convert
   * the fallback to the target codeset, and then go back to
   * converting the original string after finishing with the fallback.
   *
   * The variables save_p and save_inbytes store the input state
   * for the original string while we are converting the fallback
   */
  p = utf8;

  outbuf_size = len + NUL_TERMINATOR_LENGTH;
  outbytes_remaining = outbuf_size - NUL_TERMINATOR_LENGTH;
  outp = dest = n_malloc (outbuf_size);

  while (!done && !have_error)
    {
      nsize inbytes_tmp = inbytes_remaining;
      err = n_iconv (cd, (char **)&p, &inbytes_tmp, &outp, &outbytes_remaining);
      inbytes_remaining = inbytes_tmp;

      if (err == (nsize) -1){
	  switch (errno)
	    {
	    case EINVAL:
	      n_assert_not_reached();
	      break;
	    case E2BIG:
	      {
		nsize used = outp - dest;

		outbuf_size *= 2;
		dest = n_realloc (dest, outbuf_size);

		outp = dest + used;
		outbytes_remaining = outbuf_size - used - NUL_TERMINATOR_LENGTH;

		break;
	      }
	    case EILSEQ:
	      if (save_p)
		{
		  /* Error converting fallback string - fatal
		   */
		  n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE,
			       "Cannot convert fallback “%s” to codeset “%s”",
			       insert_str, to_codeset);
		  have_error = TRUE;
		  break;
		}
	      else if (p)
		{
		  if (!fallback)
		    {
		      nunichar ch = n_utf8_get_char (p);
		      insert_str = n_strdup_printf (ch < 0x10000 ? "\\u%04x" : "\\U%08x",
						    ch);
		    }
		  else
		    insert_str = fallback;

		  save_p = n_utf8_next_char (p);
		  save_inbytes = inbytes_remaining - (save_p - p);
		  p = insert_str;
		  inbytes_remaining = strlen (p);
		  break;
		}
              /* if p is null */
              N_GNUC_FALLTHROUGH;
	    default:
              {
                int errsv = errno;

                n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_FAILED,
                             "Error during conversion: %s",
                             n_strerror (errsv));
              }

	      have_error = TRUE;
	      break;
	    }
	}
      else
	{
	  if (save_p)
	    {
	      if (!fallback)
		n_free ((nchar *)insert_str);
	      p = save_p;
	      inbytes_remaining = save_inbytes;
	      save_p = NULL;
	    }
	  else if (p)
	    {
	      /* call n_iconv with NULL inbuf to cleanup shift state */
	      p = NULL;
	      inbytes_remaining = 0;
	    }
	  else
	    done = TRUE;
	}
    }

  /* Cleanup
   */
  memset (outp, 0, NUL_TERMINATOR_LENGTH);

  close_converter (cd);

  if (bytes_written)
    *bytes_written = outp - dest;	/* Doesn't include '\0' */

  n_free (utf8);

  if (have_error)
    {
      if (save_p && !fallback)
	n_free ((nchar *)insert_str);
      n_free (dest);
      return NULL;
    }
  else
    return dest;
}

/*
 * n_locale_to_utf8
 *
 *
 */

/*
 * Validate @string as UTF-8. @len can be negative if @string is
 * nul-terminated, or a non-negative value in bytes. If @string ends in an
 * incomplete sequence, or contains any illegal sequences or nul codepoints,
 * %NULL will be returned and the error set to
 * %N_CONVERT_ERROR_ILLEGAL_SEQUENCE.
 * On success, @bytes_read and @bytes_written, if provided, will be set to
 * the number of bytes in @string up to @len or the terminating nul byte.
 * On error, @bytes_read will be set to the byte offset after the last valid
 * and non-nul UTF-8 sequence in @string, and @bytes_written will be set to 0.
 */
static nchar *strdup_len (const nchar *string,
	    nssize       len,
	    nsize       *bytes_read,
	    nsize       *bytes_written,
	    NError     **error)
{
  nsize real_len;
  const nchar *end_valid;

  if (!n_utf8_validate (string, len, &end_valid))
    {
      if (bytes_read)
	*bytes_read = end_valid - string;
      if (bytes_written)
	*bytes_written = 0;

      n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                           "Invalid byte sequence in conversion input");
      return NULL;
    }

  real_len = end_valid - string;

  if (bytes_read)
    *bytes_read = real_len;
  if (bytes_written)
    *bytes_written = real_len;

  return n_strndup (string, real_len);
}

typedef enum
{
  CONVERT_CHECK_NO_NULS_IN_INPUT  = 1 << 0,
  CONVERT_CHECK_NO_NULS_IN_OUTPUT = 1 << 1
} ConvertCheckFlags;

/*
 * Convert from @string in the encoding identified by @from_codeset,
 * returning a string in the encoding identifed by @to_codeset.
 * @len can be negative if @string is nul-terminated, or a non-negative
 * value in bytes. Flags defined in #ConvertCheckFlags can be set in @flags
 * to check the input, the output, or both, for embedded nul bytes.
 * On success, @bytes_read, if provided, will be set to the number of bytes
 * in @string up to @len or the terminating nul byte, and @bytes_written, if
 * provided, will be set to the number of output bytes written into the
 * returned buffer, excluding the terminating nul sequence.
 * On error, @bytes_read will be set to the byte offset after the last valid
 * sequence in @string, and @bytes_written will be set to 0.
 */
static nchar *convert_checked (const nchar      *string,
                 nssize            len,
                 const nchar      *to_codeset,
                 const nchar      *from_codeset,
                 ConvertCheckFlags flags,
                 nsize            *bytes_read,
                 nsize            *bytes_written,
                 NError          **error)
{
  nchar *out;
  nsize outbytes;

  if ((flags & CONVERT_CHECK_NO_NULS_IN_INPUT) && len > 0)
    {
      const nchar *early_nul = memchr (string, '\0', len);
      if (early_nul != NULL)
        {
          if (bytes_read)
            *bytes_read = early_nul - string;
          if (bytes_written)
            *bytes_written = 0;

          n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                               "Embedded NUL byte in conversion input");
          return NULL;
        }
    }

  out = n_convert (string, len, to_codeset, from_codeset,
                   bytes_read, &outbytes, error);
  if (out == NULL)
    {
      if (bytes_written)
        *bytes_written = 0;
      return NULL;
    }

  if ((flags & CONVERT_CHECK_NO_NULS_IN_OUTPUT)
      && memchr (out, '\0', outbytes) != NULL)
    {
      n_free (out);
      if (bytes_written)
        *bytes_written = 0;
      n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_EMBEDDED_NUL,
                           "Embedded NUL byte in conversion output");
      return NULL;
    }

  if (bytes_written)
    *bytes_written = outbytes;
  return out;
}


nchar *n_locale_to_utf8 (const nchar  *opsysstring,
		  nssize        len,
		  nsize        *bytes_read,
		  nsize        *bytes_written,
		  NError      **error)
{
  const char *charset;

  if (n_get_charset (&charset))
    return strdup_len (opsysstring, len, bytes_read, bytes_written, error);
  else
    return convert_checked (opsysstring, len, "UTF-8", charset,
                            CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                            bytes_read, bytes_written, error);
}


nchar *n_locale_from_utf8 (const nchar *utf8string,
		    nssize       len,
		    nsize       *bytes_read,
		    nsize       *bytes_written,
		    NError     **error)
{
  const nchar *charset;

  if (n_get_charset (&charset))
    return strdup_len (utf8string, len, bytes_read, bytes_written, error);
  else
    return convert_checked (utf8string, len, charset, "UTF-8",
                            CONVERT_CHECK_NO_NULS_IN_INPUT,
                            bytes_read, bytes_written, error);
}


typedef struct _GFilenameCharsetCache GFilenameCharsetCache;

struct _GFilenameCharsetCache {
  nboolean is_utf8;
  nchar *charset;
  nchar **filename_charsets;
};

static void filename_charset_cache_free (npointer data)
{
  GFilenameCharsetCache *cache = data;
  n_free (cache->charset);
  n_strfreev (cache->filename_charsets);
  n_free (cache);
}


static const nchar *n_env_get_env (const nchar *variable)
{
  n_return_val_if_fail (variable != NULL, NULL);
  return getenv (variable);
}


nboolean n_get_filename_charsets (const nchar ***filename_charsets)
{
 // static NThreadSpecific cache_private = N_THREAD_SPECIFIC_INIT (filename_charset_cache_free);

  static GFilenameCharsetCache *cache =NULL;// n_thread_specific_get (&cache_private);
  const nchar *charset;

  if (!cache)
    cache = n_malloc0(sizeof (GFilenameCharsetCache));//n_thread_specific_set_alloc0 (&cache_private, sizeof (GFilenameCharsetCache));

  n_get_charset (&charset);

  if (!(cache->charset && strcmp (cache->charset, charset) == 0))
    {
      const nchar *new_charset;
      const nchar *p;
      nint i;

      n_free (cache->charset);
      n_strfreev (cache->filename_charsets);
      cache->charset = n_strdup (charset);

      p = n_env_get_env ("N_FILENAME_ENCODING");
      if (p != NULL && p[0] != '\0')
	{
	  cache->filename_charsets = n_strsplit (p, ",", 0);
	  cache->is_utf8 = (strcmp (cache->filename_charsets[0], "UTF-8") == 0);

	  for (i = 0; cache->filename_charsets[i]; i++)
	    {
	      if (strcmp ("@locale", cache->filename_charsets[i]) == 0)
		{
		  n_get_charset (&new_charset);
		  n_free (cache->filename_charsets[i]);
		  cache->filename_charsets[i] = n_strdup (new_charset);
		}
	    }
	}
      else if (n_env_get_env ("N_BROKEN_FILENAMES") != NULL)
	{
	  cache->filename_charsets = n_new0 (nchar *, 2);
	  cache->is_utf8 = n_get_charset (&new_charset);
	  cache->filename_charsets[0] = n_strdup (new_charset);
	}
      else
	{
	  cache->filename_charsets = n_new0 (nchar *, 3);
	  cache->is_utf8 = TRUE;
	  cache->filename_charsets[0] = n_strdup ("UTF-8");
	  if (!n_get_charset (&new_charset))
	    cache->filename_charsets[1] = n_strdup (new_charset);
	}
    }

  if (filename_charsets)
    *filename_charsets = (const nchar **)cache->filename_charsets;

  return cache->is_utf8;
}



static nboolean get_filename_charset (const nchar **filename_charset)
{
  const nchar **charsets;
  nboolean is_utf8;

  is_utf8 = n_get_filename_charsets (&charsets);

  if (filename_charset)
    *filename_charset = charsets[0];

  return is_utf8;
}


nchar* n_filename_to_utf8 (const nchar *opsysstring,
		    nssize       len,
		    nsize       *bytes_read,
		    nsize       *bytes_written,
		    NError     **error)
{
  const nchar *charset;

  n_return_val_if_fail (opsysstring != NULL, NULL);

  if (get_filename_charset (&charset))
    return strdup_len (opsysstring, len, bytes_read, bytes_written, error);
  else
    return convert_checked (opsysstring, len, "UTF-8", charset,
                            CONVERT_CHECK_NO_NULS_IN_INPUT |
                            CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                            bytes_read, bytes_written, error);
}


nchar* n_filename_from_utf8 (const nchar *utf8string,
		      nssize       len,
		      nsize       *bytes_read,
		      nsize       *bytes_written,
		      NError     **error)
{
  const nchar *charset;

  if (get_filename_charset (&charset))
    return strdup_len (utf8string, len, bytes_read, bytes_written, error);
  else
    return convert_checked (utf8string, len, charset, "UTF-8",
                            CONVERT_CHECK_NO_NULS_IN_INPUT |
                            CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                            bytes_read, bytes_written, error);
}

/* Test of haystack has the needle prefix, comparing case
 * insensitive. haystack may be UTF-8, but needle must
 * contain only ascii. */
static nboolean has_case_prefix (const nchar *haystack, const nchar *needle)
{
  const nchar *h, *n;

  /* Eat one character at a time. */
  h = haystack;
  n = needle;

  while (*n && *h &&
	 n_ascii_tolower (*n) == n_ascii_tolower (*h))
    {
      n++;
      h++;
    }

  return *n == '\0';
}

typedef enum {
  UNSAFE_ALL        = 0x1,  /* Escape all unsafe characters   */
  UNSAFE_ALLOW_PLUS = 0x2,  /* Allows '+'  */
  UNSAFE_PATH       = 0x8,  /* Allows '/', '&', '=', ':', '@', '+', '$' and ',' */
  UNSAFE_HOST       = 0x10, /* Allows '/' and ':' and '@' */
  UNSAFE_SLASHES    = 0x20  /* Allows all characters except for '/' and '%' */
} UnsafeCharacterSet;

static const nuchar acceptable[96] = {
  /* A table of the ASCII chars from space (32) to DEL (127) */
  /*      !    "    #    $    %    &    '    (    )    *    +    ,    -    .    / */
  0x00,0x3F,0x20,0x20,0x28,0x00,0x2C,0x3F,0x3F,0x3F,0x3F,0x2A,0x28,0x3F,0x3F,0x1C,
  /* 0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ? */
  0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x38,0x20,0x20,0x2C,0x20,0x20,
  /* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
  0x38,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
  /* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
  0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x20,0x3F,
  /* `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o */
  0x20,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
  /* p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~  DEL */
  0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x3F,0x20
};

static const nchar hex[16] = "0123456789ABCDEF";

/* Note: This escape function works on file: URIs, but if you want to
 * escape something else, please read RFC-2396 */
static nchar *n_escape_uri_string (const nchar *string,UnsafeCharacterSet mask)
{
#define ACCEPTABLE(a) ((a)>=32 && (a)<128 && (acceptable[(a)-32] & use_mask))

  const nchar *p;
  nchar *q;
  nchar *result;
  int c;
  nint unacceptable;
  UnsafeCharacterSet use_mask;

  n_return_val_if_fail (mask == UNSAFE_ALL
			|| mask == UNSAFE_ALLOW_PLUS
			|| mask == UNSAFE_PATH
			|| mask == UNSAFE_HOST
			|| mask == UNSAFE_SLASHES, NULL);

  unacceptable = 0;
  use_mask = mask;
  for (p = string; *p != '\0'; p++)
    {
      c = (nuchar) *p;
      if (!ACCEPTABLE (c))
	unacceptable++;
    }

  result = n_malloc (p - string + unacceptable * 2 + 1);

  use_mask = mask;
  for (q = result, p = string; *p != '\0'; p++)
    {
      c = (nuchar) *p;

      if (!ACCEPTABLE (c))
	{
	  *q++ = '%'; /* means hex coming */
	  *q++ = hex[c >> 4];
	  *q++ = hex[c & 15];
	}
      else
	*q++ = *p;
    }

  *q = '\0';

  return result;
}


static nchar *n_escape_file_uri (const nchar *hostname,const nchar *pathname)
{
  char *escaped_hostname = NULL;
  char *escaped_path;
  char *res;


  if (hostname && *hostname != '\0')
    {
      escaped_hostname = n_escape_uri_string (hostname, UNSAFE_HOST);
    }

  escaped_path = n_escape_uri_string (pathname, UNSAFE_PATH);

  res = n_strconcat ("file://",
		     (escaped_hostname) ? escaped_hostname : "",
		     (*escaped_path != '/') ? "/" : "",
		     escaped_path,
		     NULL);


  n_free (escaped_hostname);
  n_free (escaped_path);

  return res;
}

static int unescape_character (const char *scanner)
{
  int first_digit;
  int second_digit;

  first_digit = n_ascii_xdigit_value (scanner[0]);
  if (first_digit < 0)
    return -1;

  second_digit = n_ascii_xdigit_value (scanner[1]);
  if (second_digit < 0)
    return -1;

  return (first_digit << 4) | second_digit;
}

static nchar *n_unescape_uri_string (const char *escaped,
		       int         len,
		       const char *illegal_escaped_characters,
		       nboolean    ascii_must_not_be_escaped)
{
  const nchar *in, *in_end;
  nchar *out, *result;
  int c;

  if (escaped == NULL)
    return NULL;

  if (len < 0)
    len = strlen (escaped);

  result = n_malloc (len + 1);

  out = result;
  for (in = escaped, in_end = escaped + len; in < in_end; in++)
    {
      c = *in;

      if (c == '%')
	{
	  /* catch partial escape sequences past the end of the substring */
	  if (in + 3 > in_end)
	    break;

	  c = unescape_character (in + 1);

	  /* catch bad escape sequences and NUL characters */
	  if (c <= 0)
	    break;

	  /* catch escaped ASCII */
	  if (ascii_must_not_be_escaped && c <= 0x7F)
	    break;

	  /* catch other illegal escaped characters */
	  if (strchr (illegal_escaped_characters, c) != NULL)
	    break;

	  in += 2;
	}

      *out++ = c;
    }

  n_assert (out - result <= len);
  *out = '\0';

  if (in != in_end)
    {
      n_free (result);
      return NULL;
    }

  return result;
}

static nboolean is_asciialphanum (nunichar c)
{
  return c <= 0x7F && n_ascii_isalnum (c);
}

static nboolean is_asciialpha (nunichar c)
{
  return c <= 0x7F && n_ascii_isalpha (c);
}

/* allows an empty string */
static nboolean hostname_validate (const char *hostname)
{
  const char *p;
  nunichar c, first_char, last_char;

  p = hostname;
  if (*p == '\0')
    return TRUE;
  do
    {
      /* read in a label */
      c = n_utf8_get_char (p);
      p = n_utf8_next_char (p);
      if (!is_asciialphanum (c))
	return FALSE;
      first_char = c;
      do
	{
	  last_char = c;
	  c = n_utf8_get_char (p);
	  p = n_utf8_next_char (p);
	}
      while (is_asciialphanum (c) || c == '-');
      if (last_char == '-')
	return FALSE;

      /* if that was the last label, check that it was a toplabel */
      if (c == '\0' || (c == '.' && *p == '\0'))
	return is_asciialpha (first_char);
    }
  while (c == '.');
  return FALSE;
}

/**
 * n_filename_from_uri:
 * @uri: a uri describing a filename (escaped, encoded in ASCII).
 * @hostname: (out) (optional) (nullable): Location to store hostname for the URI.
 *            If there is no hostname in the URI, %NULL will be
 *            stored in this location.
 * @error: location to store the error occurring, or %NULL to ignore
 *         errors. Any of the errors in #NConvertError may occur.
 *
 * Converts an escaped ASCII-encoded URI to a local filename in the
 * encoding used for filenames.
 *
 * Returns: (type filename): a newly-allocated string holding
 *               the resulting filename, or %NULL on an error.
 **/
nchar *n_filename_from_uri (const nchar *uri,
		     nchar      **hostname,
		     NError     **error)
{
  const char *path_part;
  const char *host_part;
  char *unescaped_hostname;
  char *result;
  char *filename;
  int offs;


  if (hostname)
    *hostname = NULL;

  if (!has_case_prefix (uri, "file:/"))
    {
      n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_BAD_URI,
		   "The URI “%s” is not an absolute URI using the “file” scheme",
		   uri);
      return NULL;
    }

  path_part = uri + strlen ("file:");

  if (strchr (path_part, '#') != NULL)
    {
      n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_BAD_URI,
		   "The local file URI “%s” may not include a “#”",
		   uri);
      return NULL;
    }

  if (has_case_prefix (path_part, "///"))
    path_part += 2;
  else if (has_case_prefix (path_part, "//"))
    {
      path_part += 2;
      host_part = path_part;

      path_part = strchr (path_part, '/');

      if (path_part == NULL)
	{
	  n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_BAD_URI,
		       "The URI “%s” is invalid",
		       uri);
	  return NULL;
	}

      unescaped_hostname = n_unescape_uri_string (host_part, path_part - host_part, "", TRUE);

      if (unescaped_hostname == NULL ||
	  !hostname_validate (unescaped_hostname))
	{
	  n_free (unescaped_hostname);
	  n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_BAD_URI,
		       "The hostname of the URI “%s” is invalid",
		       uri);
	  return NULL;
	}

      if (hostname)
	*hostname = unescaped_hostname;
      else
	n_free (unescaped_hostname);
    }

  filename = n_unescape_uri_string (path_part, -1, "/", FALSE);

  if (filename == NULL)
    {
      n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_BAD_URI,
		   "The URI “%s” contains invalidly escaped characters",
		   uri);
      return NULL;
    }

  offs = 0;
  result = n_strdup (filename + offs);
  n_free (filename);

  return result;
}


nchar *n_filename_to_uri (const nchar *filename,const nchar *hostname,NError     **error)
{
  char *escaped_uri=NULL;

// zclei n_return_val_if_fail (filename != NULL, NULL);
//
//  if (!n_path_is_absolute (filename))
//    {
//      n_set_error (error, N_CONVERT_ERROR, N_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
//		   "The pathname “%s” is not an absolute path",
//		   filename);
//      return NULL;
//    }
//
//  if (hostname && !(n_utf8_validate (hostname, -1, NULL) && hostname_validate (hostname)))
//    {
//      n_set_error_literal (error, N_CONVERT_ERROR, N_CONVERT_ERROR_ILLEGAL_SEQUENCE,
//                           "Invalid hostname");
//      return NULL;
//    }
//
//
//  escaped_uri = n_escape_file_uri (hostname, filename);

  return escaped_uri;
}


nchar **n_uri_list_extract_uris (const nchar *uri_list)
{
  NPtrArray *uris;
  const nchar *p, *q;

  uris = n_ptr_array_new ();

  p = uri_list;

  /* We don't actually try to validate the URI according to RFC
   * 2396, or even check for allowed characters - we just ignore
   * comments and trim whitespace off the ends.  We also
   * allow LF delimination as well as the specified CRLF.
   *
   * We do allow comments like specified in RFC 2483.
   */
  while (p)
    {
      if (*p != '#')
	{
	  while (n_ascii_isspace (*p))
	    p++;

	  q = p;
	  while (*q && (*q != '\n') && (*q != '\r'))
	    q++;

	  if (q > p)
	    {
	      q--;
	      while (q > p && n_ascii_isspace (*q))
		q--;

	      if (q > p)
                n_ptr_array_add (uris, n_strndup (p, q - p + 1));
            }
        }
      p = strchr (p, '\n');
      if (p)
	p++;
    }

  n_ptr_array_add (uris, NULL);

  return (nchar **) n_ptr_array_free (uris, FALSE);
}


nchar *n_filename_display_basename (const nchar *filename)
{
  char *basename;
  char *display_name;

  n_return_val_if_fail (filename != NULL, NULL);

  basename = n_path_get_basename (filename);
  display_name = n_filename_display_name (basename);
  n_free (basename);
  return display_name;
}


nchar *n_filename_display_name (const nchar *filename)
{
  nint i;
  const nchar **charsets;
  nchar *display_name = NULL;
  nboolean is_utf8;

  is_utf8 = n_get_filename_charsets (&charsets);

  if (is_utf8)
    {
      if (n_utf8_validate (filename, -1, NULL))
	display_name = n_strdup (filename);
    }

  if (!display_name)
    {
      /* Try to convert from the filename charsets to UTF-8.
       * Skip the first charset if it is UTF-8.
       */
      for (i = is_utf8 ? 1 : 0; charsets[i]; i++)
	{
	  display_name = n_convert (filename, -1, "UTF-8", charsets[i],
				    NULL, NULL, NULL);

	  if (display_name)
	    break;
	}
    }

  /* if all conversions failed, we replace invalid UTF-8
   * by a question mark
   */
  if (!display_name)
    display_name = n_utf8_make_valid (filename, -1);

  return display_name;
}


