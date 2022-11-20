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


#ifndef __GCC_PARSER_STATIC_H__
#define __GCC_PARSER_STATIC_H__

#include "c-aet.h"

typedef struct _ParserStatic ParserStatic;
/* --- structures --- */
struct _ParserStatic
{
	c_parser *parser;
    NPtrArray *tokenArray;
    int compileIndex;
};

ParserStatic   *parser_static_get();
tree            parser_static_create_temp_tree(ParserStatic *self,location_t loc,ClassName *className,char *orginalName);

void  parser_static_collect_token(ParserStatic *self,ClassPermissionType permission,nboolean isFinal);
void  parser_static_compile(ParserStatic *self,char *sysName);
void  parser_static_continue_compile(ParserStatic *self);
void  parser_static_ready(ParserStatic *self);
/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 *  1.AHashFunc func;
 *  func=AObject.strHash;
 *  2.AHashFunc func;
 *  func=5>3?AObject.strHash:xxx;
 *  3.AHashFunc func=AObject.strHash;
 *  4.AHashFunc func=5>3?AObject.strHash:xxx;
 */
tree  parser_static_modify_or_init_func_pointer(ParserStatic *self,tree lhs,tree rhs);
void  parser_static_set_parser(ParserStatic *self,c_parser *parser);


#endif

