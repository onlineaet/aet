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

#ifndef __GCC_MIDDLE_FILE_H__
#define __GCC_MIDDLE_FILE_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "linkfile.h"


typedef struct _MiddleFile MiddleFile;
/* --- structures --- */
struct _MiddleFile
{
   LinkFile *linkFile;
   nboolean  addImplIface;
};


MiddleFile  *middle_file_get();
void         middle_file_modify(MiddleFile *self);
void         middle_file_create_global_var(MiddleFile *self);
void         middle_file_import_lib(MiddleFile *self);
void         middle_file_ready_define_iface_static_var_and_init_method(MiddleFile *self);

char        *middle_file_decode(char *value,int size);




#endif


