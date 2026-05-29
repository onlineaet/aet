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
#include "aetparser.h"
#include "supercall.h"
#include "genericmodel.h"
#include "genericfunc.h"


typedef struct _ClassParser ClassParser;
/* --- structures --- */
struct _ClassParser
{
   AetParser *parser;
   ClassCtor *classCtor;
   ClassInterface *classInterface;
   ClassInit *classInit;
   ClassFinalize *classFinalize;
   ClassPackage *classPackage;
   ClassPermission *classPermission;
   ClassFinal *classFinal;
   ClassBuild *classBuild;
   ClassParserState state;
   ClassName *currentClassName;
   char *fileName;
   nboolean expandMemory;
   SuperCall *superCall;
   nboolean compileIsMtcsClass;
   c_token tokens[20];
   int tokensCount;
   GenericModel *currentFuncModel;//当前泛型函数的model;
   //给.c文件加入头
   struct addAetHeader{
      nboolean added;//已经加过了
      c_token headBackTokens[10];
      int backTokenCount;
      location_t loc;//执行加头文件前的位置，可能是class$ interface$ public$的位置
      //aetparser.c 中分析说明符时，会报警告：空声明 加入状态 running ,aetparser可以具此跳过警告。
      nboolean running;
   }addAetHeader;
};


ClassParser       *class_parser_get();
struct c_typespec  class_parser_parser_class_specifier (ClassParser *self);
void               class_parser_replace_class_to_typedef(ClassParser *self);
void               class_parser_abstract_keyword(ClassParser *self);
nboolean           class_parser_is_parsering(ClassParser *self);
nboolean           class_parser_goto(ClassParser *self,nboolean start_attr_ok,int *action);
void               class_parser_final(ClassParser *self,struct c_declspecs *specs);
nboolean           class_parser_exception(ClassParser *self,tree value);
void               class_parser_decorate(ClassParser *self);
void               class_parser_parser_enum_dot(ClassParser *self,struct c_typespec *ret);
struct c_typespec  class_parser_enum(ClassParser *self,location_t loc);
void               class_parser_complete_enum(ClassParser *self,struct c_declspecs *specs,
                              nboolean haveAccessControl,ClassPermissionType premisson,ClassName *className);
ClassName         *class_parser_get_class_name(ClassParser *self);
GenericModel      *class_parser_get_func_generic_mode(ClassParser *self);
//加入aet.h头文年
nboolean           class_parser_add_include(ClassParser *self);
//是否正在加入aet.h头文件
nboolean           class_parser_is_add_include(ClassParser *self);

#endif

