/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

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

#ifndef __N_PTR_ARRAY_H__
#define __N_PTR_ARRAY_H__


#include "nbase.h"



typedef struct _NPtrArray	NPtrArray;

struct _NPtrArray
{
  npointer *pdata;
  nuint	    len;
};


/* Resizable pointer array.  This interface is much less complicated
 * than the above.  Add appends a pointer.  Remove fills any cleared 
 * spot and shortens the array. remove_fast will again distort order.  
 */
#define    n_ptr_array_index(array,index_) ((array)->pdata)[index_]

NPtrArray* n_ptr_array_new                (void);

NPtrArray* n_ptr_array_new_with_free_func (NDestroyNotify    element_free_func);

npointer*   n_ptr_array_steal              (NPtrArray        *array,nsize            *len);

NPtrArray *n_ptr_array_copy               (NPtrArray        *array,
                                           NCopyFunc         func,
                                           npointer          user_data);

NPtrArray* n_ptr_array_sized_new          (nuint             reserved_size);

NPtrArray* n_ptr_array_new_full           (nuint             reserved_size,
					   NDestroyNotify    element_free_func);

npointer*  n_ptr_array_free               (NPtrArray        *array,
					   nboolean          free_seg);

NPtrArray* n_ptr_array_ref                (NPtrArray        *array);

void       n_ptr_array_unref              (NPtrArray        *array);

void       n_ptr_array_set_free_func      (NPtrArray        *array,
                                           NDestroyNotify    element_free_func);

void       n_ptr_array_set_size           (NPtrArray        *array,
					   nint              length);

npointer   n_ptr_array_remove_index       (NPtrArray        *array,
					   nuint             index_);

npointer   n_ptr_array_remove_index_fast  (NPtrArray        *array,
					   nuint             index_);

npointer   n_ptr_array_steal_index        (NPtrArray        *array,
                                           nuint             index_);
npointer   n_ptr_array_steal_index_fast   (NPtrArray        *array,
                                           nuint             index_);

nboolean   n_ptr_array_remove             (NPtrArray        *array,
					   npointer          data);

nboolean   n_ptr_array_remove_fast        (NPtrArray        *array,
					   npointer          data);

NPtrArray *n_ptr_array_remove_range       (NPtrArray        *array,
					   nuint             index_,
					   nuint             length);

void       n_ptr_array_add                (NPtrArray        *array,
					   npointer          data);

void n_ptr_array_extend                   (NPtrArray        *array_to_extend,
                                           NPtrArray        *array,
                                           NCopyFunc         func,
                                           npointer          user_data);

void n_ptr_array_extend_and_steal         (NPtrArray        *array_to_extend,
                                           NPtrArray        *array);

void       n_ptr_array_insert             (NPtrArray        *array,
                                           nint              index_,
                                           npointer          data);

void       n_ptr_array_sort               (NPtrArray        *array,
					   NCompareFunc      compare_func);

void       n_ptr_array_sort_with_data     (NPtrArray        *array,
					   NCompareDataFunc  compare_func,
					   npointer          user_data);

void       n_ptr_array_foreach            (NPtrArray        *array,
					   NFunc             func,
					   npointer          user_data);

nboolean   n_ptr_array_find               (NPtrArray        *haystack,
                                           nconstpointer     needle,
                                           nuint            *index_);

nboolean   n_ptr_array_find_with_equal_func (NPtrArray     *haystack,
                                             nconstpointer  needle,
                                             NEqualFunc     equal_func,
                                             nuint         *index_);




#endif /* _N_ARRAY_H__ */
