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

#ifndef __GCC_MAKEFILE_PARM_H__
#define __GCC_MAKEFILE_PARM_H__

#include "nlib.h"
#include "linkfile.h"


typedef struct _MakefileParm MakefileParm;
/* --- structures --- */
struct _MakefileParm
{
	NPtrArray *bufferFiles;
	LinkFile *collect2LinkFile;
	char     *objectPath;
	nboolean  isSecondCompile;
	char     *objectFile;//.o文件
};


MakefileParm  *makefile_parm_get();
nboolean       makefile_parm_is_second_compile(MakefileParm *self);
char          *makefile_parm_get_full_file(MakefileParm *self,char *fileName);
char          *makefile_parm_get_object_root_path(MakefileParm *self);
char          *makefile_parm_get_object_file(MakefileParm *self);
void           makefile_parm_write_compile_argv(MakefileParm *self);
void           makefile_parm_append_d_file(MakefileParm *self);



#endif


