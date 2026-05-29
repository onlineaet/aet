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


#ifndef __GCC_GENERIC_PARSER_H__
#define __GCC_GENERIC_PARSER_H__

#include "nlib.h"
#include "c-aet.h"
#include "aetparser.h"

typedef struct _GenericParser GenericParser;
/* --- structures --- */
struct _GenericParser
{
	AetParser *parser;
	char *currentDefineStr;
	//新版 E_int_0_5{,F_float_1_8}
	void *directives[10];
	int directiveCount;
};

GenericParser  *generic_parser_get();
void           generic_parser_cast_by_token(GenericParser *self,c_token *token);
void           generic_parser_replace(GenericParser *self,char *genStr);
void           generic_parser_parser_typeof(GenericParser *self);

//新版11-10
void           generic_parser_enter(GenericParser *self);


#endif
