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
 * AET was originally developed  by the zclei@sina.com
 */


#ifndef __A_THREAD_SPECIFIC_H__
#define __A_THREAD_SPECIFIC_H__

#include "abase.h"

/**
 * 线程的私有数据 ，可以返回当前线程 AThread或用户定制的数据
 */
typedef struct _AThreadSpecific        AThreadSpecific;

struct _AThreadSpecific
{
  apointer       p;
  ADestroyNotify notify;
};

apointer        a_thread_specific_get(AThreadSpecific  *key);
void            a_thread_specific_set(AThreadSpecific *key,apointer value);
void            a_thread_specific_replace(AThreadSpecific *key,apointer value);
apointer        a_thread_specific_set_alloc0 (AThreadSpecific *key,asize size);


#endif /* __A_UNICODE_H__ */


