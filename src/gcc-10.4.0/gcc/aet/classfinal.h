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

#ifndef __GCC_CLASS_FINAL_H__
#define __GCC_CLASS_FINAL_H__

#include "nlib.h"
#include "classinfo.h"


typedef struct _ClassFinal ClassFinal;
/* --- structures --- */
struct _ClassFinal
{
	c_parser *parser;
	nboolean isFinal;
    location_t  loc ;
};


ClassFinal *class_final_new();
void        class_final_parser(ClassFinal *self,ClassParserState state,struct c_declspecs *specs);
nboolean    class_final_is_final(ClassFinal *self);
void        class_final_set_final(ClassFinal *self,nboolean is);
nboolean    class_final_set_field(ClassFinal *self,tree decls,ClassName *className);
void        class_final_check_and_set(ClassFinal *self,ClassParserState state);



#endif


