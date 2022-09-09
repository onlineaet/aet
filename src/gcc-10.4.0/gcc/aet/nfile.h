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

#ifndef __N_FILE_H__
#define __N_FILE_H__

#include "nbase.h"
#include "nlist.h"
#include "nptrarray.h"
#include "nlist.h"

typedef struct _NFile         NFile;
typedef struct _NFilePrivate  NFilePrivate;



typedef enum{
  ACCESS_READ=     0x04,
  ACCESS_WRITE=    0x02,
  ACCESS_EXECUTE=  0x01,
}FileSystemAccessType;

struct _NFile
{
	NFilePrivate *priv;

};



NFile      *n_file_new(const char *pathname);
NFile      *n_file_new_by_parent(NFile *parent, const char *child);
NFile      *n_file_new_by_parent_str(const char *parent, const char *child);

const char *n_file_get_name(NFile *self);
const char *n_file_get_parent(NFile *self);
const char *n_file_get_path(NFile *self);
NFile      *n_file_get_parent_file(NFile *self);
nboolean    n_file_is_absolute(NFile *self);
nboolean    n_file_exists(NFile *self);
char       *n_file_get_absolute_path(NFile *self);
NFile      *n_file_get_absolute_file(NFile *self) ;
nboolean    n_file_can_read(NFile *self) ;
nboolean    n_file_can_write(NFile *self) ;
nboolean    n_file_is_directory(NFile *self);
nboolean    n_file_is_file(NFile *self);
nboolean    n_file_is_hidden(NFile *self);
nuint64     n_file_get_last_modified(NFile *self);
nuint64	    n_file_get_create_time(NFile *self);
nuint64     n_file_get_length(NFile *self);
nboolean    n_file_create_new_file(NFile *self);
nboolean    n_file_delete(NFile *self);
char      **n_file_list(NFile *self);
NFile     **n_file_list_files(NFile *self);
NList      *n_file_list_files_to_list(NFile *self);
nboolean    n_file_mkdir(NFile *self);
nboolean    n_file_mkdirs(NFile *self);
NFile 	   *n_file_get_canonical_file(NFile *self);
nboolean 	n_file_can_execute(NFile *self);
nboolean    n_file_rename(NFile *self,NFile *dest);
nboolean    n_file_set_read_only(NFile *self);
nboolean    n_file_set_last_modified(NFile *self,long time);
nboolean    n_file_set_writable(NFile *self,nboolean writable, nboolean ownerOnly);
nboolean    n_file_set_readable(NFile *self,nboolean readable, nboolean ownerOnly);
nboolean    n_file_set_executable(NFile *self,nboolean executable, nboolean ownerOnly);
nuint64     n_file_get_total_space(NFile *self);
nuint64     n_file_get_total_free(NFile *self);
nuint64     n_file_get_total_usable(NFile *self);
nboolean    n_file_equals(NFile *self,NFile *dest);
const char *n_file_to_string(NFile *self);
int         n_file_lengthv(NFile **self);
void        n_file_freev(NFile **self);
const char *n_file_get_default_parent();
void        n_file_unref(NFile *self);


#endif /* __N_FILE_H__*/




