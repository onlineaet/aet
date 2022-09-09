
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include "aatomic.h"
#include "amem.h"
#include "alog.h"
#include "datastruct.h"
#include "innerlock.h"

static void a_thread_abort (aint status,const achar *function)
{
  fprintf (stderr, "ALib (thread-posix.c): Unexpected error from C library during '%s': %s.  Aborting.\n",
           function, xstrerror (status));
  a_abort ();
}


static pthread_mutex_t *creatPthread (void)
{
  pthread_mutexattr_t *pattr = NULL;
  pthread_mutex_t *mutex;
  aint status;
  mutex = a_malloc (sizeof (pthread_mutex_t));
  if A_UNLIKELY (mutex == NULL)
    a_thread_abort (errno, "malloc");
  if A_UNLIKELY ((status = pthread_mutex_init (mutex, pattr)) != 0)
    a_thread_abort (status, "pthread_mutex_init");
  return mutex;
}

static void freePThread (pthread_mutex_t *mutex)
{
  pthread_mutex_destroy (mutex);
  free (mutex);
}



apointer inner_mutex_get_impl(InnerMutex *mutex)
{
  pthread_mutex_t *impl = a_atomic_pointer_get (&mutex->p);
  if A_UNLIKELY (impl == NULL)
    {
      impl = creatPthread ();
      if (!a_atomic_pointer_compare_and_exchange (&mutex->p, NULL, impl))
    	  freePThread(impl);
      impl = mutex->p;
    }

  return impl;
}

void inner_mutex_init (InnerMutex *mutex)
{
  mutex->p = creatPthread();
}

void inner_mutex_clear (InnerMutex *mutex)
{
	freePThread (mutex->p);
}

void inner_mutex_lock (InnerMutex *mutex)
{
  aint status;

  if A_UNLIKELY ((status = pthread_mutex_lock (inner_mutex_get_impl (mutex))) != 0)
    a_thread_abort (status, "pthread_mutex_lock");
}

void inner_mutex_unlock (InnerMutex *mutex)
{
  aint status;
  if A_UNLIKELY ((status = pthread_mutex_unlock (inner_mutex_get_impl (mutex))) != 0)
    a_thread_abort (status, "pthread_mutex_unlock");
}

aboolean inner_mutex_trylock (InnerMutex *mutex)
{
  aint status;

  if A_LIKELY ((status = pthread_mutex_trylock (inner_mutex_get_impl(mutex))) == 0)
    return TRUE;

  if A_UNLIKELY (status != EBUSY)
    a_thread_abort (status, "pthread_mutex_trylock");

  return FALSE;
}

////////////////////////////////////////////////InnerOnce-----------------------


static InnerMutex inner_once_mutex;
static InnerCond     inner_once_cond;
static DSList   *inner_once_init_list = NULL;



apointer a_once_impl (InnerOnce *once,InnerThreadFunc  func, apointer     arg)
{
    inner_mutex_lock (&inner_once_mutex);
    while (once->status == INNER_ONCE_STATUS_PROGRESS)
      inner_cond_wait (&inner_once_cond, &inner_once_mutex);

    if (once->status != INNER_ONCE_STATUS_READY){
       apointer retval;
       once->status = INNER_ONCE_STATUS_PROGRESS;
       inner_mutex_unlock (&inner_once_mutex);
       retval = func (arg);
       inner_mutex_lock (&inner_once_mutex);

       once->retval = retval;
       once->status = INNER_ONCE_STATUS_READY;
       inner_cond_broadcast (&inner_once_cond);
    }
    inner_mutex_unlock (&inner_once_mutex);
    return once->retval;
}


aboolean (a_once_init_enter) (volatile void *location)
{
  volatile asize *value_location = location;
  aboolean need_init = FALSE;
  inner_mutex_lock (&inner_once_mutex);
  if (a_atomic_pointer_get (value_location) == 0)
    {
      if (!d_slist_find (inner_once_init_list, (void*) value_location))
        {
          need_init = TRUE;
          inner_once_init_list = d_slist_prepend (inner_once_init_list, (void*) value_location);
        }
      else
        do
          inner_cond_wait (&inner_once_cond, &inner_once_mutex);
        while (d_slist_find (inner_once_init_list, (void*) value_location));
    }
  inner_mutex_unlock (&inner_once_mutex);
  return need_init;
}


