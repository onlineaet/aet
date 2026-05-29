/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the onlineaet@163.com
 */

#ifndef __GCC_MTCS_UNSPEC__
#define __GCC_MTCS_UNSPEC__

#include "../nlib.h"
#include "mtcscomponent.h"

/*
 * 匹配insn-constants.h
 */
typedef struct _MtcsUnspec MtcsUnspec;
struct _MtcsUnspec
{
    MtcsComponent parent;
    //原型 insns-constants.h
    int numUnspec;
    char **unspecStrings;

    int numUnspecv;
    char **unspecvStrings;
};

void mtcs_unspec_init(MtcsUnspec *self);
void mtcs_unspec_set_unspec_string(MtcsUnspec *self,char **values,int number);
void mtcs_unspec_set_unspecv_string(MtcsUnspec *self,char **values,int number);
char *mtcs_unspec_get_unspecv_string(MtcsUnspec *self,int index);
char *mtcs_unspec_get_unspec_string(MtcsUnspec *self,int index);
int mtcs_unspec_get_unspec_num(MtcsUnspec *self);
int mtcs_unspec_get_unspecv_num(MtcsUnspec *self);


#endif

