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



#ifndef __GCC_LINK_FILE_H__
#define __GCC_LINK_FILE_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"


typedef struct _LinkFile LinkFile;
/* --- structures --- */
struct _LinkFile
{
   char *lockFile;
   char *fileName;
};


LinkFile  *link_file_new(char *fileName);
LinkFile  *link_file_new_with(char *fileName,char *lockFile);
int        link_file_lock(LinkFile *self);
void       link_file_unlock(int fd);
int        link_file_read(LinkFile *self,char *buffer,int size);
int        link_file_write(LinkFile *self,char *content,int size);
void       link_file_touch(LinkFile *self);
int        link_file_lock_read(LinkFile *self,char *buffer,int size);
int        link_file_apppend(LinkFile *self,char *content,int size);
int        link_file_lock_write(LinkFile *self,char *buffer,int size);
nboolean   link_file_lock_delete(LinkFile *self);
nint64     link_file_lock_get_create_time(LinkFile *self);
char      *link_file_read_text(LinkFile *self);
void       link_file_add_second_compile_file(LinkFile *self,char **oFiles,int count);




#endif


