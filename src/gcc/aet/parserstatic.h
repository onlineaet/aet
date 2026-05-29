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
#include "aetparser.h"

typedef struct _ParserStatic ParserStatic;
/* --- structures --- */
struct _ParserStatic
{
	 AetParser *parser;
};

ParserStatic   *parser_static_get();
tree            parser_static_create_temp_tree(ParserStatic *self,location_t loc,ClassName *className,char *orginalName);

/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 *  1.AHashFunc func;
 *  func=AObject.strHash;
 *  2.AHashFunc func;
 *  func=5>3?AObject.strHash:xxx;
 *  3.AHashFunc func=AObject.strHash;
 *  4.AHashFunc func=5>3?AObject.strHash:xxx;
 */
tree  parser_static_modify_or_init_func_pointer(ParserStatic *self,location_t loc,tree lhs,tree rhs);
//在所有静态符号中找出与id相同的标识符
nboolean parser_static_find_identifier(ParserStatic *self,tree id);
void parser_static_compile(ParserStatic *self,char *sysName,ClassPermissionType permission,nboolean isFinal);
/**
 * 编历 struct c_declarator *declarator 在数组的维度
 * 如果是变量，看是否有初始值，如果有并且是常数，替换
 */
void parser_static_replace_dimension(ParserStatic *self,struct c_declarator *declarator);


#endif

