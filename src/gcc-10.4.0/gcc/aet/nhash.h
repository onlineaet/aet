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

#ifndef __N_HASH_H__
#define __N_HASH_H__



#include "nbase.h"
#include "nlist.h"




typedef struct _NHashTable  NHashTable;

typedef nboolean  (*NHRFunc)  (npointer  key,npointer  value,npointer  user_data);

typedef struct _NHashTableIter NHashTableIter;

struct _NHashTableIter
{
  /*< private >*/
  npointer      dummy1;
  npointer      dummy2;
  npointer      dummy3;
  int           dummy4;
  nboolean      dummy5;
  npointer      dummy6;
};

NHashTable* n_hash_table_new               (NHashFunc       hash_func,
                                            NEqualFunc      key_equal_func);
NHashTable* n_hash_table_new_full          (NHashFunc       hash_func,
                                            NEqualFunc      key_equal_func,
                                            NDestroyNotify  key_destroy_func,
                                            NDestroyNotify  value_destroy_func);
void        n_hash_table_destroy           (NHashTable     *hash_table);
nboolean    n_hash_table_insert            (NHashTable     *hash_table,
                                            npointer        key,
                                            npointer        value);
nboolean    n_hash_table_replace           (NHashTable     *hash_table,
                                            npointer        key,
                                            npointer        value);

nboolean    n_hash_table_add               (NHashTable     *hash_table,
                                            npointer        key);

nboolean    n_hash_table_remove            (NHashTable     *hash_table,
                                            nconstpointer   key);

void        n_hash_table_remove_all        (NHashTable     *hash_table);

nboolean    n_hash_table_steal             (NHashTable     *hash_table,
                                            nconstpointer   key);

nboolean    n_hash_table_steal_extended    (NHashTable     *hash_table,
                                            nconstpointer   lookup_key,
                                            npointer       *stolen_key,
                                            npointer       *stolen_value);

void        n_hash_table_steal_all         (NHashTable     *hash_table);

npointer    n_hash_table_lookup            (NHashTable     *hash_table,
                                            nconstpointer   key);

nboolean    n_hash_table_contains          (NHashTable     *hash_table,
                                            nconstpointer   key);

nboolean    n_hash_table_lookup_extended   (NHashTable     *hash_table,
                                            nconstpointer   lookup_key,
                                            npointer       *orin_key,
                                            npointer       *value);

void        n_hash_table_foreach           (NHashTable     *hash_table,
                                            NHFunc          func,
                                            npointer        user_data);

npointer    n_hash_table_find              (NHashTable     *hash_table,
                                            NHRFunc         predicate,
                                            npointer        user_data);

nuint       n_hash_table_foreach_remove    (NHashTable     *hash_table,
                                            NHRFunc         func,
                                            npointer        user_data);

nuint       n_hash_table_foreach_steal     (NHashTable     *hash_table,
                                            NHRFunc         func,
                                            npointer        user_data);

nuint       n_hash_table_size              (NHashTable     *hash_table);

NList *     n_hash_table_get_keys          (NHashTable     *hash_table);

NList *     n_hash_table_get_values        (NHashTable     *hash_table);

npointer *  n_hash_table_get_keys_as_array (NHashTable     *hash_table,
                                            nuint          *length);


void        n_hash_table_iter_init         (NHashTableIter *iter,
                                            NHashTable     *hash_table);

nboolean    n_hash_table_iter_next         (NHashTableIter *iter,
                                            npointer       *key,
                                            npointer       *value);
NHashTable* n_hash_table_iter_get_hash_table (NHashTableIter *iter);

void        n_hash_table_iter_remove       (NHashTableIter *iter);
void        n_hash_table_iter_replace      (NHashTableIter *iter,
                                            npointer        value);
void        n_hash_table_iter_steal        (NHashTableIter *iter);

NHashTable* n_hash_table_ref               (NHashTable     *hash_table);
void        n_hash_table_unref             (NHashTable     *hash_table);


nboolean n_str_equal    (nconstpointer  v1,nconstpointer  v2);
nuint    n_str_hash     (nconstpointer  v);


nboolean n_int_equal    (nconstpointer  v1,nconstpointer  v2);
nuint    n_int_hash     (nconstpointer  v);

nboolean n_int64_equal  (nconstpointer  v1,nconstpointer  v2);
nuint    n_int64_hash   (nconstpointer  v);

nboolean n_double_equal (nconstpointer  v1,nconstpointer  v2);
nuint    n_double_hash  (nconstpointer  v);

nuint    n_direct_hash  (nconstpointer  v) ;
nboolean n_direct_equal (nconstpointer  v1,nconstpointer  v2) ;


#endif /* __G_HASH_H__ */
