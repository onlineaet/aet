

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "aprintf.h"
#include "alog.h"
#include "amem.h"

#define A_VA_COPY(ap1, ap2)	  memmove ((ap1), (ap2), sizeof (va_list))


aint a_printf (achar const *format,...)
{
  va_list args;
  aint retval;
  va_start (args, format);
  retval = a_vprintf (format, args);
  va_end (args);
    return retval;
}


aint a_fprintf (FILE *file, achar const *format,...)
{
  va_list args;
  aint retval;
  va_start (args, format);
  retval = a_vfprintf (file, format, args);
  va_end (args);
  return retval;
}


aint a_sprintf (achar       *string,achar const *format,...)
{
  va_list args;
  aint retval;
  va_start (args, format);
  retval = a_vsprintf (string, format, args);
  va_end (args);
  return retval;
}


aint a_snprintf (achar	*string,aulong	 n,achar const *format,...)
{
  va_list args;
  aint retval;
  va_start (args, format);
  retval = a_vsnprintf (string, n, format, args);
  va_end (args);
  return retval;
}


aint a_vprintf (achar const *format, va_list      args)
{
  a_return_val_if_fail (format != NULL, -1);
  return _a_vprintf (format, args);
}


aint a_vfprintf (FILE        *file,achar const *format,va_list      args)
{
  a_return_val_if_fail (format != NULL, -1);
  return _a_vfprintf (file, format, args);
}


aint a_vsprintf (achar	 *string,achar const *format,va_list      args)
{
  a_return_val_if_fail (string != NULL, -1);
  a_return_val_if_fail (format != NULL, -1);
  return _a_vsprintf (string, format, args);
}


aint a_vsnprintf (achar	 *string,
	     aulong	  n,
	     achar const *format,
	     va_list      args)
{
  a_return_val_if_fail (n == 0 || string != NULL, -1);
  a_return_val_if_fail (format != NULL, -1);
  return _a_vsnprintf (string, n, format, args);
}


aint a_vasprintf (achar **string,achar const *format,va_list  args)
{
	//以下方法会出 double free or corruption (out) 换成va_list
    aint len;
    a_return_val_if_fail (string != NULL, -1);
    int saved_errno;
    len = vasprintf (string, format, args);
    saved_errno = errno;
    if (len < 0){
        if (saved_errno == ENOMEM)
          a_error ("%s: failed to allocate memory", A_STRLOC);
        else
          *string = NULL;
    }

//	a_return_val_if_fail (string != NULL, -1);
//    va_list args2;
//    A_VA_COPY (args2, args);
//    *string = a_new (achar, a_printf_string_upper_bound (format, args));
//    aint len = _a_vsprintf (*string, format, args2);
//    va_end (args2);
    return len;
}

asize   a_printf_string_upper_bound (const achar *format, va_list args)
{
  achar c;
  return _a_vsnprintf (&c, 1, format, args) + 1;
}
