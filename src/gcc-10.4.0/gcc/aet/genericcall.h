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

#ifndef __GCC_GENERIC_CALL_H__
#define __GCC_GENERIC_CALL_H__

#include "nlib.h"
#include "c-aet.h"
#include "classinfo.h"
#include "classfunc.h"
#include "genericfunc.h"


typedef struct _GenericCall GenericCall;
/* --- structures --- */
struct _GenericCall
{
	 int resver;
	 GenericFunc *genericFunc;
};


GenericCall  *generic_call_get();
void          generic_call_set_global(GenericCall *self,ClassName *className,GenericModel * genDefine);
tree          generic_call_check_parm(GenericCall *self,location_t loc,tree function,tree origGenericDecl,tree val,nboolean npc,nboolean excessrecision);
int           generic_call_replace_parm(GenericCall *self,location_t ploc,tree function,vec<tree, va_gc> *values);
GenericModel *generic_call_get_generic_from_component_ref(GenericCall *self,tree componentRef);
tree          generic_call_convert_generic_to_user(GenericCall *self,tree expr);
void          generic_call_add_fpgi_parm(GenericCall *self,ClassFunc *func,ClassName *className,vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine);


#endif

