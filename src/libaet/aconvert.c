#include <iconv.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aconvert.h"
#include "alog.h"
#include "aerror.h"
#include "acharset.h"
#include "aunicode.h"
#include "amem.h"
#include "astrfuncs.h"



#define NUL_TERMINATOR_LENGTH 4
#define A_CONVERT_ERROR_DOMAIN 4

static aboolean try_conversion (const char *to_codeset,
		const char *from_codeset,
		iconv_t    *cd)
{
  *cd = iconv_open (to_codeset, from_codeset);

  if (*cd == (iconv_t)-1 && errno == EINVAL)
    return FALSE;
  else
    return TRUE;
}

static aboolean try_to_aliases (const char **to_aliases,
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


AIConv a_iconv_open (const achar  *to_codeset,const achar  *from_codeset)
{
  iconv_t cd;
  if (!try_conversion (to_codeset, from_codeset, &cd)){
      const char **to_aliases =   _a_charset_get_aliases (to_codeset);
      const char **from_aliases = _a_charset_get_aliases (from_codeset);

      if (from_aliases){
	    const char **p = from_aliases;
	    while (*p){
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
   return (cd == (iconv_t)-1) ? (AIConv)-1 : (AIConv)cd;
}


asize a_iconv (AIConv   converter,
	 achar  **inbuf,
	 asize   *inbytes_left,
	 achar  **outbuf,
	 asize   *outbytes_left)
{
  iconv_t cd = (iconv_t)converter;
  return iconv (cd, inbuf, inbytes_left, outbuf, outbytes_left);
}

/**
 * a_iconv_close: (skip)
 * @converter: a conversion descriptor from a_iconv_open()
 *
 * Same as the standard UNIX routine iconv_close(), but
 * may be implemented via libiconv on UNIX flavors that lack
 * a native implementation. Should be called to clean up
 * the conversion descriptor from a_iconv_open() when
 * you are done converting things.
 *
 * GLib provides a_convert() and a_locale_to_utf8() which are likely
 * more convenient than the raw iconv wrappers.
 *
 * Returns: -1 on error, 0 on success
 **/
aint a_iconv_close (AIConv converter)
{
  iconv_t cd = (iconv_t)converter;
  return iconv_close (cd);
}

static AIConv opea_converter (const achar *to_codeset,const achar *from_codeset,AError **error)
{
  AIConv cd;

  cd = a_iconv_open (to_codeset, from_codeset);

  if (cd == (AIConv) -1)
    {
      /* Something went wrong.  */
      if (error)
	{
	  if (errno == EINVAL)
		  a_error_printf (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_NO_CONVERSION,
			 "Conversion from character set “%s” to “%s” is not supported",
			 from_codeset, to_codeset);
	  else
		  a_error_printf (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_FAILED,
			 "Could not open converter from “%s” to “%s”",
			 from_codeset, to_codeset);
	}
    }

  return cd;
}

static int close_converter (AIConv cd)
{
  if (cd == (AIConv) -1)
    return 0;

  return a_iconv_close (cd);
}


achar* a_convert_with_iconv (const achar *str,
		      assize       len,
		      AIConv       converter,
		      asize       *bytes_read,
		      asize       *bytes_written,
		      AError     **error)
{
  achar *dest;
  achar *outp;
  const achar *p;
  asize inbytes_remaining;
  asize outbytes_remaining;
  asize err;
  asize outbuf_size;
  aboolean have_error = FALSE;
  aboolean done = FALSE;
  aboolean reset = FALSE;

  a_return_val_if_fail (converter != (AIConv) -1, NULL);

  if (len < 0)
    len = strlen (str);

  p = str;
  inbytes_remaining = len;
  outbuf_size = len + NUL_TERMINATOR_LENGTH;

  outbytes_remaining = outbuf_size - NUL_TERMINATOR_LENGTH;
  outp = dest = a_malloc (outbuf_size);

  while (!done && !have_error){
      if (reset)
        err = a_iconv (converter, NULL, &inbytes_remaining, &outp, &outbytes_remaining);
      else
        err = a_iconv (converter, (char **)&p, &inbytes_remaining, &outp, &outbytes_remaining);

      if (err == (asize) -1){
	  switch (errno)
	    {
	    case EINVAL:
	      /* Incomplete text, do not report an error */
	      done = TRUE;
	      break;
	    case E2BIG:
	      {
		    asize used = outp - dest;
		    outbuf_size *= 2;
		    dest = a_realloc (dest, outbuf_size);
		    outp = dest + used;
		    outbytes_remaining = outbuf_size - used - NUL_TERMINATOR_LENGTH;
	      }
	      break;
	    case EILSEQ:
	    	a_error_set (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,"Invalid byte sequence in conversion input");
	      have_error = TRUE;
	      break;
	    default:
              {
                int errsv = errno;
                char buffer[1024];
                a_strerror (errsv,buffer);
                a_error_printf (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_FAILED,"Error during conversion: %s",buffer);
              }
	      have_error = TRUE;
	      break;
	    }
	}
      else if (err > 0)
        {
          /* @err gives the number of replacement characters used. */
    	  a_error_set (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                               "Unrepresentable character in conversion input");
          have_error = TRUE;
        }
      else
	{
	  if (!reset)
	    {
	      /* call a_iconv with NULL inbuf to cleanup shift state */
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
        	  a_error_set (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_PARTIAL_INPUT,
                                   "Partial character sequence at end of input");
              have_error = TRUE;
            }
	}
    }

  if (bytes_written)
    *bytes_written = outp - dest;	/* Doesn't include '\0' */

  if (have_error)
    {
      a_free (dest);
      return NULL;
    }
  else
    return dest;
}

/**
 * a_convert:
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
 *                 #A_CONVERT_ERROR_ILLEGAL_SEQUENCE occurs, the value
 *                 stored will be the byte offset after the last valid
 *                 input sequence.
 * @bytes_written: (out) (optional): the number of bytes stored in
 *                 the output buffer (not including the terminating nul).
 * @error:         location to store the error occurring, or %NULL to ignore
 *                 errors. Any of the errors in #NConvertError may occur.
 *
 * Converts a string from one character set to another.
 *
 * Note that you should use a_iconv() for streaming conversions.
 * Despite the fact that @bytes_read can return information about partial
 * characters, the a_convert_... functions are not generally suitable
 * for streaming. If the underlying converter maintains internal state,
 * then this won't be preserved across successive calls to a_convert(),
 * a_convert_with_iconv() or a_convert_with_fallback(). (An example of
 * this is the GNU C converter for CP1255 which does not emit a base
 * character until it knows that the next character is not a mark that
 * could combine with the base character.)
 *
 * Using extensions such as "//TRANSLIT" may not work (or may not work
 * well) on many platforms.  Consider using a_str_to_ascii() instead.
 *
 * Returns: (array length=bytes_written) (element-type guint8) (transfer full):
 *          If the conversion was successful, a newly allocated buffer
 *          containing the converted string, which must be freed with a_free().
 *          Otherwise %NULL and @error will be set.
 **/
achar* a_convert (const achar *str,assize len,const achar *to_codeset,const achar *from_codeset,
           asize *bytes_read,asize *bytes_written, AError **error)
{
  achar *res;
  AIConv cd;
  a_return_val_if_fail (str != NULL, NULL);
  a_return_val_if_fail (to_codeset != NULL, NULL);
  a_return_val_if_fail (from_codeset != NULL, NULL);
  cd = opea_converter (to_codeset, from_codeset, error);
  if (cd == (AIConv) -1)
    {
      if (bytes_read)
        *bytes_read = 0;

      if (bytes_written)
        *bytes_written = 0;
      return NULL;
    }

  res = a_convert_with_iconv (str, len, cd,bytes_read, bytes_written,error);
  close_converter (cd);
  return res;
}


achar* a_convert_with_fallback (const achar *str,
			 assize       len,
			 const achar *to_codeset,
			 const achar *from_codeset,
			 const achar *fallback,
			 asize       *bytes_read,
			 asize       *bytes_written,
			 AError     **error)
{
  achar *utf8;
  achar *dest;
  achar *outp;
  const achar *insert_str = NULL;
  const achar *p;
  asize inbytes_remaining;
  const achar *save_p = NULL;
  asize save_inbytes = 0;
  asize outbytes_remaining;
  asize err;
  AIConv cd;
  asize outbuf_size;
  aboolean have_error = FALSE;
  aboolean done = FALSE;

  AError *local_error = NULL;
  a_return_val_if_fail (str != NULL, NULL);
  a_return_val_if_fail (to_codeset != NULL, NULL);
  a_return_val_if_fail (from_codeset != NULL, NULL);
  if (len < 0)
    len = strlen (str);
  /* Try an exact conversion; we only proceed if this fails
   * due to an illegal sequence in the input string.
   */
  dest = a_convert (str, len, to_codeset, from_codeset,bytes_read, bytes_written, &local_error);
  if (!local_error)
    return dest;

  if (!a_error_matches (local_error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_ILLEGAL_SEQUENCE))
    {
	  a_error_transfer (error, local_error);
      return NULL;
    }
  else
    a_error_free (local_error);

  local_error = NULL;

  /* No go; to proceed, we need a converter from "UTF-8" to
   * to_codeset, and the string as UTF-8.
   */
  cd = opea_converter (to_codeset, "UTF-8", error);
  if (cd == (AIConv) -1)
    {
      if (bytes_read)
        *bytes_read = 0;

      if (bytes_written)
        *bytes_written = 0;

      return NULL;
    }

  utf8 = a_convert (str, len, "UTF-8", from_codeset,bytes_read, &inbytes_remaining, error);
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
  outp = dest = a_malloc (outbuf_size);

  while (!done && !have_error)
    {
      asize inbytes_tmp = inbytes_remaining;
      err = a_iconv (cd, (char **)&p, &inbytes_tmp, &outp, &outbytes_remaining);
      inbytes_remaining = inbytes_tmp;

      if (err == (asize) -1){
	  switch (errno)
	    {
	    case EINVAL:
	      printf("不应该进到这里。。。a_convert_with_fallback EINVAL\n");
	      exit(0);
	      break;
	    case E2BIG:
	      {
		asize used = outp - dest;

		outbuf_size *= 2;
		dest = a_realloc (dest, outbuf_size);

		outp = dest + used;
		outbytes_remaining = outbuf_size - used - NUL_TERMINATOR_LENGTH;

		break;
	      }
	    case EILSEQ:
	      if (save_p)
		{
		  /* Error converting fallback string - fatal
		   */
	    	  a_error_printf (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
			       "Cannot convert fallback “%s” to codeset “%s”",
			       insert_str, to_codeset);
		  have_error = TRUE;
		  break;
		}
	      else if (p)
		{
		  if (!fallback)
		    {
		      aunichar ch = a_utf8_get_char (p);
		      insert_str = a_strdup_printf (ch < 0x10000 ? "\\u%04x" : "\\U%08x",
						    ch);
		    }
		  else
		    insert_str = fallback;

		  save_p = a_utf8_next_char (p);
		  save_inbytes = inbytes_remaining - (save_p - p);
		  p = insert_str;
		  inbytes_remaining = strlen (p);
		  break;
		}
              /* if p is null */
              A_GNUC_FALLTHROUGH;
	    default:
              {
                int errsv = errno;
                char buffer[1024];
                a_strerror (errsv,buffer);
                a_error_printf (error, A_CONVERT_ERROR_DOMAIN, A_CONVERT_ERROR_FAILED,"Error during conversion: %s",buffer);
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
		a_free ((achar *)insert_str);
	      p = save_p;
	      inbytes_remaining = save_inbytes;
	      save_p = NULL;
	    }
	  else if (p)
	    {
	      /* call a_iconv with NULL inbuf to cleanup shift state */
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

  a_free (utf8);

  if (have_error)
    {
      if (save_p && !fallback)
	a_free ((achar *)insert_str);
      a_free (dest);
      return NULL;
    }
  else
    return dest;
}











