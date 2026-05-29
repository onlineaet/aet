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

#ifndef __A_CHARSET_H__
#define __A_CHARSET_H__


#include "abase.h"

aboolean        a_get_console_charset (const char **charset);
const char **  _a_charset_get_aliases (const char *canonical_name);
achar          *a_get_codeset (void);
aboolean        a_get_charset (const char **charset);
const char    *_a_locale_charset_unalias (const char *codeset);


#endif  /* __A_CHARSET_H__ */

