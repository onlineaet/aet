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

#ifndef __GCC_CLASS_BUILD_H__
#define __GCC_CLASS_BUILD_H__

#include "nlib.h"
#include "classinfo.h"


typedef struct _ClassBuild ClassBuild;
/* --- structures --- */
struct _ClassBuild
{
	c_parser *parser;

};


ClassBuild *class_build_new();
void        class_build_create_codes(ClassBuild *self,ClassName *className,NString *codes);
tree        class_build_get_func(ClassBuild *self,ClassName *className);
void        class_build_replace_getclass_rtn(ClassBuild *self,ClassName *className);
void        class_build_create_func_name(ClassName *className,char *buf);
void        class_build_create_func_name_by_sys_name(char *sysName,char *buf);




#endif

