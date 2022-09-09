

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include "nmem.h"
/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

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

#include "nqsort.h"


#define ALIGNOF_GUINT32 4

#define ALIGNOF_GUINT64 8

#define ALIGNOF_UNSIGNED_LONG 8

struct msort_param
{
  size_t s;
  size_t var;
  NCompareDataFunc cmp;
  void *arg;
  char *t;
};

static void msort_with_tmp (const struct msort_param *p, void *b, size_t n);

static void msort_with_tmp (const struct msort_param *p, void *b, size_t n)
{
  char *b1, *b2;
  size_t n1, n2;
  char *tmp = p->t;
  const size_t s = p->s;
  NCompareDataFunc cmp = p->cmp;
  void *arg = p->arg;

  if (n <= 1)
    return;

  n1 = n / 2;
  n2 = n - n1;
  b1 = b;
  b2 = (char *) b + (n1 * p->s);

  msort_with_tmp (p, b1, n1);
  msort_with_tmp (p, b2, n2);

  switch (p->var)
    {
    case 0:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(nuint32 *) tmp = *(nuint32 *) b1;
	      b1 += sizeof (nuint32);
	      --n1;
	    }
	  else
	    {
	      *(nuint32 *) tmp = *(nuint32 *) b2;
	      b2 += sizeof (nuint32);
	      --n2;
	    }
	  tmp += sizeof (nuint32);
	}
      break;
    case 1:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(nuint64 *) tmp = *(nuint64 *) b1;
	      b1 += sizeof (nuint64);
	      --n1;
	    }
	  else
	    {
	      *(nuint64 *) tmp = *(nuint64 *) b2;
	      b2 += sizeof (nuint64);
	      --n2;
	    }
	  tmp += sizeof (nuint64);
	}
      break;
    case 2:
      while (n1 > 0 && n2 > 0)
	{
	  unsigned long *tmpl = (unsigned long *) tmp;
	  unsigned long *bl;

	  tmp += s;
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      bl = (unsigned long *) b1;
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      bl = (unsigned long *) b2;
	      b2 += s;
	      --n2;
	    }
	  while (tmpl < (unsigned long *) tmp)
	    *tmpl++ = *bl++;
	}
      break;
    case 3:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (*(const void **) b1, *(const void **) b2, arg) <= 0)
	    {
	      *(void **) tmp = *(void **) b1;
	      b1 += sizeof (void *);
	      --n1;
	    }
	  else
	    {
	      *(void **) tmp = *(void **) b2;
	      b2 += sizeof (void *);
	      --n2;
	    }
	  tmp += sizeof (void *);
	}
      break;
    default:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      memcpy (tmp, b1, s);
	      tmp += s;
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      memcpy (tmp, b2, s);
	      tmp += s;
	      b2 += s;
	      --n2;
	    }
	}
      break;
    }

  if (n1 > 0)
    memcpy (tmp, b1, n1 * s);
  memcpy (b, p->t, (n - n2) * s);
}


static void msort_r (void *b, size_t n, size_t s, NCompareDataFunc cmp, void *arg)
{
  size_t size = n * s;
  char *tmp = NULL;
  struct msort_param p;

  /* For large object sizes use indirect sorting.  */
  if (s > 32)
    size = 2 * n * sizeof (void *) + s;

  if (size < 1024)
    /* The temporary array is small, so put it on the stack.  */
    p.t = alloca (size);
  else
    {
      /* It's large, so malloc it.  */
      tmp = n_malloc (size);
      p.t = tmp;
    }

  p.s = s;
  p.var = 4;
  p.cmp = cmp;
  p.arg = arg;

  if (s > 32)
    {
      /* Indirect sorting.  */
      char *ip = (char *) b;
      void **tp = (void **) (p.t + n * sizeof (void *));
      void **t = tp;
      void *tmp_storage = (void *) (tp + n);
      char *kp;
      size_t i;

      while ((void *) t < tmp_storage)
	{
	  *t++ = ip;
	  ip += s;
	}
      p.s = sizeof (void *);
      p.var = 3;
      msort_with_tmp (&p, p.t + n * sizeof (void *), n);

      /* tp[0] .. tp[n - 1] is now sorted, copy around entries of
	 the original array.  Knuth vol. 3 (2nd ed.) exercise 5.2-10.  */
      for (i = 0, ip = (char *) b; i < n; i++, ip += s)
	if ((kp = tp[i]) != ip)
	  {
	    size_t j = i;
	    char *jp = ip;
	    memcpy (tmp_storage, ip, s);

	    do
	      {
		size_t k = (kp - (char *) b) / s;
		tp[j] = jp;
		memcpy (jp, kp, s);
		j = k;
		jp = kp;
		kp = tp[k];
	      }
	    while (kp != ip);

	    tp[j] = jp;
	    memcpy (jp, tmp_storage, s);
	  }
    }
  else
    {
      if ((s & (sizeof (nuint32) - 1)) == 0
	  && ((char *) b - (char *) 0) % ALIGNOF_GUINT32 == 0)
	{
	  if (s == sizeof (nuint32))
	    p.var = 0;
	  else if (s == sizeof (nuint64)
		   && ((char *) b - (char *) 0) % ALIGNOF_GUINT64 == 0)
	    p.var = 1;
	  else if ((s & (sizeof (unsigned long) - 1)) == 0
		   && ((char *) b - (char *) 0)
		      % ALIGNOF_UNSIGNED_LONG == 0)
	    p.var = 2;
	}
      msort_with_tmp (&p, b, n);
    }
  n_free (tmp);
}


void n_qsort_with_data (nconstpointer  pbase,nint total_elems,nsize size,NCompareDataFunc compare_func,npointer user_data)
{
  msort_r ((npointer)pbase, total_elems, size, compare_func, user_data);
}
