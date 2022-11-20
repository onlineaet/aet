#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "AHashTable.h"
#include "AAssert.h"


struct _AHashTablePriv
{
  asize            size;
  aint             mod;
  auint            mask;
  aint             nnodes;
  aint             noccupied;  /* nnodes + tombstones */

  auint            have_big_keys : 1;
  auint            have_big_values : 1;

  apointer         keys;
  auint           *hashes;
  apointer         values;

  AHashFunc        hashFunc;
  AEqualFunc       keyEqualFunc;
  ADestroyNotify   keyDestroyFunc;
  ADestroyNotify   valueDestroyFunc;
};

static const aint prime_mod [] =
{
  1,          /* For 1 << 0 */
  2,
  3,
  7,
  13,
  31,
  61,
  127,
  251,
  509,
  1021,
  2039,
  4093,
  8191,
  16381,
  32749,
  65521,      /* For 1 << 16 */
  131071,
  262139,
  524287,
  1048573,
  2097143,
  4194301,
  8388593,
  16777213,
  33554393,
  67108859,
  134217689,
  268435399,
  536870909,
  1073741789,
  2147483647  /* For 1 << 31 */
};

#define HASH_TABLE_MIN_SHIFT 3  /* 1 << 3 == 8 buckets */

#define UNUSED_HASH_VALUE 0
#define TOMBSTONE_HASH_VALUE 1
#define HASH_IS_UNUSED(h_) ((h_) == UNUSED_HASH_VALUE)
#define HASH_IS_TOMBSTONE(h_) ((h_) == TOMBSTONE_HASH_VALUE)
#define HASH_IS_REAL(h_) ((h_) >= 2)


static inline apointer realloc_key_or_value_array (apointer a, auint size, A_GNUC_UNUSED aboolean is_big)
{
  return a_renew (apointer, a, size);
}

static inline apointer fetch_key_or_value (apointer a, auint index, aboolean is_big)
{
  is_big = TRUE;
  return is_big ? *(((apointer *) a) + index) : AUINT_TO_POINTER (*(((auint *) a) + index));
}

static inline void assign_key_or_value (apointer a, auint index, aboolean is_big, apointer v)
{
  is_big = TRUE;
  if (is_big)
    *(((apointer *) a) + index) = v;
  else
    *(((auint *) a) + index) = APOINTER_TO_UINT (v);
}
/**
 * 驱逐，赶出，逐出;
 */
static inline apointer evict_key_or_value (apointer a, auint index, aboolean is_big, apointer v)
{
  is_big = TRUE;
  if (is_big)
    {
      apointer r = *(((apointer *) a) + index);
      *(((apointer *) a) + index) = v;
      return r;
    }
  else
    {
      apointer r = AUINT_TO_POINTER (*(((auint *) a) + index));
      *(((auint *) a) + index) = APOINTER_TO_UINT (v);
      return r;
    }
}


static inline aboolean get_status_bit (const auint32 *bitmap, auint index)
{
  return (bitmap[index / 32] >> (index % 32)) & 1;
}

static inline void set_status_bit (auint32 *bitmap, auint index)
{
  bitmap[index / 32] |= 1U << (index % 32);
}



static inline apointer reallocKeyOrValue(apointer a, auint size, A_GNUC_UNUSED aboolean is_big)
{
    return a_renew (apointer, a, size);
}

impl$ HashIter{
};

impl$ AHashTable{

   static void setShift(aint shift){
	  priv->size = 1 << shift;
	  priv->mod  = prime_mod [shift];

	  /* hash_table->size is always a power of two, so we can calculate the mask
	   * by simply subtracting 1 from it. The leading assertion ensures that
	   * we're really dealing with a power of two. */
	  a_assert ((priv->size & (priv->size - 1)) == 0);
	  priv->mask = priv->size - 1;
   }



   static void setupStorage(){
	  aboolean small = FALSE;
	  setShift (HASH_TABLE_MIN_SHIFT);
	  priv->have_big_keys = TRUE;
	  priv->have_big_values = TRUE;

	  priv->keys   = reallocKeyOrValue (NULL, priv->size, priv->have_big_keys);
	  priv->values = priv->keys;
	  priv->hashes = a_new0 (auint, priv->size);
   }

