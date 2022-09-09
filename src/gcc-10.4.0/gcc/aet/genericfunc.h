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

#ifndef __GCC_GENERIC_FUNC_H__
#define __GCC_GENERIC_FUNC_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"


typedef struct _GenericFunc GenericFunc;
/* --- structures --- */
struct _GenericFunc
{
	int reserve;
};


GenericFunc  *generic_func_new();
void          generic_func_add_fpgi_actual_parm(GenericFunc *self,ClassFunc *func,ClassName *className,vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine);



#endif

