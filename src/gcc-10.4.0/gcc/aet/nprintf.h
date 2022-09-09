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

#ifndef __N_PRINTF_H__
#define __N_PRINTF_H__

#include <stdio.h>
#include <stdarg.h>

#include "nbase.h"

#define _n_printf    printf
#define _n_fprintf   fprintf
#define _n_sprintf   sprintf
#define _n_snprintf  snprintf

#define _n_vprintf   vprintf
#define _n_vfprintf  vfprintf
#define _n_vsprintf  vsprintf
#define _n_vsnprintf vsnprintf

nint                  n_printf    (nchar const *format,...) ;

nint                  n_fprintf   (FILE        *file, nchar const *format, ...) ;

nint                  n_sprintf   (nchar       *string, nchar const *format, ...) ;


nint                  n_vprintf   (nchar const *format,va_list      args) ;

nint                  n_vfprintf  (FILE        *file,nchar const *format,va_list      args) ;

nint                  n_vsprintf  (nchar       *string, nchar const *format,va_list      args) ;

nint                  n_vasprintf (nchar      **string,nchar const *format,va_list      args) ;

nsize                 n_printf_string_upper_bound (const nchar *format, va_list      args);

nint                  n_snprintf(nchar *string ,nulong n,nchar const *format,...);
nint                  n_vsnprintf          (nchar *string,nulong n,nchar const *format, va_list  args);


#endif /* __N_PRINTF_H__ */
