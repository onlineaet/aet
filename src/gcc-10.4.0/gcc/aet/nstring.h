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

#ifndef __N_STRING_H__
#define __N_STRING_H__


#include "nbase.h"
#include "nunicode.h"




typedef struct _NString         NString;

struct _NString
{
  nchar  *str;
  nsize len;
  nsize allocated_len;
};


NString*     n_string_new               (const nchar     *init);

NString*     n_string_new_len           (const nchar     *init,
                                         nssize           len);

NString*     n_string_sized_new         (nsize            dfl_size);

nchar*       n_string_free              (NString         *string,
                                         nboolean         free_segment);


nboolean     n_string_equal             (const NString   *v,
                                         const NString   *v2);

nuint        n_string_hash              (const NString   *str);

NString*     n_string_assign            (NString         *string,
                                         const nchar     *rval);

NString*     n_string_truncate          (NString         *string,nsize            len);

NString*     n_string_set_size          (NString         *string,nsize            len);

NString*     n_string_insert_len        (NString         *string,nssize pos,const nchar *val,nssize len);

NString*     n_string_append            (NString         *string,const nchar     *val);

NString*     n_string_append_len        (NString         *string,const nchar     *val,nssize len);

NString*     n_string_append_c          (NString         *string,nchar     c);

NString*     n_string_append_unichar    (NString         *string,nunichar         wc);

NString*     n_string_prepend           (NString         *string,const nchar     *val);

NString*     n_string_prepend_c         (NString         *string,nchar c);

NString*     n_string_prepend_unichar   (NString         *string,nunichar         wc);

NString*     n_string_prepend_len       (NString         *string,const nchar     *val,nssize  len);

NString*     n_string_insert            (NString         *string,nssize pos, const nchar     *val);

NString*     n_string_insert_c          (NString         *string,nssize pos,nchar     c);

NString*     n_string_insert_unichar    (NString         *string,nssize pos,nunichar  wc);

NString*     n_string_overwrite         (NString         *string,nsize pos,const nchar     *val);

NString*     n_string_overwrite_len     (NString         *string,nsize pos,const nchar     *val,nssize len);

NString*     n_string_erase             (NString         *string,nssize pos,nssize len);

NString*     n_string_ascii_down        (NString         *string);

NString*     n_string_ascii_up          (NString         *string);

void         n_string_vprintf           (NString         *string,const nchar     *format, va_list args);

void         n_string_printf            (NString         *string,const nchar     *format, ...);

void         n_string_append_vprintf    (NString         *string,const nchar     *format,va_list          args);

void         n_string_append_printf     (NString         *string,const nchar     *format,...) ;
nuint        n_string_replace (NString *string, const nchar *find,const nchar *replace,int limit);



nboolean  n_string_starts_with_from(NString *self,const char  *prefix, int toffset);
nboolean  n_string_starts_with(NString *self,const char  *prefix);
int       n_string_indexof_from(NString *self,const char *str,int fromIndex);
int       n_string_indexof(NString *self,const char *str);
int       n_string_last_indexof(NString *self,const char *str);
int       n_string_last_indexof_from(NString *self,const char *str,int from);
NString  *n_string_substring_from(NString *self,int begin,int end);
NString  *n_string_substring(NString *self,int begin);
nboolean  n_string_ends_with(NString *self,const char  *suffix);
void      n_string_trim(NString *self);
int       n_string_get_character_count(NString *self);


#endif /* __N_STRINN_H__ */
