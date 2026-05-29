/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */

#ifndef __A_PRINTF_H__
#define __A_PRINTF_H__

#include <stdio.h>
#include <stdarg.h>
#include "abase.h"

#define _a_printf    printf
#define _a_fprintf   fprintf
#define _a_sprintf   sprintf
#define _a_snprintf  snprintf

#define _a_vprintf   vprintf
#define _a_vfprintf  vfprintf
#define _a_vsprintf  vsprintf
#define _a_vsnprintf vsnprintf

aint                  a_printf    (achar const *format,...) ;
aint                  a_fprintf   (FILE        *file, achar const *format, ...) ;
aint                  a_sprintf   (achar       *string, achar const *format, ...) ;
aint                  a_vprintf   (achar const *format,va_list      args) ;
aint                  a_vfprintf  (FILE        *file,achar const *format,va_list      args) ;
aint                  a_vsprintf  (achar       *string, achar const *format,va_list      args) ;
aint                  a_vasprintf (achar      **string,achar const *format,va_list      args) ;
asize                 a_printf_string_upper_bound (const achar *format, va_list      args);
aint                  a_snprintf(achar *string ,aulong n,achar const *format,...);
aint                  a_vsnprintf          (achar *string,aulong n,achar const *format, va_list  args);


#endif /* __A_PRINTF_H__ */

