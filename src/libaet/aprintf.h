
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

