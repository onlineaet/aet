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

#include "nerror.h"
#include "nslice.h"
#include "nstrfuncs.h"
#include "nlog.h"
#include "nmem.h"


NError* n_error_new_valist (NQuark domain,   nint code, const nchar *format, va_list      args)
{
  NError *error;
  n_warn_if_fail (domain != 0);
  n_warn_if_fail (format != NULL);

  error = (NError *)n_slice_new (NError);

  error->domain = domain;
  error->code = code;
  error->message = n_strdup_vprintf (format, args);

  return error;
}


NError* n_error_new (NQuark       domain, nint code,const nchar *format, ...)
{
  NError* error;
  va_list args;

  n_return_val_if_fail (format != NULL, NULL);
  n_return_val_if_fail (domain != 0, NULL);

  va_start (args, format);
  error = n_error_new_valist (domain, code, format, args);
  va_end (args);

  return error;
}


NError* n_error_new_literal (NQuark  domain,nint code, const nchar   *message)
{
  NError* err;

  n_return_val_if_fail (message != NULL, NULL);
  n_return_val_if_fail (domain != 0, NULL);
  err = (NError *) n_slice_new (NError);
  err->domain = domain;
  err->code = code;
  err->message = n_strdup (message);
  return err;
}


void n_error_free (NError *error)
{
  n_return_if_fail (error != NULL);
  n_free (error->message);
  n_slice_free (NError, error);
}


NError* n_error_copy (const NError *error)
{
  NError *copy;
 
  n_return_val_if_fail (error != NULL, NULL);
  n_warn_if_fail (error->domain != 0);
  n_warn_if_fail (error->message != NULL);

  copy =  (NError *)n_slice_new (NError);

  *copy = *error;

  copy->message = n_strdup (error->message);

  return copy;
}


nboolean n_error_matches (const NError *error,NQuark domain,nint code)
{
    return error && error->domain == domain &&  error->code == code;
}

#define ERROR_OVERWRITTEN_WARNING "NError set over the top of a previous NError or uninitialized memory.\n" \
               "This indicates a bug in someone's code. You must ensure an error is NULL before it's set.\n" \
               "The overwriting error message was: %s"

void n_set_error (NError      **err,NQuark  domain,nint code,const nchar  *format,...)
{
  NError *newError;
  va_list args;

  if (err == NULL)
    return;

  va_start (args, format);
  newError = n_error_new_valist (domain, code, format, args);
  va_end (args);

  if (*err == NULL)
    *err = newError;
  else
    {
      n_warning (ERROR_OVERWRITTEN_WARNING, newError->message);
      n_error_free (newError);
    }
}


void n_set_error_literal (NError **err,NQuark domain,nint code,const nchar  *message)
{
  if (err == NULL)
    return;
  if (*err == NULL){
    *err = n_error_new_literal (domain, code, message);
  }else{
    n_warning (ERROR_OVERWRITTEN_WARNING, message);
  }
}


void n_propagate_error (NError **dest, NError  *src)
{
  n_return_if_fail (src != NULL);
   if (dest == NULL)
    {
      if (src)
        n_error_free (src);
      return;
    }
  else
    {
      if (*dest != NULL)
        {
          n_warning (ERROR_OVERWRITTEN_WARNING, src->message);
          n_error_free (src);
        }
      else
        *dest = src;
    }
}


void n_clear_error (NError **err)
{
  if (err && *err)
    {
      n_error_free (*err);
      *err = NULL;
    }
}

static void n_error_add_prefix (nchar       **string,const nchar  *format,va_list       ap)
{
  nchar *oldstring;
  nchar *prefix;

  prefix = n_strdup_vprintf (format, ap);
  oldstring = *string;
  *string = n_strconcat (prefix, oldstring, NULL);
  n_free (oldstring);
  n_free (prefix);
}


void n_prefix_error (NError      **err,
                const nchar  *format,
                ...)
{
  if (err && *err)
    {
      va_list ap;

      va_start (ap, format);
      n_error_add_prefix (&(*err)->message, format, ap);
      va_end (ap);
    }
}


void n_propagate_prefixed_error (NError      **dest,
                            NError       *src,
                            const nchar  *format,
                            ...)
{
  n_propagate_error (dest, src);

  if (dest && *dest)
    {
      va_list ap;

      va_start (ap, format);
      n_error_add_prefix (&(*dest)->message, format, ap);
      va_end (ap);
    }
}