   static void init(AHashFunc hashFunc,AEqualFunc keyEqualFunc,ADestroyNotify keyDestroyFunc,ADestroyNotify valueDestroyFunc){
 	   priv=a_malloc(sizeof(AHashTablePriv));
 	   priv->hashFunc=hashFunc ? hashFunc : AHashTable.directHash;
 	   priv->keyEqualFunc=keyEqualFunc;
 	   priv->keyDestroyFunc=keyDestroyFunc;
 	   priv->valueDestroyFunc=valueDestroyFunc;
 	   priv->nnodes             = 0;
 	   priv->noccupied          = 0;
 	   setupStorage();
   }

   AHashTable (AHashFunc hashFunc,AEqualFunc keyEqualFunc,ADestroyNotify keyDestroyFunc,ADestroyNotify valueDestroyFunc){
      init(hashFunc,keyEqualFunc,keyDestroyFunc,valueDestroyFunc);
   }

   AHashTable(AHashFunc hashFunc,AEqualFunc keyEqualFunc){
      init(hashFunc,keyEqualFunc, NULL, NULL);
   }

   AHashTable(){
      init(AHashTable.strHash,AHashTable.strEqual,NULL,NULL);
   }

   static inline auint hashToIndex(auint hash){
     /* Multiply the hash by a small prime before applying the modulo. This
      * prevents the table from becoming densely packed, even with a poor hash
      * function. A densely packed table would have poor performance on
      * workloads with many failed lookups or a high degree of churn. */
      return (hash * 11) % priv->mod;
   }

   static inline auint lookupNode(aconstpointer  key,auint *hash_return){
      auint node_index;
      auint node_hash;
      auint hash_value;
      auint first_tombstone = 0;
      aboolean have_tombstone = FALSE;
      auint step = 0;

      hash_value = priv->hashFunc(key);//获取hash值
      if(A_UNLIKELY (!HASH_IS_REAL (hash_value)))
          hash_value = 2;

     *hash_return = hash_value;

      node_index = hashToIndex (hash_value);//hash值转成索引
      node_hash = priv->hashes[node_index];
      while (!HASH_IS_UNUSED (node_hash)){
         /* We first check if our full hash values
          * are equal so we can avoid calling the full-blown
          * key equality function in most cases.
          */
         if(node_hash == hash_value){
            apointer node_key = fetch_key_or_value (priv->keys, node_index, priv->have_big_keys);
            if (priv->keyEqualFunc){
               if(priv->keyEqualFunc (node_key, key))
                  return node_index;
            }else if (node_key == key){
               return node_index;
            }
         }else if (HASH_IS_TOMBSTONE (node_hash) && !have_tombstone){
             first_tombstone = node_index;
             have_tombstone = TRUE;
         }

         step++;
         node_index += step;
         node_index &= priv->mask;
         node_hash = priv->hashes[node_index];
      }

      if (have_tombstone)
         return first_tombstone;

      return node_index;
   }

   static inline void ensure_keyval_fits (apointer key, apointer value){
      aboolean is_a_set = (priv->keys == priv->values);
      /* Just split if necessary */
      if (is_a_set && key != value)
         priv->values = a_memdup (priv->keys, sizeof (apointer) * priv->size);
   }

   static aint findClosestShift(aint n){
      aint i;
      for (i = 0; n; i++)
         n >>= 1;

      return i;
   }

   static void setShiftFromSize (aint size){
      aint shift;
      shift = findClosestShift (size);
      shift = MAX (shift, HASH_TABLE_MIN_SHIFT);
      setShift (shift);
   }

