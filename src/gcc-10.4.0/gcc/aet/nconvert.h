/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

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

#ifndef __N_CONVERT_H__
#define __N_CONVERT_H__



#include "nbase.h"
#include "nerror.h"


typedef enum
{
  N_CONVERT_ERROR_NO_CONVERSION,
  N_CONVERT_ERROR_ILLEGAL_SEQUENCE,
  N_CONVERT_ERROR_FAILED,
  N_CONVERT_ERROR_PARTIAL_INPUT,
  N_CONVERT_ERROR_BAD_URI,
  N_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
  N_CONVERT_ERROR_NO_MEMORY,
  N_CONVERT_ERROR_EMBEDDED_NUL
} NConvertError;


#define N_CONVERT_ERROR 1


typedef struct _NIConv *NIConv;


NIConv n_iconv_open   (const nchar  *to_codeset,const nchar  *from_codeset);
nsize  n_iconv        (NIConv converter,nchar **inbuf,nsize *inbytes_left,nchar **outbuf,nsize *outbytes_left);
nint   n_iconv_close  (NIConv        converter);
nchar* n_convert      (const nchar  *str,nssize  len, const nchar  *to_codeset,
				const nchar  *from_codeset,
				nsize        *bytes_read,
				nsize        *bytes_written,
				NError      **error) ;

nchar* n_convert_with_iconv    (const nchar  *str,
				nssize        len,
				NIConv        converter,
				nsize        *bytes_read,
				nsize        *bytes_written,
				NError      **error) ;

nchar* n_convert_with_fallback (const nchar  *str,
				nssize        len,
				const nchar  *to_codeset,
				const nchar  *from_codeset,
				const nchar  *fallback,
				nsize        *bytes_read,
				nsize        *bytes_written,
				NError      **error) ;


/* Convert between libc's idea of strings and UTF-8.
 */

nchar* n_locale_to_utf8   (const nchar  *opsysstring,
			   nssize        len,
			   nsize        *bytes_read,
			   nsize        *bytes_written,
			   NError      **error) ;

nchar* n_locale_from_utf8 (const nchar  *utf8string,
			   nssize        len,
			   nsize        *bytes_read,
			   nsize        *bytes_written,
			   NError      **error) ;

/* Convert between the operating system (or C runtime)
 * representation of file names and UTF-8.
 */

nchar* n_filename_to_utf8   (const nchar  *opsysstring,
			     nssize        len,
			     nsize        *bytes_read,
			     nsize        *bytes_written,
			     NError      **error) ;

nchar* n_filename_from_utf8 (const nchar  *utf8string,
			     nssize        len,
			     nsize        *bytes_read,
			     nsize        *bytes_written,
			     NError      **error) ;


nchar *n_filename_from_uri (const nchar *uri,
			    nchar      **hostname,
			    NError     **error) ;


nchar *n_filename_to_uri   (const nchar *filename,
			    const nchar *hostname,
			    NError     **error) ;

nchar *n_filename_display_name (const nchar *filename) ;

nboolean n_get_filename_charsets (const nchar ***filename_charsets);


nchar *n_filename_display_basename (const nchar *filename) ;


nchar **n_uri_list_extract_uris (const nchar *uri_list);


#endif /* __N_CONVERT_H__ */













