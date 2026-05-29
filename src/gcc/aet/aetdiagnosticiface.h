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

#ifndef __GCC_AET_DIAGNOSTIC_IFACE_H__
#define __GCC_AET_DIAGNOSTIC_IFACE_H__

//因为makefile.in中的OBJS-libcommon = 需要依赖实现文件aet/aetdiagnosticiface.c中的方法pp_demangle_text_by_aet
//采用回调方法后可以取消这种依赖。
void    aet_diagnostic_iface_set_callback();


#endif /* ! __GCC_AET_DIAGNOSTIC_IFACE_H__ */