   static void  reallocArrays (aboolean is_a_set){
      priv->hashes = a_renew (auint, priv->hashes, priv->size);
      priv->keys = realloc_key_or_value_array (priv->keys, priv->size, priv->have_big_keys);

     if (is_a_set)
       priv->values = priv->keys;
     else
       priv->values = realloc_key_or_value_array (priv->values, priv->size, priv->have_big_values);
   }

#define DEFINE_RESIZE_FUNC(fname) \
static void fname (auint old_size, auint32 *reallocated_buckets_bitmap) \
{                                                                       \
  auint i;                                                              \
                                                                        \
  for (i = 0; i < old_size; i++)                                        \
    {                                                                   \
      auint node_hash = priv->hashes[i];                                \
      apointer key, value A_GNUC_UNUSED;                                \
                                                                        \
      if (!HASH_IS_REAL (node_hash))                                    \
        {                                                               \
          /* Clear tombstones */                                        \
          priv->hashes[i] = UNUSED_HASH_VALUE;                          \
          continue;                                                     \
        }                                                               \
                                                                        \
      /* Skip entries relocated through eviction */                     \
      if (get_status_bit (reallocated_buckets_bitmap, i))               \
        continue;                                                       \
                                                                        \
      priv->hashes[i] = UNUSED_HASH_VALUE;                        \
      EVICT_KEYVAL (priv, i, NULL, NULL, key, value);             \
                                                                        \
      for (;;)                                                          \
        {                                                               \
          auint hash_val;                                               \
          auint replaced_hash;                                          \
          auint step = 0;                                               \
                                                                        \
          hash_val = hashToIndex(node_hash); \
                                                                        \
          while (get_status_bit (reallocated_buckets_bitmap, hash_val)) \
            {                                                           \
              step++;                                                   \
              hash_val += step;                                         \
              hash_val &= priv->mask;                             \
            }                                                           \
                                                                        \
          set_status_bit (reallocated_buckets_bitmap, hash_val);        \
                                                                        \
          replaced_hash = priv->hashes[hash_val];                 \
          priv->hashes[hash_val] = node_hash;                     \
          if (!HASH_IS_REAL (replaced_hash))                            \
            {                                                           \
              ASSIGN_KEYVAL (priv, hash_val, key, value);         \
              break;                                                    \
            }                                                           \
                                                                        \
          node_hash = replaced_hash;                                    \
          EVICT_KEYVAL (priv, hash_val, key, value, key, value);  \
        }                                                               \
    }                                                                   \
}

#define ASSIGN_KEYVAL(ht, index, key, value) A_STMT_START{ \
    assign_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
    assign_key_or_value ((ht)->values, (index), (ht)->have_big_values, (value)); \
  }A_STMT_END

#define EVICT_KEYVAL(ht, index, key, value, outkey, outvalue) A_STMT_START{ \
    (outkey) = evict_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
    (outvalue) = evict_key_or_value ((ht)->values, (index), (ht)->have_big_values, (value)); \
  }A_STMT_END

DEFINE_RESIZE_FUNC (resize_map)

#undef ASSIGN_KEYVAL
#undef EVICT_KEYVAL

#define ASSIGN_KEYVAL(ht, index, key, value) A_STMT_START{ \
    assign_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
  }A_STMT_END

#define EVICT_KEYVAL(ht, index, key, value, outkey, outvalue) A_STMT_START{ \
    (outkey) = evict_key_or_value ((ht)->keys, (index), (ht)->have_big_keys, (key)); \
  }A_STMT_END

DEFINE_RESIZE_FUNC (resize_set)

