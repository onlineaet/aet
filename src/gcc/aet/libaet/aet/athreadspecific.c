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


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <pthread.h>
#include "athreadspecific.h"
#include "amem.h"
#include "aatomic.h"


static void a_thread_abort (aint status,const achar *function)
{
  fprintf (stderr, "ALib specific Unexpected error from C library during '%s': %s.  Aborting.\n",
           function, xstrerror (status));
  a_abort ();
}

static pthread_key_t *createSpecific (ADestroyNotify notify)
{
  pthread_key_t *key;
  aint status;

  key = a_malloc(sizeof (pthread_key_t));
  if A_UNLIKELY (key == NULL)
    a_thread_abort (errno, "malloc");
  status = pthread_key_create (key, notify);
  if A_UNLIKELY (status != 0)
    a_thread_abort (status, "pthread_key_create");

  return key;
}

static void freeSpecific (pthread_key_t *key)
{
  aint status;

  status = pthread_key_delete (*key);
  if A_UNLIKELY (status != 0)
    a_thread_abort (status, "pthread_key_delete");
  free (key);
}



static inline pthread_key_t * a_thread_specific_get_impl (AThreadSpecific *key)
{
  pthread_key_t *impl = a_atomic_pointer_get (&key->p);

  if A_UNLIKELY (impl == NULL)
    {
      impl = createSpecific (key->notify);
      if (!a_atomic_pointer_compare_and_exchange (&key->p, NULL, impl))
        {
    	  freeSpecific (impl);
          impl = key->p;
        }
    }

  return impl;
}


apointer a_thread_specific_get (AThreadSpecific *key)
{
  /* quote POSIX: No errors are returned from pthread_getspecific(). */
  return pthread_getspecific (*a_thread_specific_get_impl (key));
}


void a_thread_specific_set (AThreadSpecific *key,         apointer  value)
{
  aint status;
  if A_UNLIKELY ((status = pthread_setspecific (*a_thread_specific_get_impl (key), value)) != 0)
    a_thread_abort (status, "pthread_setspecific");
}


void a_thread_specific_replace (AThreadSpecific *key,            apointer  value)
{
  pthread_key_t *impl = a_thread_specific_get_impl (key);
  apointer old;
  aint status;

  old = pthread_getspecific (*impl);

  if A_UNLIKELY ((status = pthread_setspecific (*impl, value)) != 0)
    a_thread_abort (status, "pthread_setspecific");

  if (old && key->notify)
    key->notify (old);
}

apointer a_thread_specific_set_alloc0 (AThreadSpecific *key,asize size)
{
  apointer allocated = a_malloc0 (size);
  a_thread_specific_set (key, allocated);
  return a_steal_pointer (&allocated);
}
