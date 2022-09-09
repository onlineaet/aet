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
#include "genericcommon.h"




typedef struct _GenericParser GenericParser;
/* --- structures --- */
struct _GenericParser
{
	c_parser *parser;
	SetBlockHelp *help;
	char *currentDefineStr;
};




GenericParser *generic_parser_new();
void           generic_parser_set_parser(GenericParser *self,c_parser *parser);
void           generic_parser_cast_by_token(GenericParser *self,c_token *token);
void           generic_parser_replace(GenericParser *self,char *genStr);
void           generic_parser_set_help(GenericParser *self,SetBlockHelp *help);
void           generic_parser_produce_cond_define(GenericParser *self);
void           generic_parser_parser_typeof(GenericParser *self);



#endif
