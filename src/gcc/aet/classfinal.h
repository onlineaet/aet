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
#include "aetparser.h"


typedef struct _ClassFinal ClassFinal;
/* --- structures --- */
struct _ClassFinal
{
   AetParser *parser;
   NPtrArray *finalVarArray;//记录编译单元初始化的
};


ClassFinal *class_final_get();
void        class_final_parser(ClassFinal *self,ClassParserState state,struct c_declspecs *specs);
void        class_final_check_modify(ClassFinal *self,location_t loc,tree lhs,tree rhs);
void        class_final_check_var(ClassFinal *self,ClassName *className);



#endif