void (a_once_init_leave) (volatile void *location,asize          result)
{
   volatile asize *value_location = location;
   a_return_if_fail (a_atomic_pointer_get (value_location) == 0);
   a_return_if_fail (result != 0);
   a_atomic_pointer_set (value_location, result);
   inner_mutex_lock (&inner_once_mutex);
   a_return_if_fail (inner_once_init_list != NULL);
   inner_once_init_list = d_slist_remove (inner_once_init_list, (void*) value_location);
   inner_cond_broadcast (&inner_once_cond);
   inner_mutex_unlock (&inner_once_mutex);
}


////////////////////////////////InnerCond-------------------------------------
static pthread_cond_t *createCond (void)
{
  pthread_condattr_t attr;
  pthread_cond_t *cond;
  aint status;

  pthread_condattr_init (&attr);


  if A_UNLIKELY ((status = pthread_condattr_setclock (&attr, CLOCK_MONOTONIC)) != 0)
    a_thread_abort (status, "pthread_condattr_setclock");


  cond = a_malloc (sizeof (pthread_cond_t));
  if A_UNLIKELY (cond == NULL)
    a_thread_abort (errno, "malloc");

  if A_UNLIKELY ((status = pthread_cond_init (cond, &attr)) != 0)
     a_thread_abort (status, "pthread_cond_init");

  pthread_condattr_destroy (&attr);

  return cond;
}

static void freeCond(pthread_cond_t *cond)
{
  pthread_cond_destroy (cond);
  free (cond);
}

static inline pthread_cond_t *inner_cond_get_impl (InnerCond *cond)
{
  pthread_cond_t *impl = a_atomic_pointer_get (&cond->p);

  if A_UNLIKELY (impl == NULL)
    {
      impl = createCond ();
      if (!a_atomic_pointer_compare_and_exchange (&cond->p, NULL, impl))
    	  freeCond(impl);
      impl = cond->p;
    }

  return impl;
}

static void freeCcond (pthread_cond_t *cond)
{
  pthread_cond_destroy (cond);
  free (cond);
}

void inner_cond_init (InnerCond *cond)
{
  cond->p = createCond ();
}

void inner_cond_clear (InnerCond *cond)
{
	freeCcond (cond->p);
}

void inner_cond_wait (InnerCond  *cond,InnerMutex *mutex)
{
  aint status;

  if A_UNLIKELY ((status = pthread_cond_wait (inner_cond_get_impl (cond), inner_mutex_get_impl (mutex))) != 0)
    a_thread_abort (status, "pthread_cond_wait");
}


void inner_cond_signal (InnerCond *cond)
{
  aint status;

  if A_UNLIKELY ((status = pthread_cond_signal (inner_cond_get_impl (cond))) != 0)
    a_thread_abort (status, "pthread_cond_signal");
}


void inner_cond_broadcast (InnerCond *cond)
{
  aint status;

  if A_UNLIKELY ((status = pthread_cond_broadcast (inner_cond_get_impl (cond))) != 0)
    a_thread_abort (status, "pthread_cond_broadcast");
}


aboolean inner_cond_wait_until (InnerCond  *cond,InnerMutex *mutex,aint64  end_time)
{
    struct timespec ts;
    aint status;
    ts.tv_sec = end_time / 1000000;
    ts.tv_nsec = (end_time % 1000000) * 1000;

    if ((status = pthread_cond_timedwait (inner_cond_get_impl (cond), inner_mutex_get_impl (mutex), &ts)) == 0)
      return TRUE;

    if A_UNLIKELY (status != ETIMEDOUT)
      a_thread_abort (status, "pthread_cond_timedwait");
    return FALSE;
}

////////////////////////////////innerCreateThread
static volatile auint n_thread_n_created_counter = 0;


auint inner_thread_n_created (void)
{
  return a_atomic_int_get (&n_thread_n_created_counter);
}

