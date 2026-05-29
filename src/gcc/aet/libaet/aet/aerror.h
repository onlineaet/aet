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


#ifndef __A_ERROR_H__
#define __A_ERROR_H__

#include <stdarg.h>
#include "abase.h"

typedef struct _AError AError;

struct _AError
{
  auint       domain;
  aint        code;
  achar       *message;
};

AError*  a_error_new_printf(auint domain,aint  code, const achar   *format,...) ;
AError*  a_error_new(auint domain,aint    code,const achar   *message);
AError*  a_error_new_valist(auint domain, aint   code, const achar   *format, va_list args);
void     a_error_free(AError        *error);
AError*  a_error_copy(const AError  *error);
aboolean a_error_matches(const AError  *error, auint domain, aint code);

void     a_error_printf(AError  **err,auint domain, aint code,const achar *format,...) ;
void     a_error_set(AError  **err,auint domain, aint code,const achar  *message);

/* 转移src到dest.
 */
void     a_error_transfer(AError  **dest,AError *src);




#endif /* __A_ERROR_H__ */

