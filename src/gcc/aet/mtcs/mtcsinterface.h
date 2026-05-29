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


#ifndef __GCC_MTCS_INTERFACE__
#define __GCC_MTCS_INTERFACE__

#include "../nlib.h"


typedef struct _MtcsBackupRestore MtcsBackupRestore;

struct _MtcsBackupRestore
{
    npointer impl;//接口实现的类
    void (*backup)(MtcsBackupRestore *self);
    void (*restore)(MtcsBackupRestore *self);
};



#endif
