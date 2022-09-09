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

#ifndef __GCC_GENERIC_FILE_H__
#define __GCC_GENERIC_FILE_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "genericinfo.h"
#include "linkfile.h"
#include "xmlfile.h"


typedef struct _GenericFile GenericFile;
/* --- structures --- */
struct _GenericFile
{
	   LinkFile *secondCompileFile;
	   XmlFile *xmlFile;
	   NPtrArray *genericDataArray;
};


GenericFile  *generic_file_get();
/**
 * 保存泛型信息
 */
void          generic_file_use_generic(GenericFile *self);
void          generic_file_save(GenericFile *self,ClassName *className);
void          generic_file_save_define_new$(GenericFile *self,char *content);
void          generic_file_save_out_func_call(GenericFile *self,char *content);
/**
 * 获取泛型信息
 */
BlockContent *generic_file_get_block_content(GenericFile *self,char *sysName);
NPtrArray    *generic_file_get_define_new_object(GenericFile *self,char *sysName);
NPtrArray    *generic_file_get_undefine_new_object(GenericFile *self,char *belongSysName);
char         *generic_file_get_file_class_located(GenericFile *self,char *sysName);
NPtrArray    *generic_file_get_define_func_call(GenericFile *self,char *belongSysName);
NPtrArray    *generic_file_get_undefine_func_call(GenericFile *self,char *belongSysName);



#endif
