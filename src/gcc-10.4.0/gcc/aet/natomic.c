/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

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

#include "natomic.h"



#include <pthread.h>

static pthread_mutex_t n_atomic_lock = PTHREAD_MUTEX_INITIALIZER;

nint (n_atomic_int_get) (const volatile nint *atomic)
{
  nint value;

  pthread_mutex_lock (&n_atomic_lock);
  value = *atomic;
  pthread_mutex_unlock (&n_atomic_lock);

  return value;
}

void (n_atomic_int_set) (volatile nint *atomic,
                    nint           value)
{
  pthread_mutex_lock (&n_atomic_lock);
  *atomic = value;
  pthread_mutex_unlock (&n_atomic_lock);
}

void (n_atomic_int_inc) (volatile nint *atomic)
{
  pthread_mutex_lock (&n_atomic_lock);
  (*atomic)++;
  pthread_mutex_unlock (&n_atomic_lock);
}

nboolean (n_atomic_int_dec_and_test) (volatile nint *atomic)
{
  nboolean is_zero;

  pthread_mutex_lock (&n_atomic_lock);
  is_zero = --(*atomic) == 0;
  pthread_mutex_unlock (&n_atomic_lock);

  return is_zero;
}

nboolean (n_atomic_int_compare_and_exchange) (volatile nint *atomic, nint oldval, nint newval)
{
  nboolean success;

  pthread_mutex_lock (&n_atomic_lock);

  if ((success = (*atomic == oldval)))
    *atomic = newval;

  pthread_mutex_unlock (&n_atomic_lock);

  return success;
}

nint (n_atomic_int_add) (volatile nint *atomic,nint val)
{
  nint oldval;
  pthread_mutex_lock (&n_atomic_lock);
  oldval = *atomic;
  *atomic = oldval + val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}

nuint (n_atomic_int_and) (volatile nuint *atomic,nuint val)
{
  nuint oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *atomic;
  *atomic = oldval & val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}

nuint (n_atomic_int_or) (volatile nuint *atomic,nuint           val)
{
  nuint oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *atomic;
  *atomic = oldval | val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}

nuint (n_atomic_int_xor) (volatile nuint *atomic,nuint val)
{
  nuint oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *atomic;
  *atomic = oldval ^ val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}


npointer (n_atomic_pointer_get) (const volatile void *atomic)
{
  const volatile npointer *ptr = atomic;
  npointer value;
  pthread_mutex_lock (&n_atomic_lock);
  value = *ptr;
  pthread_mutex_unlock (&n_atomic_lock);
  return value;
}

void (n_atomic_pointer_set) (volatile void *atomic,npointer newval)
{
  volatile npointer *ptr = atomic;
  pthread_mutex_lock (&n_atomic_lock);
  *ptr = newval;
  pthread_mutex_unlock (&n_atomic_lock);
}

nboolean (n_atomic_pointer_compare_and_exchange) (volatile void *atomic,npointer oldval,npointer newval)
{
  volatile npointer *ptr = atomic;
  nboolean success;

  pthread_mutex_lock (&n_atomic_lock);

  if ((success = (*ptr == oldval)))
    *ptr = newval;

  pthread_mutex_unlock (&n_atomic_lock);

  return success;
}

nssize (n_atomic_pointer_add) (volatile void *atomic,nssize  val)
{
  volatile nssize *ptr = atomic;
  nssize oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *ptr;
  *ptr = oldval + val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}

nsize (n_atomic_pointer_and) (volatile void *atomic, nsize val)
{
  volatile nsize *ptr = atomic;
  nsize oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *ptr;
  *ptr = oldval & val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}

nsize (n_atomic_pointer_or) (volatile void *atomic, nsize  val)
{
  volatile nsize *ptr = atomic;
  nsize oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *ptr;
  *ptr = oldval | val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}

nsize (n_atomic_pointer_xor) (volatile void *atomic, nsize val)
{
  volatile nsize *ptr = atomic;
  nsize oldval;

  pthread_mutex_lock (&n_atomic_lock);
  oldval = *ptr;
  *ptr = oldval ^ val;
  pthread_mutex_unlock (&n_atomic_lock);

  return oldval;
}



