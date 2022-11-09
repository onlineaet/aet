/*
 * Copyright (C) 2022 , guiyang,wangyong co.,ltd.

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

#ifndef __GCC_AET_EXPR_H__
#define __GCC_AET_EXPR_H__

#include "nlib.h"
#include "classinfo.h"
#include "classfunc.h"
#include "enumparser.h"


typedef struct _AetExpr AetExpr;
/* --- structures --- */
struct _AetExpr
{
	c_parser *parser;
};


AetExpr      *aet_expr_new();
struct c_expr aet_expr_varof_parser(AetExpr *self,struct c_expr lhs);
void          aet_expr_set_parser(AetExpr *self,c_parser *parser);




#endif


