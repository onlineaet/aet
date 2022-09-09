

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


