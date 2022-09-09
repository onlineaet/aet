
#ifndef __INNER_LOCK_H__
#define __INNER_LOCK_H__

#include "abase.h"
#include "aatomic.h"

typedef union  _InnerMutex          InnerMutex;

union _InnerMutex
{
  /*< private >*/
  apointer p;
  auint i[2];
};

#define INNER_LOCK_NAME(name)             a__ ## name ## _lock
#define INNER_LOCK_DEFINE_STATIC(name)    static INNER_LOCK_DEFINE (name)
#define INNER_LOCK_DEFINE(name)           InnerMutex INNER_LOCK_NAME (name)
#define INNER_LOCK_EXTERN(name)           extern InnerMutex INNER_LOCK_NAME (name)


#define INNER_LOCK(name) inner_mutex_lock       (&INNER_LOCK_NAME (name))
#define INNER_UNLOCK(name) inner_mutex_unlock   (&INNER_LOCK_NAME (name))
#define INNER_TRYLOCK(name) inner_mutex_trylock (&INNER_LOCK_NAME (name))



void            inner_mutex_init       (InnerMutex         *mutex);
void            inner_mutex_clear      (InnerMutex         *mutex);
void            inner_mutex_lock       (InnerMutex         *mutex);
aboolean        inner_mutex_trylock    (InnerMutex         *mutex);
void            inner_mutex_unlock     (InnerMutex         *mutex);
apointer        inner_mutex_get_impl   (InnerMutex *mutex);




////////////////////////////////////////////

typedef struct _InnerCond           InnerCond;
struct _InnerCond
{
  apointer p;
  auint i[2];
};

void            inner_cond_init                     (InnerCond  *cond);
void            inner_cond_clear                    (InnerCond  *cond);
void            inner_cond_wait                     (InnerCond  *cond,InnerMutex *mutex);
void            inner_cond_signal                   (InnerCond  *cond);
void            inner_cond_broadcast                (InnerCond  *cond);
aboolean        inner_cond_wait_until               (InnerCond  *cond,InnerMutex *mutex,aint64 end_time);


/////////////////////////InnerThread
typedef enum
{
  INNER_THREAD_ERROR_AGAIN /* Resource temporarily unavailable */
} InnerThreadError;

typedef apointer (*InnerThreadFunc) (apointer data);


auint inner_thread_n_created (void);


////////////////////////////////////////////////////


typedef enum
{
  INNER_ONCE_STATUS_NOTCALLED,
  INNER_ONCE_STATUS_PROGRESS,
  INNER_ONCE_STATUS_READY
} INNEROnceStatus;

typedef struct _InnerOnce InnerOnce;

struct _InnerOnce
{
  volatile INNEROnceStatus status;
  volatile apointer retval;
};

#define INNER_ONCE_INIT { INNER_ONCE_STATUS_NOTCALLED, NULL }


apointer        a_once_impl(InnerOnce *once,InnerThreadFunc func,apointer arg);
aboolean        a_once_init_enter(volatile void  *location);
void            a_once_init_leave(volatile void  *location, asize result);

#define a_once(once, func, arg) a_once_impl ((once), (func), (arg))


#define a_once_init_enter(location) \
  (A_GNUC_EXTENSION ({                                               \
    A_STATIC_ASSERT (sizeof *(location) == sizeof (apointer));       \
    (void) (0 ? (apointer) *(location) : NULL);                      \
    (!a_atomic_pointer_get (location) &&                             \
     a_once_init_enter (location));                                  \
  }))

#define a_once_init_leave(location, result) \
  (A_GNUC_EXTENSION ({                                               \
    A_STATIC_ASSERT (sizeof *(location) == sizeof (apointer));       \
    0 ? (void) (*(location) = (result)) : (void) 0;                  \
    a_once_init_leave ((location), (asize) (result));                \
  }))



#endif /* __A_MESSAGES_H__ */


