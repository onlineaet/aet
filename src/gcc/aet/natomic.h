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


#ifndef __N_ATOMIC_H__
#define __N_ATOMIC_H__



#include "nbase.h"



nint                    n_atomic_int_get                      (const volatile nint *atomic);

void                    n_atomic_int_set                      (volatile nint  *atomic,
                                                               nint            newval);

void                    n_atomic_int_inc                      (volatile nint  *atomic);

nboolean                n_atomic_int_dec_and_test             (volatile nint  *atomic);

nboolean                n_atomic_int_compare_and_exchange     (volatile nint  *atomic,
                                                               nint            oldval,
                                                               nint            newval);

nint                    n_atomic_int_add                      (volatile nint  *atomic,
                                                               nint            val);
nuint                   n_atomic_int_and                      (volatile nuint *atomic,
                                                               nuint           val);
nuint                   n_atomic_int_or                       (volatile nuint *atomic,
                                                               nuint           val);

nuint                   n_atomic_int_xor                      (volatile nuint *atomic,
                                                               nuint           val);


npointer                n_atomic_pointer_get                  (const volatile void *atomic);

void                    n_atomic_pointer_set                  (volatile void  *atomic,
                                                               npointer        newval);

nboolean                n_atomic_pointer_compare_and_exchange (volatile void  *atomic,
                                                               npointer        oldval,
                                                               npointer        newval);

nssize                  n_atomic_pointer_add                  (volatile void  *atomic,
                                                               nssize          val);

nsize                   n_atomic_pointer_and                  (volatile void  *atomic,
                                                               nsize           val);

nsize                   n_atomic_pointer_or                   (volatile void  *atomic,
                                                               nsize           val);

nsize                   n_atomic_pointer_xor                  (volatile void  *atomic,
                                                               nsize           val);


#define n_atomic_int_get(atomic) \
  (n_atomic_int_get ((nint *) (atomic)))
#define n_atomic_int_set(atomic, newval) \
  (n_atomic_int_set ((nint *) (atomic), (nint) (newval)))
#define n_atomic_int_compare_and_exchange(atomic, oldval, newval) \
  (n_atomic_int_compare_and_exchange ((nint *) (atomic), (oldval), (newval)))
#define n_atomic_int_add(atomic, val) \
  (n_atomic_int_add ((nint *) (atomic), (val)))
#define n_atomic_int_and(atomic, val) \
  (n_atomic_int_and ((nuint *) (atomic), (val)))
#define n_atomic_int_or(atomic, val) \
  (n_atomic_int_or ((nuint *) (atomic), (val)))
#define n_atomic_int_xor(atomic, val) \
  (n_atomic_int_xor ((nuint *) (atomic), (val)))
#define n_atomic_int_inc(atomic) \
  (n_atomic_int_inc ((nint *) (atomic)))
#define n_atomic_int_dec_and_test(atomic) \
  (n_atomic_int_dec_and_test ((nint *) (atomic)))

#define n_atomic_pointer_get(atomic) \
  (n_atomic_pointer_get (atomic))
#define n_atomic_pointer_set(atomic, newval) \
  (n_atomic_pointer_set ((atomic), (npointer) (newval)))
#define n_atomic_pointer_compare_and_exchange(atomic, oldval, newval) \
  (n_atomic_pointer_compare_and_exchange ((atomic), (npointer) (oldval), (npointer) (newval)))
#define n_atomic_pointer_add(atomic, val) \
  (n_atomic_pointer_add ((atomic), (nssize) (val)))
#define n_atomic_pointer_and(atomic, val) \
  (n_atomic_pointer_and ((atomic), (nsize) (val)))
#define n_atomic_pointer_or(atomic, val) \
  (n_atomic_pointer_or ((atomic), (nsize) (val)))
#define n_atomic_pointer_xor(atomic, val) \
  (n_atomic_pointer_xor ((atomic), (nsize) (val)))


#endif /* __N_ATOMIC_H__ */
