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


#ifndef __GCC_SUPER_DEFINE_H__
#define __GCC_SUPER_DEFINE_H__

#include "aetmangle.h"
#include "c-aet.h"
#include "classinfo.h"
#include "nlib.h"


typedef struct _SuperDefine SuperDefine;
/* --- structures --- */
struct _SuperDefine
{
	c_parser *parser;
};

SuperDefine *super_define_new();
char        *super_define_add(SuperDefine *self,ClassName *className);



#endif



