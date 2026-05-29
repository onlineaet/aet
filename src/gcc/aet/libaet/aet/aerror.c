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

#include <stdio.h>
#include "aerror.h"
#include "aslice.h"
#include "astrfuncs.h"
#include "alog.h"
#include "amem.h"


AError* a_error_new_valist (auint domain, aint code, const achar *format, va_list  args)
{
  AError *error;
  a_warn_if_fail (domain != 0);
  a_warn_if_fail (format != NULL);
  error = a_slice_new (AError);
  error->domain = domain;
  error->code = code;
  error->message = a_strdup_vprintf (format, args);
  return error;
}


AError* a_error_new_printf (auint domain, aint code,const achar *format, ...)
{
  AError* error;
  va_list args;
  a_return_val_if_fail (format != NULL, NULL);
  a_return_val_if_fail (domain != 0, NULL);
  va_start (args, format);
  error = a_error_new_valist (domain, code, format, args);
  va_end (args);
  return error;
}


AError* a_error_new(auint domain,aint code, const achar *message)
{
  AError* err;
  a_return_val_if_fail (message != NULL, NULL);
  a_return_val_if_fail (domain != 0, NULL);
  err = a_slice_new (AError);
  err->domain = domain;
  err->code = code;
  err->message = a_strdup (message);
  return err;
}


void a_error_free (AError *error)
{
  a_return_if_fail (error != NULL);
  a_free (error->message);
  a_slice_free (AError, error);
}


AError* a_error_copy (const AError *error)
{
  AError *copy;
  a_return_val_if_fail (error != NULL, NULL);
  /* See a_error_new_valist for why these don't return */
  a_warn_if_fail (error->domain != 0);
  a_warn_if_fail (error->message != NULL);
  copy = a_slice_new (AError);
  *copy = *error;
  copy->message = a_strdup (error->message);
  return copy;
}


aboolean a_error_matches (const AError *error,auint domain,aint code)
{
    return error && error->domain == domain &&  error->code == code;
}

#define ERROR_OVERWRITTEA_WARNING "AError set over the top of a previous AError or uninitialized memory.\n" \
               "This indicates a bug in someone's code. You must ensure an error is NULL before it's set.\n" \
               "The overwriting error message was: %s"

void a_error_printf(AError **err,auint  domain,aint code,const achar  *format,...)
{
  AError *newError;
  va_list args;
  if (err == NULL)
    return;
  va_start (args, format);
  newError = a_error_new_valist (domain, code, format, args);
  va_end (args);
  if (*err == NULL)
    *err = newError;
  else{
      a_warning (ERROR_OVERWRITTEA_WARNING, newError->message);
      a_error_free (newError);
  }
}

/**
 * 设置信息到**err
 */
void a_error_set(AError **err,auint domain,aint code,const achar  *message)
{
  if (err == NULL)
    return;
  if (*err == NULL){
     *err = a_error_new (domain, code, message);
  }else{
      a_warning (ERROR_OVERWRITTEA_WARNING, message);
  }
}

/**
 * 把dest转移到src
 */
void a_error_transfer (AError **dest,AError  *src)
{
   a_return_if_fail (src != NULL);
   if (dest == NULL){
      if (src)
        a_error_free (src);
      return;
   }else{
      if (*dest != NULL){
          a_warning (ERROR_OVERWRITTEA_WARNING, src->message);
          a_error_free (src);
      }else
        *dest = src;
   }
}





