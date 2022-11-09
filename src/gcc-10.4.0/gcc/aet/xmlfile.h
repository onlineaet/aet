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


#ifndef __GCC_XML_FILE_H__
#define __GCC_XML_FILE_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "linkfile.h"


typedef struct _XmlFile XmlFile;
/* --- structures --- */
struct _XmlFile
{
	LinkFile *linkFile;
};

typedef struct _FuncCheckData{
	char *sysName;
	char *title;
	char *item;
	char *objectFile;
}FuncCheckData;

typedef struct _GenericSaveData{
	int   id;
	char *name;
	int blockCount;
	int undefineCount;
	int defineCount;
	char *value;
	struct blockStruct{
		char *key;
		char *value;
		int index;
	}blocks[30];

	struct undefineStruct{
		char *key;
		char *value;
    }undefine;
	struct defineStruct{
		char *key;
		char *value;
    }define;
	struct funcCallStruct{
		char *key;
		char *value;
    }funcCall;
	struct undefineFuncCallStruct{
		char *key;
		char *value;
    }undefineFuncCall;

	struct defineFuncCallStruct{
		char *key;
		char *value;
    }defineFuncCall;

	struct classFileStruct{
		char *key;
		char *value;
    }classfile;
}GenericSaveData;

typedef struct _IFaceSaveData{
	char *sysName;
	char *oFile;
	char *cFile;
}IFaceSaveData;

typedef enum{
	CLASS_GENERIC_TYPE,
	OUT_CLASS_NEW$_OBJECT,
	OUT_LOCAL_CLASS_FUNC_CALL,
}XmlGenericIdType;

XmlFile         *xml_file_new(char *fileName);
void             xml_file_add_func_check(XmlFile *self,char *sysName,char *title,char *item);
NPtrArray       *xml_file_get_func_check(XmlFile *self);

void             xml_file_remove_generic(XmlFile *self,int id,char *key);
void             xml_file_add_generic(XmlFile *self,GenericSaveData *data);
NPtrArray       *xml_file_get_generic_data(XmlFile *self);

char            *xml_file_create_iface_xml_string(NPtrArray *ifaceSaveDataArray);

void             xml_file_get_lib(XmlFile *self,NPtrArray **dataArrays);


GenericSaveData *xml_file_create_generic_data();
void             xml_file_free_generic_data(GenericSaveData *data);



#endif


