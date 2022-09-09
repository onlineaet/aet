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

#ifndef __GCC_ENUM_PARSER_H__
#define __GCC_ENUM_PARSER_H__


#include "nlib.h"
#include "classinfo.h"



typedef struct _EnumParser EnumParser;
/* --- structures --- */
struct _EnumParser
{
	c_parser *parser;
	NHashTable *hashTable;
};

typedef struct _EnumElement{
	char *sysName;
	char *origName;
	char *mangleName;
	tree value;
}EnumElement;

typedef struct _EnumData{
	char *sysName;
	char *origName;
	char *typedefName;
	char *enumName;
	ClassPermissionType permission;
	tree typeDecl;
	EnumElement *elements[300];
	int elementCount;
	location_t loc;
}EnumData;


EnumParser  *enum_parser_get();
nboolean     enum_parser_set_enum_type(EnumParser *self,c_token *who);
char        *enum_parser_get_orig_name(EnumParser *self,char *mangle);
EnumData    *enum_parser_get_enum(EnumParser *self,char *mangle);

struct c_typespec  enum_parser_parser(EnumParser *self,location_t loc,ClassName *className);
void         enum_parser_create_decl(EnumParser *self,location_t loc,ClassName *className,struct c_declspecs *specs,ClassPermissionType permission);
tree         enum_parser_parser_dot(EnumParser *self,struct c_typespec *specs);
nboolean     enum_parser_is_enum(EnumParser *self,char *typedefName);
void         enum_parser_build_dot (EnumParser *self, location_t loc,struct c_expr *expr);
nboolean     enum_parser_is_class_dot_enum(EnumParser *self,char *sysName,char *origEnumName);
void         enum_parser_build_class_dot_enum (EnumParser *self, location_t loc,char *sysName,char *origEnumName,struct c_expr *expr);
EnumData    *enum_parser_get_by_enum_name(EnumParser *self,char *sysName,char *enumName);
void         enum_parser_set_parser(EnumParser *self,c_parser *parser);


#endif

