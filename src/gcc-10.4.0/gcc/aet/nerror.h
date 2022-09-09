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

#ifndef __N_ERROR_H__
#define __N_ERROR_H__



#include <stdarg.h>

#include "nbase.h"


typedef struct _NError NError;

struct _NError
{
  NQuark       domain;
  nint         code;
  nchar       *message;
};

NError*  n_error_new           (NQuark         domain,nint  code, const nchar   *format,...) ;
NError*  n_error_new_literal   (NQuark         domain,nint    code,const nchar   *message);
NError*  n_error_new_valist    (NQuark         domain, nint   code, const nchar   *format, va_list args);
void     n_error_free          (NError        *error);
NError*  n_error_copy          (const NError  *error);
nboolean n_error_matches       (const NError  *error, NQuark domain, nint code);


void     n_set_error           (NError  **err,NQuark domain, nint code,const nchar *format,...) ;
void     n_set_error_literal   (NError  **err,NQuark domain, nint code,const nchar  *message);



void     n_propagate_error     (NError       **dest,NError        *src);


void     n_clear_error         (NError       **err);


void     n_prefix_error               (NError       **err,const nchar   *format,...) ;


void     n_propagate_prefixed_error   (NError       **dest,NError  *src,const nchar   *format,...);


#endif /* __N_ERROR_H__ */
