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

#ifndef __GCC_CLASS_PARSER_H__
#define __GCC_CLASS_PARSER_H__


#include "nlib.h"
#include "classctor.h"
#include "classfinalize.h"
#include "classinfo.h"
#include "classinit.h"
#include "classinterface.h"
#include "classpackage.h"
#include "classpermission.h"
#include "c-aet.h"
#include "genericimpl.h"
#include "parserstatic.h"
#include "classfinal.h"
#include "classbuild.h"
#include "funccheck.h"


typedef struct _ClassParser ClassParser;
/* --- structures --- */
struct _ClassParser
{
	c_parser *parser;
	ClassCtor *classCtor;
	ClassInterface *classInterface;
    ClassInit *classInit;
    ClassFinalize *classFinalize;
    ClassPackage *classPackage;
    ClassPermission *classPermission;
    ClassFinal *classFinal;
    ClassBuild *classBuild;
    FuncCheck *funcCheck;

    ClassParserState state;

    ClassName *currentClassName;
    char *fileName;
};


ClassParser       *class_parser_get();
struct c_typespec  class_parser_parser_class_specifier (ClassParser *self);
void               class_parser_replace_class_to_typedef(ClassParser *self);
void               class_parser_abstract_keyword(ClassParser *self);
nboolean           class_parser_is_parsering(ClassParser *self);
nboolean           class_parser_goto(ClassParser *self,nboolean start_attr_ok,int *action);
void               class_parser_is_static_continue_compile_and_goto(ClassParser *self);
void               class_parser_final(ClassParser *self,struct c_declspecs *specs);
nboolean           class_parser_exception(ClassParser *self,tree value);
void               class_parser_set_parser(ClassParser *self,c_parser *parser);
void               class_parser_decorate(ClassParser *self);
void               class_parser_parser_enum_dot(ClassParser *self,struct c_typespec *ret);
struct c_typespec  class_parser_enum(ClassParser *self,location_t loc);
void               class_parser_over_enum(ClassParser *self,struct c_declspecs *specs);
ClassName         *class_parser_get_class_name(ClassParser *self);


#endif

