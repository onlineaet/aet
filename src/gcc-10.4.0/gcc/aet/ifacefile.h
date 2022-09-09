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

#ifndef __GCC_IFACE_FILE_H__
#define __GCC_IFACE_FILE_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "xmlfile.h"


typedef struct _IfaceFile IfaceFile;
/* --- structures --- */
struct _IfaceFile
{
	   XmlFile *xmlFile;
	   NPtrArray *recordCallIface;
};


IfaceFile  *iface_file_get();
void        iface_file_record(IfaceFile *self,ClassName *className);
void        iface_file_record_by_sys_name(IfaceFile *self,char *sysName);
void        iface_file_save(IfaceFile *self);
NPtrArray  *iface_file_read(IfaceFile *self);
void        iface_file_write(IfaceFile *self,NPtrArray *ifaceSaveDataArray);
void        iface_file_impl_iface(IfaceFile *self);




#endif


