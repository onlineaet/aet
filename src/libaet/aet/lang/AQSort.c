
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "amem.h"
#include "AQSort.h"


#define ALIGNOF_AUINT32 4

#define ALIGNOF_AUINT64 8

#define ALIGNOF_UNSIGNED_LONG 8


struct msort_param
{
  size_t s;
  size_t var;
  ACompareDataFunc cmp;
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
  ACompareDataFunc cmp = p->cmp;
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
	      *(auint32 *) tmp = *(auint32 *) b1;
	      b1 += sizeof (auint32);
	      --n1;
	    }
	  else
	    {
	      *(auint32 *) tmp = *(auint32 *) b2;
	      b2 += sizeof (auint32);
	      --n2;
	    }
	  tmp += sizeof (auint32);
	}
      break;
    case 1:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(auint64 *) tmp = *(auint64 *) b1;
	      b1 += sizeof (auint64);
	      --n1;
	    }
	  else
	    {
	      *(auint64 *) tmp = *(auint64 *) b2;
	      b2 += sizeof (auint64);
	      --n2;
	    }
	  tmp += sizeof (auint64);
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


static void msort_r (void *b, size_t n, size_t s, ACompareDataFunc cmp, void *arg)
{
  size_t size = n * s;
  char *tmp = NULL;
  struct msort_param p;

  /* For large object sizes use indirect sorting.  */
  if (s > 32)
    size = 2 * n * sizeof (void *) + s;

  if (size < 1024)
    /* The temporary array is small, so put it on the stack.  */
    p.t = a_alloca (size);
  else
    {
      /* It's large, so malloc it.  */
      tmp = a_malloc (size);
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
      if ((s & (sizeof (auint32) - 1)) == 0
	  && ((char *) b - (char *) 0) % ALIGNOF_AUINT32 == 0)
	{
	  if (s == sizeof (auint32))
	    p.var = 0;
	  else if (s == sizeof (auint64)
		   && ((char *) b - (char *) 0) % ALIGNOF_AUINT64 == 0)
	    p.var = 1;
	  else if ((s & (sizeof (unsigned long) - 1)) == 0
		   && ((char *) b - (char *) 0)
		      % ALIGNOF_UNSIGNED_LONG == 0)
	    p.var = 2;
	}
      msort_with_tmp (&p, b, n);
    }
  a_free (tmp);
}

impl$ AQSort{
   static void sort(aconstpointer pbase,aint totalElems,asize elementSize,
		   ACompareDataFunc compareFunc,apointer userData){
     msort_r ((apointer)pbase, totalElems, elementSize, compareFunc, userData);
   }
};
