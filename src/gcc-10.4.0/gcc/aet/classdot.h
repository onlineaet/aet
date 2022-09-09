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

#ifndef __GCC_CLASS_DOT_H__
#define __GCC_CLASS_DOT_H__

#include "classcast.h"
#include "classctor.h"
#include "classinfo.h"
#include "nlib.h"
#include "supercall.h"
#include "varcall.h"
#include "classaccess.h"
#include "classinterface.h"


typedef struct _ClassDot ClassDot;
/* --- structures --- */
struct _ClassDot
{
	ClassAccess parent;
	ClassInterface *classInterface;

};


ClassDot *class_dot_new(ClassInterface *classInterface);
nboolean  class_dot_is_class_ref(ClassDot *self,tree ptr);
tree      class_dot_build_ref(ClassDot *self,location_t loc,location_t component_loc,tree component,tree exprValue);

#endif

