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

#ifndef __GCC_MTCS_CODES__
#define __GCC_MTCS_CODES__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsCodes MtcsCodes;
struct _MtcsCodes
{
    MtcsComponent parent;
    //原型 NUM_INSN_CODES insn-codes.h
    nuint numInsnCodes;
    //原型 targetm.code_for_allocate_stack #define TARGET_CODE_FOR_ALLOCATE_STACK CODE_FOR_allocate_stack
    int (*get_code_for_allocate_stack)(MtcsCodes *self);

};

void   mtcs_codes_init(MtcsCodes *self);
//原型 NUM_INSN_CODES insn-codes.h
void   mtcs_codes_set_number(MtcsCodes *self,nuint numInsnCodes);
nuint  mtcs_codes_get_number(MtcsCodes *self);
//原型 targetm.code_for_allocate_stack #define TARGET_CODE_FOR_ALLOCATE_STACK CODE_FOR_allocate_stack
int    mtcs_codes_get_code_for_allocate_stack(MtcsCodes *self);

#endif

