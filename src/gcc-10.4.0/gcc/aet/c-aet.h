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

#ifndef __GCC_C_AET_H__
#define __GCC_C_AET_H__

#include "nlib.h"
#include "aetmicro.h"


#define DECL_LANG_SPECIFIC_CREATE_METHOD(NODE) c_aet_get_create_method(NODE)
#define DECL_LANG_SPECIFIC_CTOR(NODE) c_aet_get_ctor_codes(NODE)
#define DECL_LANG_SPECIFIC_SYS_CLASS_NAME(NODE) c_aet_get_ctor_sys_class_name(NODE)

void c_aet_set_create_method(tree dest,CreateClassMethod method);//创建对象的方法共7种
int  c_aet_get_create_method(tree dest);
void c_aet_set_ctor(tree dest,tree ctor,tree sysClassName,location_t loc);
tree c_aet_get_ctor_codes(tree dest);
location_t c_aet_get_ctor_location(tree dest);
tree c_aet_get_ctor_sys_class_name(tree dest);
void c_aet_set_modify_stack_new(tree dest,int end);
int  c_aet_get_is_modify_stack_new(tree dest);
void c_aet_copy_lang(tree dest,tree src);
void c_aet_set_decl_method(tree dest,int declMethod);

///////////////////////////genericmodel----------------------------
void *c_aet_get_func_generics_model(tree dest);
void *c_aet_get_generics_model(tree dest);
void c_aet_set_generics_model(tree dest,void *model);
void c_aet_set_func_generics_model(tree dest,void *model);
/////////////////////////////////////

void  c_aet_dump_return_expr(tree node,NPtrArray *array);


#endif /* ! GCC_C_AET_H */