#undef ASSIGN_KEYVAL
#undef EVICT_KEYVAL


   static void resize (){
      auint32 *reallocated_buckets_bitmap;
      asize old_size;
      aboolean is_a_set;

      old_size = priv->size;
      is_a_set = priv->keys == priv->values;

     /* The outer checks in g_hash_table_maybe_resize() will only consider
      * cleanup/resize when the load factor goes below .25 (1/4, ignoring
      * tombstones) or above .9375 (15/16, including tombstones).
      *
      * Once this happens, tombstones will always be cleaned out. If our
      * load sans tombstones is greater than .75 (1/1.333, see below), we'll
      * take this opportunity to grow the table too.
      *
      * Immediately after growing, the load factor will be in the range
      * .375 .. .469. After shrinking, it will be exactly .5. */

      setShiftFromSize (priv->nnodes * 1.333);

      if (priv->size > old_size){
    	  reallocArrays (is_a_set);
         memset (&priv->hashes[old_size], 0, (priv->size - old_size) * sizeof (auint));
         reallocated_buckets_bitmap = a_new0 (auint32, (priv->size + 31) / 32);
      }else{
         reallocated_buckets_bitmap = a_new0 (auint32, (old_size + 31) / 32);
      }

      if (is_a_set)
         resize_set (old_size, reallocated_buckets_bitmap);
      else
         resize_map (old_size, reallocated_buckets_bitmap);

      a_free (reallocated_buckets_bitmap);

      if (priv->size < old_size)
         reallocArrays (is_a_set);

      priv->noccupied = priv->nnodes;
   }

   static inline void maybeResize(){
      aint noccupied = priv->noccupied;
      aint size = priv->size;
      if ((size > priv->nnodes * 4 && size > 1 << HASH_TABLE_MIN_SHIFT) || (size <= noccupied + (noccupied / 16)))
        resize ();
   }


   static aboolean putNode(auint node_index,auint key_hash,apointer new_key,
		   apointer new_value,aboolean keep_new_key,aboolean reusing_key){
      aboolean already_exists;
      auint old_hash;
      apointer key_to_free = NULL;
      apointer key_to_keep = NULL;
      apointer value_to_free = NULL;

      old_hash = priv->hashes[node_index];
      already_exists = HASH_IS_REAL (old_hash);

     /* Proceed in three steps.  First, deal with the key because it is the
      * most complicated.  Then consider if we need to split the table in
      * two (because writing the value will result in the set invariant
      * becoming broken).  Then deal with the value.
      *
      * There are three cases for the key:
      *
      *  - entry already exists in table, reusing key:
      *    free the just-passed-in new_key and use the existing value
      *
      *  - entry already exists in table, not reusing key:
      *    free the entry in the table, use the new key
      *
      *  - entry not already in table:
      *    use the new key, free nothing
      *
      * We update the hash at the same time...
      */
      if (already_exists){
         /* Note: we must record the old value before writing the new key
          * because we might change the value in the event that the two
          * arrays are shared.
          */
         value_to_free = fetch_key_or_value (priv->values, node_index, priv->have_big_values);
         if (keep_new_key){
             key_to_free = fetch_key_or_value (priv->keys, node_index, priv->have_big_keys);
             key_to_keep = new_key;
         }else{
             key_to_free = new_key;
             key_to_keep = fetch_key_or_value (priv->keys, node_index, priv->have_big_keys);
         }
      }else{
         priv->hashes[node_index] = key_hash;
         key_to_keep = new_key;
      }

     /* Resize key/value arrays and split table as necessary */
      ensure_keyval_fits (key_to_keep, new_value);
      assign_key_or_value (priv->keys, node_index, priv->have_big_keys, key_to_keep);

     /* Step 3: Actually do the write */
      assign_key_or_value (priv->values, node_index, priv->have_big_values, new_value);

     /* Now, the bookkeeping... */
      if (!already_exists){
         priv->nnodes++;
         if (HASH_IS_UNUSED (old_hash)){
             /* We replaced an empty node, and not a tombstone */
             priv->noccupied++;
             maybeResize ();
         }
      }

      if (already_exists){
         if (priv->keyDestroyFunc && !reusing_key)
           (* priv->keyDestroyFunc) (key_to_free);
         if (priv->valueDestroyFunc)
           (* priv->valueDestroyFunc) (value_to_free);
      }
      return !already_exists;
   }

   static aboolean putInternal (apointer key,apointer value,aboolean keep_new_key){
      auint key_hash;
      auint node_index;
      a_return_val_if_fail (self != NULL, FALSE);
      node_index = lookupNode(key, &key_hash);
      return putNode(node_index, key_hash, key, value, keep_new_key, FALSE);
   }

   aboolean put(apointer key,apointer value){
      return putInternal (key, value, FALSE);
   }

   apointer get(aconstpointer key){
	  auint node_index;
	  auint node_hash;
	  a_return_val_if_fail (self != NULL, NULL);
      node_index = lookupNode(key, &node_hash);
      return HASH_IS_REAL (priv->hashes[node_index])?fetch_key_or_value (priv->values, node_index, priv->have_big_values): NULL;
   }

   aboolean contains(aconstpointer key){
	  auint node_index;
	  auint node_hash;
	  a_return_val_if_fail (self != NULL, FALSE);
	  node_index = lookupNode(key, &node_hash);
	  return HASH_IS_REAL (priv->hashes[node_index]);
   }

   auint size (){
      a_return_val_if_fail (self != NULL, 0);
      return priv->nnodes;
   }

   static void removeAllNodes(aboolean notify,aboolean destruction){
      int i;
      apointer key;
      apointer value;
      aint old_size;
      apointer *old_keys;
      apointer *old_values;
      auint    *old_hashes;
      aboolean  old_have_big_keys;
      aboolean  old_have_big_values;

     /* If the hash table is already empty, there is nothing to be done. */
      if (priv->nnodes == 0)
         return;

      priv->nnodes = 0;
      priv->noccupied = 0;

       /* Easy case: no callbacks, so we just zero out the arrays */
      if (!notify || (priv->keyDestroyFunc == NULL && priv->valueDestroyFunc == NULL)){
         if (!destruction){
             memset (priv->hashes, 0, priv->size * sizeof (auint));
             memset (priv->keys, 0, priv->size * sizeof (apointer));
             memset (priv->values, 0, priv->size * sizeof (apointer));
         }
         return;
      }

     /* Hard case: we need to do user callbacks.  There are two
      * possibilities here:
      *
      *   1) there are no outstanding references on the table and therefore
      *   nobody should be calling into it again (destroying == true)
      *
      *   2) there are outstanding references, and there may be future
      *   calls into the table, either after we return, or from the destroy
      *   notifies that we're about to do (destroying == false)
      *
      * We handle both cases by taking the current state of the table into
      * local variables and replacing it with something else: in the "no
      * outstanding references" cases we replace it with a bunch of
      * null/zero values so that any access to the table will fail.  In the
      * "may receive future calls" case, we reinitialise the struct to
      * appear like a newly-created empty table.
      *
      * In both cases, we take over the references for the current state,
      * freeing them below.
      */
      old_size = priv->size;
      old_have_big_keys = priv->have_big_keys;
      old_have_big_values = priv->have_big_values;
      old_keys   = a_steal_pointer (&priv->keys);
      old_values = a_steal_pointer (&priv->values);
      old_hashes = a_steal_pointer (&priv->hashes);

      if (!destruction)
         /* Any accesses will see an empty table */
         setupStorage();
      else
       /* Will cause a quick crash on any attempted access */
         priv->size = priv->mod = priv->mask = 0;

       /* Now do the actual destroy notifies */
      for (i = 0; i < old_size; i++){
         if (HASH_IS_REAL (old_hashes[i])){
            key = fetch_key_or_value (old_keys, i, old_have_big_keys);
            value = fetch_key_or_value (old_values, i, old_have_big_values);
            old_hashes[i] = UNUSED_HASH_VALUE;
            assign_key_or_value (old_keys, i, old_have_big_keys, NULL);
            assign_key_or_value (old_values, i, old_have_big_values, NULL);

            if (priv->keyDestroyFunc != NULL)
               priv->keyDestroyFunc (key);

            if (priv->valueDestroyFunc != NULL)
               priv->valueDestroyFunc (value);
         }
      }

      /* Destroy old storage space. */
      if (old_keys != old_values)
        a_free (old_values);

      a_free (old_keys);
      a_free (old_hashes);
   }

   static void removeNode(aint i,aboolean notify){
      apointer key;
      apointer value;

      key =fetch_key_or_value (priv->keys, i, priv->have_big_keys);
      value = fetch_key_or_value (priv->values, i, priv->have_big_values);

     /* Erect tombstone */
      priv->hashes[i] = TOMBSTONE_HASH_VALUE;

     /* Be GC friendly */
      assign_key_or_value (priv->keys, i, priv->have_big_keys, NULL);
      assign_key_or_value (priv->values, i, priv->have_big_values, NULL);

      priv->nnodes--;

      if (notify && priv->keyDestroyFunc)
         priv->keyDestroyFunc (key);

      if (notify && priv->valueDestroyFunc)
         priv->valueDestroyFunc (value);

   }

   static aboolean removeInternal (aconstpointer  key,aboolean notify){
      auint node_index;
      auint node_hash;
      a_return_val_if_fail (self != NULL, FALSE);
      node_index = lookupNode (key, &node_hash);
      if (!HASH_IS_REAL (priv->hashes[node_index]))
         return FALSE;

      removeNode (node_index, notify);
      maybeResize();
      return TRUE;
   }

   aboolean remove (aconstpointer  key){
      return removeInternal (key, TRUE);
   }

   void removeAll (){
      a_return_if_fail (self != NULL);
      removeAllNodes(TRUE, FALSE);
      maybeResize();
   }

   void foreach (AHFunc func,apointer user_data){
      asize i;
      a_return_if_fail (self != NULL);
      a_return_if_fail (func != NULL);
      for (i = 0; i < priv->size; i++){
         auint node_hash = priv->hashes[i];
         apointer node_key = fetch_key_or_value (priv->keys, i, priv->have_big_keys);
         apointer node_value =fetch_key_or_value (priv->values, i, priv->have_big_values);
         if (HASH_IS_REAL (node_hash))
           (* func) (node_key, node_value, user_data);
      }
   }

   aboolean  iterNext(HashIter *iter,apointer *key,apointer *value){
	  aint position;
      if(iter->hashTable!=self)
         return FALSE;
	  a_return_val_if_fail (iter != NULL, FALSE);
	  a_return_val_if_fail (iter->position < (assize) priv->size, FALSE);
      position = iter->position;
      do{
	        position++;
	        if (position >= (assize) priv->size){
	        	iter->position = position;
	            return FALSE;
	        }
	  }while (!HASH_IS_REAL (priv->hashes[position]));

	  if (key != NULL)
	     *key = fetch_key_or_value (priv->keys, position, priv->have_big_keys);
	  if (value != NULL)
	     *value = fetch_key_or_value (priv->values, position, priv->have_big_values);
	  iter->position = position;
	  return TRUE;
   }

   void iterInit (HashIter *iter){
     a_return_if_fail (iter != NULL);
     a_return_if_fail (self != NULL);
     iter->hashTable = self;
     iter->position = -1;
   }

   AList *getKeys(){
      asize i;
      a_return_val_if_fail (self != NULL, NULL);
      AList *retval=new$ AList();
      for (i = 0; i < priv->size; i++){
         if (HASH_IS_REAL (priv->hashes[i]))
           retval->addFirst(fetch_key_or_value (priv->keys,i,priv->have_big_keys));
      }
      return retval;
   }

   AList *getValues(){
      asize i;
      a_return_val_if_fail (self != NULL, NULL);
      AList *retval=new$ AList();
      for (i = 0; i < priv->size; i++){
         if (HASH_IS_REAL (priv->hashes[i]))
           retval->addFirst(fetch_key_or_value (priv->values, i, priv->have_big_keys));
      }
      return retval;
   }

   aboolean replace (apointer key,apointer value) {
     return putInternal (key, value, TRUE);
   }

   /**
    * 释放前最后的清除工作
    */
   ~AHashTable(){
	 removeAllNodes (TRUE, TRUE);
	 if (priv->keys != priv->values)
	   a_free (priv->values);
	 a_free (priv->keys);
	 a_free (priv->hashes);
	 a_free(priv);
   }

   static aboolean strEqual (const char *string1,const char *string2){
      if(string1==NULL || string2==NULL)
    	 return FALSE;
      return strcmp (string1, string2) == 0;
   }

   static auint strHash(const char *str){
      const signed char *p;
      auint32 h = 5381;
      for (p = str; *p != '\0'; p++)
        h = (h << 5) + h + *p;
      return h;
   }

   static aboolean intEqual(const int *v1,const int *v2){
	   return *v1==*v2;
   }

   static auint intHash(const int *v1){
	   return *v1;
   }

   static aboolean directEqual(aconstpointer v1,aconstpointer v2){
	   return v1==v2;
   }

   static auint directHash(aconstpointer v1){
	   return APOINTER_TO_UINT(v1);
   }

};
