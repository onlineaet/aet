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

#ifndef __GCC_NAMELESS_CALL_H__
#define __GCC_NAMELESS_CALL_H__


#include "nlib.h"

/**
 * 无名调用。
 * 是指调用的函数返回值是aet类，但返回值没有赋值。
 */

typedef struct _NamelessCall NamelessCall;
/* --- structures --- */
struct _NamelessCall
{
	c_parser *parser;
};


NamelessCall       *nameless_call_get();
void               nameless_call_set_parser(NamelessCall *self,c_parser *parser);
tree               nameless_call_parser(NamelessCall *self,tree call);



#endif

