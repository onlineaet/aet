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

#ifndef __GCC_CLASS_FINALIZE_H__
#define __GCC_CLASS_FINALIZE_H__

#include "aetmangle.h"
#include "nlib.h"


typedef struct _ClassFinalize ClassFinalize;
/* --- structures --- */
struct _ClassFinalize
{
	c_parser *parser;
};


ClassFinalize *class_finalize_new();


nboolean  class_finalize_parser(ClassFinalize *self,ClassName *className);
nboolean  class_finalize_have_field(ClassFinalize *self,ClassName *className);
nboolean  class_finalize_have_define(ClassFinalize *self,ClassName *className);
tree      class_finalize_create_finalize_decl(ClassFinalize *self,ClassName *className,tree structType);
tree      class_finalize_create_unref_decl(ClassFinalize *self,ClassName *className,tree structType);
void      class_finalize_build_finalize_define(ClassFinalize *self,ClassName *className);
nboolean  class_finalize_build_unref_define(ClassFinalize *self,ClassName *className);


#endif

