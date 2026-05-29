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

#include <pthread.h>
#include "aatomic.h"

aint (a_atomic_int_get) (const volatile aint *atomic)
{
  return a_atomic_int_get (atomic);
}

void (a_atomic_int_set) (volatile aint *atomic,
                    aint           newval)
{
  a_atomic_int_set (atomic, newval);
}


void (a_atomic_int_inc) (volatile aint *atomic)
{
  a_atomic_int_inc (atomic);
}


aboolean (a_atomic_int_dec_and_test) (volatile aint *atomic)
{
  return a_atomic_int_dec_and_test (atomic);
}

/**
 * a_atomic_int_compare_and_exchange:
 * @atomic: a pointer to a #aint or #auint
 * @oldval: the value to compare with
 * @newval: the value to conditionally replace with
 *
 * Compares @atomic to @oldval and, if equal, sets it to @newval.
 * If @atomic was not equal to @oldval then no change occurs.
 *
 * This compare and exchange is done atomically.
 *
 * Think of this operation as an atomic version of
 * `{ if (*atomic == oldval) { *atomic = newval; return TRUE; } else return FALSE; }`.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the exchange took place
 *
 * Since: 2.4
 **/
aboolean
(a_atomic_int_compare_and_exchange) (volatile aint *atomic,
                                     aint           oldval,
                                     aint           newval)
{
  return a_atomic_int_compare_and_exchange (atomic, oldval, newval);
}


aint
(a_atomic_int_add) (volatile aint *atomic,
                    aint           val)
{
  return a_atomic_int_add (atomic, val);
}


auint
(a_atomic_int_and) (volatile auint *atomic,
                    auint           val)
{
  return a_atomic_int_and (atomic, val);
}


auint
(a_atomic_int_or) (volatile auint *atomic,
                   auint           val)
{
  return a_atomic_int_or (atomic, val);
}


auint
(a_atomic_int_xor) (volatile auint *atomic,
                    auint           val)
{
  return a_atomic_int_xor (atomic, val);
}


apointer
(a_atomic_pointer_get) (const volatile void *atomic)
{
  return a_atomic_pointer_get ((const volatile apointer *) atomic);
}


void
(a_atomic_pointer_set) (volatile void *atomic,
                        apointer       newval)
{
  a_atomic_pointer_set ((volatile apointer *) atomic, newval);
}


aboolean
(a_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         apointer       oldval,
                                         apointer       newval)
{
  return a_atomic_pointer_compare_and_exchange ((volatile apointer *) atomic,
                                                oldval, newval);
}


assize
(a_atomic_pointer_add) (volatile void *atomic,
                        assize         val)
{
  return a_atomic_pointer_add ((volatile apointer *) atomic, val);
}


asize
(a_atomic_pointer_and) (volatile void *atomic,
                        asize          val)
{
  return a_atomic_pointer_and ((volatile apointer *) atomic, val);
}


asize
(a_atomic_pointer_or) (volatile void *atomic,
                       asize          val)
{
  return a_atomic_pointer_or ((volatile apointer *) atomic, val);
}


asize
(a_atomic_pointer_xor) (volatile void *atomic,
                        asize          val)
{
  return a_atomic_pointer_xor ((volatile apointer *) atomic, val);
}
