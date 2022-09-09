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

#ifndef __GCC_CLASS_LIB_H__
#define __GCC_CLASS_LIB_H__

#include "nlib.h"
#include "classinfo.h"
#include "genericinfo.h"

typedef struct _ClassLib ClassLib;
/* --- structures --- */
struct _ClassLib
{
   XmlFile *xmlFile;
   NPtrArray *ifaceArray;
   NPtrArray *genericArray;
   NPtrArray *funcCheckArray;
};


ClassLib    *class_lib_get();
BlockContent *class_lib_get_block_content(ClassLib *self,char *sysName);
NPtrArray    *class_lib_get_undefine_obj(ClassLib *self,char *sysName);
NPtrArray    *class_lib_get_define_obj(ClassLib *self,char *sysName);
char         *class_lib_get_file_class_located(ClassLib *self,char *sysName);
nboolean      class_lib_have_interface_static_define(ClassLib *self,char *sysName);
NPtrArray    *class_lib_get_define_func_call(ClassLib *self,char *sysName);
NPtrArray    *class_lib_get_undefine_func_call(ClassLib *self,char *sysName);



#endif


