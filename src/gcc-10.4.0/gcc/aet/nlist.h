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

#ifndef __N_LIST_H__
#define __N_LIST_H__



#include "nbase.h"


typedef struct _NList NList;

struct _NList
{
  npointer data;
  NList *next;
  NList *prev;
};

/* Doubly linked lists
 */

NList*   n_list_alloc                   (void);

void     n_list_free                    (NList            *list);

void     n_list_free_1                  (NList            *list);

void     n_list_free_full               (NList            *list, NDestroyNotify    free_func);

NList*   n_list_append                  (NList            *list,	 npointer          data) ;

NList*   n_list_prepend                 (NList            *list,	 npointer          data) ;

NList*   n_list_insert                  (NList            *list,		 npointer          data,	 nint              position) ;

NList*   n_list_insert_sorted           (NList            *list,
					 npointer          data,
					 NCompareFunc      func) ;

NList*   n_list_insert_sorted_with_data (NList            *list, npointer          data, NCompareDataFunc  func, npointer          user_data) ;

NList*   n_list_insert_before           (NList            *list, NList            *sibling,	 npointer          data) ;

NList*   n_list_insert_before_link      (NList            *list, NList            *sibling, NList            *link_) ;

NList*   n_list_concat                  (NList            *list1, NList            *list2) ;

NList*   n_list_remove                  (NList            *list, nconstpointer     data);

NList*   n_list_remove_all              (NList            *list, nconstpointer     data) ;

NList*   n_list_remove_link             (NList            *list, NList            *llink) ;

NList*   n_list_delete_link             (NList            *list, NList            *link_) ;

NList*   n_list_reverse                 (NList            *list) ;

NList*   n_list_copy                    (NList            *list) ;


NList*   n_list_copy_deep               (NList            *list, NCopyFunc         func, npointer          user_data) ;


NList*   n_list_nth                     (NList            *list, nuint             n);

NList*   n_list_nth_prev                (NList            *list, nuint             n);

NList*   n_list_find                    (NList            *list,nconstpointer     data);

NList*   n_list_find_custom             (NList            *list, nconstpointer     data,NCompareFunc      func);

nint     n_list_position                (NList            *list, NList            *llink);

nint     n_list_index                   (NList            *list, nconstpointer     data);

NList*   n_list_last                    (NList            *list);

NList*   n_list_first                   (NList            *list);

nuint    n_list_length                  (NList            *list);

void     n_list_foreach                 (NList            *list,NFunc             func,	 npointer          user_data);

NList*   n_list_sort                    (NList            *list,NCompareFunc      compare_func) ;

NList*   n_list_sort_with_data          (NList            *list,NCompareDataFunc  compare_func,	 npointer          user_data)  ;

npointer n_list_nth_data                (NList            *list,nuint             n);


void     n_clear_list                   (NList           **list_ptr,NDestroyNotify    destroy);

#define  n_clear_list(list_ptr, destroy)       \
  N_STMT_START {                               \
    NList *_list;                              \
                                               \
    _list = *(list_ptr);                       \
    if (_list)                                 \
      {                                        \
        *list_ptr = NULL;                      \
                                               \
        if ((destroy) != NULL)                 \
          n_list_free_full (_list, (destroy)); \
        else                                   \
          n_list_free (_list);                 \
      }                                        \
  } N_STMT_END                                 \


#define n_list_previous(list)	        ((list) ? (((NList *)(list))->prev) : NULL)
#define n_list_next(list)	        ((list) ? (((NList *)(list))->next) : NULL)



#endif /* __N_LIST_H__ */
