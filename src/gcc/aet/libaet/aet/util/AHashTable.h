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

#ifndef __AET_UTIL_A_HASH_TABLE_H__
#define __AET_UTIL_A_HASH_TABLE_H__

#include "../../aet.h"
#include "AList.h"

package$ aet.util;

typedef struct _AHashTablePriv AHashTablePriv;

public$ class$ HashIter{
   void  *hashTable;
   aint   position;
};

public$ class$ AHashTable{
   public$ static aboolean strEqual(const char *v1,const char *v2);
   public$ static auint    strHash(const char *v1);
   public$ static aboolean intEqual(const int *v1,const int *v2);
   public$ static auint    intHash(const int *v1);
   public$ static aboolean aulongEqual(aconstpointer v1,aconstpointer v2);
   public$ static auint    aulongHash(aconstpointer v);
   public$ static aboolean auint64Equal(const auint64 *v1,const auint64 *v2);
   public$ static auint    auint64Hash(const auint64 *v1);
   public$ static aboolean directEqual(aconstpointer v1,aconstpointer v2);
   public$ static auint    directHash(aconstpointer v1);
   private$ AHashTablePriv *priv;
   public$ AHashTable (AHashFunc hashFunc,AEqualFunc keyEqualFunc,ADestroyNotify keyDestroyFunc,ADestroyNotify valueDestroyFunc);
   public$ AHashTable ();//key类型是string
   public$ AHashTable(AHashFunc hashFunc,AEqualFunc keyEqualFunc);
   public$ aboolean put(apointer key,apointer value);
   public$ apointer get(aconstpointer key);
   public$ aboolean remove(aconstpointer key);
   public$ void     removeAll();
   public$ aboolean contains(aconstpointer key);
   public$ auint    size();
   public$ void     foreach (AHFunc func,apointer user_data);
   public$ void     iterInit(HashIter *iter);
   public$ aboolean iterNext(HashIter *iter,apointer *key,apointer *value);
   public$ AList   *getKeys();
   public$ AList   *getValues();
   public$ aboolean replace (apointer key,apointer value);

};




#endif /* __N_MEM_H__ */

