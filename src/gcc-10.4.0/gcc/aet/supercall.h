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


#ifndef __GCC_SUPER_CALL_H__
#define __GCC_SUPER_CALL_H__

#include "aetmangle.h"
#include "c-aet.h"
#include "classinfo.h"
#include "nlib.h"


typedef struct _SuperCall SuperCall;
/* --- structures --- */
struct _SuperCall
{
	c_parser *parser;
	NHashTable *hashTable;
    int nest;
};

SuperCall *super_call_new();
nboolean   super_call_is_super_state(SuperCall *self);
void       super_call_minus(SuperCall *self);
void       super_call_parser(SuperCall *self,ClassName *className,nboolean isConstructor);
tree       super_call_parser_at_postfix_expression(SuperCall *self,ClassName *className);
tree       super_call_replace_super_call(SuperCall *self,tree exprValue);
char      *super_call_create_codes(SuperCall *self,ClassName *className);


#endif



