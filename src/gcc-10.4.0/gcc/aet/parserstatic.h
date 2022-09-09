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

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classpermission.h"

typedef struct _ParserStatic ParserStatic;
/* --- structures --- */
struct _ParserStatic
{
	c_parser *parser;
    ClassPermission *classPermission;
    NPtrArray *tokenArray;
    int compileIndex;
};

typedef struct _StaticFuncParm
{
	int   parmPos[20];
	char *sysClassName[20];
	char *orginalName[20];
	int count;

	tree  matchTree[20];
	tree  portionOk[20];
	nboolean ok;

}StaticFuncParm;




ParserStatic   *parser_static_get();
tree            parser_static_create_temp_tree(ParserStatic *self,ClassName *className,char *orginalName);
void            parser_static_set_parser(ParserStatic *self,c_parser *parser,ClassPermission *permission);
StaticFuncParm* parser_static_get_func_parms(ParserStatic *self,vec<tree, va_gc> *exprlist);
void            parser_static_select_static_parm_for_ctor(ParserStatic *self,ClassName *className,StaticFuncParm *staticFuncParm);
void            parser_static_report_error(ParserStatic *self,StaticFuncParm *staticFuncParm,vec<tree, va_gc> *exprlist);
void            parser_static_replace(ParserStatic *self,StaticFuncParm *staticFuncParm,vec<tree, va_gc> *exprlist);
void            parser_static_free(ParserStatic *self,StaticFuncParm *staticFuncParm);
void            parser_static_select_static_parm_for_func(ParserStatic *self,ClassName *className,
		                char *orgiName,StaticFuncParm *staticFuncParm,nboolean isStatic);
int             parser_static_get_error_parm_pos(ParserStatic *self,StaticFuncParm *staticFuncParm,vec<tree, va_gc> *exprlist);
void            parser_static_report_error_by_pos(ParserStatic *self,int pos,vec<tree, va_gc> *exprlist);

////////////////////////---------------------
void  parser_static_collect_token(ParserStatic *self,ClassPermissionType permission,nboolean isFinal);
void  parser_static_compile(ParserStatic *self,char *sysName);
void  parser_static_continue_compile(ParserStatic *self);
void  parser_static_ready(ParserStatic *self);
/**
 * 检查是不是给函数变量赋值，如果右边是类中的静态函数。需要重新生成新的tree
 * typedef auint (*AHashFunc) (aconstpointer  key);
 * AHashFunc var=Abc.strHashFunc;
 * class Abc{
 *   public$ static auint strHashFunc(aconstpointer key);
 * }
 * 如何知道有多个strHashFunc静态函数时，该选择那一个呢？
 * 用var的参数和strHashFunc生成mangle的函数名，然后再通过funcmgr查找。
 */
tree parser_static_modify_func_pointer(ParserStatic *self,tree lhs,tree rhs);


#endif

