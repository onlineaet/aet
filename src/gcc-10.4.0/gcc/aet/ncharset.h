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

#ifndef __N_CHARSET_H__
#define __N_CHARSET_H__


#include "nbase.h"



nboolean              n_get_charset         (const char **charset);

nchar *               n_get_codeset         (void);

nboolean              n_get_console_charset (const char **charset);


const nchar * const * n_get_language_names  (void);

const nchar * const * n_get_language_names_with_category(const nchar *category_name);

nchar **              n_get_locale_variants (const nchar *locale);
const char **        _n_charset_get_aliases (const char *canonical_name);


#endif  /* __N_CHARSET_H__ */





