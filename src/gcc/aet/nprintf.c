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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "nprintf.h"
#include "nlog.h"
#include "nmem.h"

#define N_VA_COPY(ap1, ap2)	  memmove ((ap1), (ap2), sizeof (va_list))


nint n_printf (nchar const *format,...)
{
  va_list args;
  nint retval;
  va_start (args, format);
  retval = n_vprintf (format, args);
  va_end (args);
    return retval;
}


nint n_fprintf (FILE *file, nchar const *format,...)
{
  va_list args;
  nint retval;
  va_start (args, format);
  retval = n_vfprintf (file, format, args);
  va_end (args);
  return retval;
}


nint n_sprintf (nchar       *string,nchar const *format,...)
{
  va_list args;
  nint retval;
  va_start (args, format);
  retval = n_vsprintf (string, format, args);
  va_end (args);
  return retval;
}


nint n_snprintf (nchar	*string,nulong	 n,nchar const *format,...)
{
  va_list args;
  nint retval;
  va_start (args, format);
  retval = n_vsnprintf (string, n, format, args);
  va_end (args);
  return retval;
}


nint n_vprintf (nchar const *format, va_list      args)
{
  n_return_val_if_fail (format != NULL, -1);
  return _n_vprintf (format, args);
}


nint n_vfprintf (FILE        *file,nchar const *format,va_list      args)
{
  n_return_val_if_fail (format != NULL, -1);
  return _n_vfprintf (file, format, args);
}


nint n_vsprintf (nchar	 *string,nchar const *format,va_list      args)
{
  n_return_val_if_fail (string != NULL, -1);
  n_return_val_if_fail (format != NULL, -1);
  return _n_vsprintf (string, format, args);
}


nint n_vsnprintf (nchar	 *string,
	     nulong	  n,
	     nchar const *format,
	     va_list      args)
{
  n_return_val_if_fail (n == 0 || string != NULL, -1);
  n_return_val_if_fail (format != NULL, -1);
  return _n_vsnprintf (string, n, format, args);
}


nint n_vasprintf (nchar **string,nchar const *format,va_list  args)
{
    nint len;
    n_return_val_if_fail (string != NULL, -1);
    int saved_errno;
    len = vasprintf (string, format, args);
    saved_errno = errno;
    if (len < 0){
        if (saved_errno == ENOMEM)
          n_error ("%s: failed to allocate memory", N_STRLOC);
        else
          *string = NULL;
    }
    return len;
}

nsize   n_printf_string_upper_bound (const nchar *format, va_list args)
{
  nchar c;
  return _n_vsnprintf (&c, 1, format, args) + 1;
}
