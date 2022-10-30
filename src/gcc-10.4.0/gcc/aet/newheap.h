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

#ifndef __GCC_NEW_HEAP_H__
#define __GCC_NEW_HEAP_H__

#include "classinfo.h"
#include "classinit.h"
#include "nlib.h"
#include "newstrategy.h"




typedef struct _NewHeap NewHeap;
/* --- structures --- */
struct _NewHeap
{
	NewStrategy parent;

};

NewHeap    *new_heap_new();
nboolean    new_heap_create_object(NewHeap *self,tree var);
nboolean    new_heap_modify_object(NewHeap *self,tree var);
void        new_heap_create_object_no_decl(NewHeap *self,ClassName *className,GenericModel *genericDefine,char *ctorStr,nboolean isParserParmsState);
void        new_heap_finish(NewHeap *self,CreateClassMethod method,tree func);
char       *new_heap_create_object_for_static(NewHeap *self,tree var);


#endif

