

#ifndef __A_CONVERT_H__
#define __A_CONVERT_H__

#include "abase.h"
#include "aerror.h"


typedef enum
{
  A_CONVERT_ERROR_NO_CONVERSION,
  A_CONVERT_ERROR_ILLEGAL_SEQUENCE,
  A_CONVERT_ERROR_FAILED,
  A_CONVERT_ERROR_PARTIAL_INPUT,
  A_CONVERT_ERROR_BAD_URI,
  A_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
  A_CONVERT_ERROR_NO_MEMORY,
  A_CONVERT_ERROR_EMBEDDED_NUL
} AConvertError;

/**
 * A_CONVERT_ERROR:
 *
 * Error domain for character set conversions. Errors in this domain will
 * be from the #NConvertError enumeration. See #AError for information on
 * error domains.
 */
#define A_CONVERT_ERROR a_convert_error_quark()

static inline auint a_convert_error_quark (void){
	return 1;
}

/**
 * AIConv: (skip)
 *
 * The AIConv struct wraps an iconv() conversion descriptor. It contains
 * private data and should only be accessed using the following functions.
 */
typedef struct _AIConv *AIConv;


AIConv a_iconv_open   (const achar  *to_codeset,const achar  *from_codeset);
asize  a_iconv        (AIConv converter,achar **inbuf,asize *inbytes_left,achar **outbuf,asize *outbytes_left);
aint   a_iconv_close  (AIConv        converter);
achar* a_convert      (const achar  *str,assize  len, const achar  *to_codeset,
				const achar  *from_codeset,
				asize        *bytes_read,
				asize        *bytes_written,
				AError      **error) ;

achar* a_convert_with_iconv    (const achar  *str,
				assize        len,
				AIConv        converter,
				asize        *bytes_read,
				asize        *bytes_written,
				AError      **error) ;

achar* a_convert_with_fallback (const achar  *str,
				assize        len,
				const achar  *to_codeset,
				const achar  *from_codeset,
				const achar  *fallback,
				asize        *bytes_read,
				asize        *bytes_written,
				AError      **error) ;



#endif /* __A_CONVERT_H__ */



