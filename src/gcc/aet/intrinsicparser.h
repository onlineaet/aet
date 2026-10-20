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

#ifndef __GCC_INTRINSIC_PARSER_H__
#define __GCC_INTRINSIC_PARSER_H__

#include "nlib.h"
#include "classinfo.h"
#include "aetparser.h"

typedef struct _IntrinsicParser IntrinsicParser;
/* --- structures --- */
struct _IntrinsicParser
{
	AetParser *parser;
   //保存#pragma mtcs operation ld.global.cs 中的“ld.global.cs”
   struct{
      char *directives;
      nboolean enter;//=true进入了 #pragma mtcs operation
   }mtcsIntrinsic;
};

IntrinsicParser  *intrinsic_parser_new();
void              intrinsic_parser_class_end(IntrinsicParser *self);


#endif

