/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This impl is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the impl COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef __GCC_IFACE_IMPL_H__
#define __GCC_IFACE_IMPL_H__

#include "nlib.h"

#define IFACE_START "iface start:"
#define IFACE_END   "iface end:"
#define CLASS_IFACE_INFO_START "CLASS_IFACE_INFO_START:"
#define CLASS_IFACE_INFO_END "CLASS_IFACE_INFO_END:"
#define CLASS_IFACE_NEED_CHECK_START "CLASS_IFACE_NEED_CHECK_START:"
#define CLASS_IFACE_NEED_CHECK_END "CLASS_IFACE_NEED_CHECK_END:"

typedef struct _IfaceImpl IfaceImpl;
/* --- structures --- */
struct _IfaceImpl
{
    int reserve;
};


IfaceImpl  *iface_impl_get();
char       *iface_impl_save(IfaceImpl *self);
char       *iface_impl_check(IfaceImpl *self);

void        iface_impl_compile(IfaceImpl *self,char *ifaces);
void        iface_impl_compile_at_cfile(IfaceImpl *self,ClassInfo *classInfo);

void        iface_impl_compile_ready(IfaceImpl *self,char *objectRootPath,NPtrArray **arrays,int alen,int pos);
NPtrArray  *iface_impl_create_impl_codes(char *content);
void        iface_impl_valid(IfaceImpl *self,NPtrArray **arrays,int aLen,int needCheckPos,int infoPos);

#endif


