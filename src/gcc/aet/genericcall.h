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
};


GenericCall  *generic_call_get();
GenericModel *generic_call_get_generic_from_component_ref(GenericCall *self,tree componentRef);
tree          generic_call_convert_generic_to_user(GenericCall *self,tree expr);


tree generic_call_check_parm(GenericCall *self,location_t ploc, tree function, tree fundecl,
                    tree type, tree origtype, tree val, tree valtype,
                    bool npc, tree rname, int parmnum, int argnum,
                    bool excess_precision, int warnopt,ClassName *globalClassName,GenericModel *globalGenericsDefine);


tree generic_call_replace_parm_new(GenericCall *self,location_t ploc, tree function, tree fundecl,
                        tree type, tree origtype, tree val, tree valtype,
                        bool npc, tree rname, int parmnum, int argnum,
                        bool excess_precision, int warnopt,ClassName *globalClassName,GenericModel *globalGenericsDefine);

nboolean  generic_call_check(GenericCall *self,ClassFunc *func,
      ClassName *className,vec<tree, va_gc> *exprlist,GenericModel *funcGenericDefine,location_t loc);

tree  generic_call_build_call(GenericCall *self,ClassFunc *func,GenericModel *funcGenericDefine,location_t loc,tree call);

#endif

