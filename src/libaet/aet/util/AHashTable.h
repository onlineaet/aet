

#ifndef __AET_UTIL_A_HASH_TABLE_H__
#define __AET_UTIL_A_HASH_TABLE_H__

#include <stdarg.h>
#include <objectlib.h>
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

