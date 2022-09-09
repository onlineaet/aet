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

#ifndef __N_BASE_H__
#define __N_BASE_H__

#define GCC_COMPILE_AET 1
#ifdef GCC_COMPILE_AET
#include "config.h"
#include "libiberty.h"
#include "system.h"
#else
#define xmalloc(s) malloc(s)
#define xrealloc(s,a) realloc(s,a)
#define xstrerror(s) strerror(s)
#define xcalloc(s,a) calloc(s,a)
#define ISDIGIT(s) isdigit(s)
#endif
#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <stddef.h>


#define N_MINFLOAT	FLT_MIN
#define N_MAXFLOAT	FLT_MAX
#define N_MINDOUBLE	DBL_MIN
#define N_MAXDOUBLE	DBL_MAX
#define N_MINSHORT	SHRT_MIN
#define N_MAXSHORT	SHRT_MAX
#define N_MAXUSHORT	USHRT_MAX
#define N_MININT	INT_MIN
#define N_MAXINT	INT_MAX
#define N_MAXUINT	UINT_MAX
#define N_MINLONG	LONG_MIN
#define N_MAXLONG	LONG_MAX
#define N_MAXULONG	ULONG_MAX

typedef signed char nint8;
typedef unsigned char nuint8;
typedef signed short nint16;
typedef unsigned short nuint16;
typedef signed int nint32;
typedef unsigned int nuint32;


typedef signed long long nint64;
typedef unsigned long long nuint64;

#define N_NINT64_CONSTANT(val)	(val##L)
#define N_NUINT64_CONSTANT(val)	(val##UL)



typedef signed long nssize;
typedef unsigned long nsize;


#define N_MAXSIZE	N_MAXULONG
#define N_MINSSIZE	N_MINLONG
#define N_MAXSSIZE	N_MAXLONG

typedef nint64 noffset;
#define N_MINOFFSET	N_MININT64
#define N_MAXOFFSET	N_MAXINT64




#define NPOINTER_TO_INT(p)	((nint)  (nlong) (p))
#define NPOINTER_TO_UINT(p)	((nuint) (nulong) (p))

#define NINT_TO_POINTER(i)	((npointer) (nlong) (i))
#define NUINT_TO_POINTER(u)	((npointer) (nulong) (u))

typedef signed long nintptr;
typedef unsigned long nuintptr;

#define N_ATOMIC_LOCK_FREE

typedef char   nchar;
typedef short  nshort;
typedef long   nlong;
typedef int    nint;
typedef nint   nboolean;

typedef unsigned char   nuchar;
typedef unsigned short  nushort;
typedef unsigned long   nulong;
typedef unsigned int    nuint;

typedef float   nfloat;
typedef double  ndouble;

#define N_MININT8	((nint8) (-N_MAXINT8 - 1))
#define N_MAXINT8	((nint8)  0x7f)
#define N_MAXUINT8	((nuint8) 0xff)


#define  N_MININT16	((nint16) (-N_MAXINT16 - 1))
#define  N_MAXINT16	((nint16)  0x7fff)
#define  N_MAXUINT16	((nuint16) 0xffff)



#define N_MININT32	((nint32) (-N_MAXINT32 - 1))
#define N_MAXINT32	((nint32)  0x7fffffff)
#define N_MAXUINT32	((nuint32) 0xffffffff)

/**
* N_MININT64: (value -9223372036854775808)
*
* The minimum value which can be held in a #nint64.
*/
#define N_MININT64	((nint64) (-N_MAXINT64 - N_NINT64_CONSTANT(1)))
#define N_MAXINT64	N_NINT64_CONSTANT(0x7fffffffffffffff)
#define N_MAXUINT64	N_NUINT64_CONSTANT(0xffffffffffffffff)

typedef void* npointer;
typedef const void *nconstpointer;

typedef nint            (*NCompareFunc)         (nconstpointer  a,nconstpointer  b);
typedef nint            (*NCompareDataFunc)     (nconstpointer  a,nconstpointer  b,npointer       user_data);
typedef nboolean        (*NEqualFunc)           (nconstpointer  a,nconstpointer  b);
typedef void            (*NDestroyNotify)       (npointer data);
typedef void            (*NFunc)                (npointer data,npointer user_data);
typedef nuint           (*NHashFunc)            (nconstpointer  key);
typedef void            (*NHFunc)               (npointer       key,npointer value,npointer user_data);
typedef npointer	    (*NCopyFunc)            (nconstpointer  src,npointer       data);
typedef nint            nrefcount;
typedef volatile nint   natomicrefcount;

#define N_STMT_START  do
#define N_STMT_END    while (0)

#define N_LIKELY(expr) (expr)
#define N_UNLIKELY(expr) (expr)

#define N_STRUCT_OFFSET(struct_type, member) \
      ((nlong) offsetof (struct_type, member))


#define N_STRUCT_MEMBER_P(struct_p, struct_offset)   \
    ((npointer) ((nuint8*) (struct_p) + (nlong) (struct_offset)))
#define N_STRUCT_MEMBER(member_type, struct_p, struct_offset)   \
    (*(member_type*) N_STRUCT_MEMBER_P ((struct_p), (struct_offset)))

#define N_STRFUNC     ((const char*) (__FUNCTION__))
#define N_STRINGIFY(macro_or_string)	N_STRINGIFY_ARG (macro_or_string)
#define	N_STRINGIFY_ARG(contents)	#contents

#ifndef NULL
#  ifdef __cplusplus
#  define NULL        (0L)
#  else /* !__cplusplus */
#  define NULL        ((void*) 0)
#  endif /* !__cplusplus */
#endif

#define	FALSE	(0)
#define	TRUE	(!FALSE)

#define n_abort() abort ()

#define N_GNUC_EXTENSION __extension__
#define N_STATIC_ASSERT(expr) typedef char N_PASTE (_GStaticAssertCompileTimeAssertion_, __LINE__)[(expr) ? 1 : -1] N_GNUC_UNUSED
#define N_PASTE_ARGS(identifier1,identifier2) identifier1 ## identifier2
#define N_PASTE(identifier1,identifier2)      N_PASTE_ARGS (identifier1, identifier2)
#define N_STRLOC	__FILE__ ":" N_STRINGIFY (__LINE__)


#define NLIB_SIZEOF_VOID_P 8
#define NLIB_SIZEOF_LONG   8
#define NLIB_SIZEOF_SIZE_T 8
#define NLIB_SIZEOF_SSIZE_T 8


#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define N_GNUC_UNUSED                           \
  __attribute__((__unused__))


#define N_N_ELEMENTS(arr)		(sizeof (arr) / sizeof ((arr)[0]))

#define N_GNUC_FALLTHROUGH __attribute__((fallthrough))

#define N_DIR_SEPARATOR '/'
#define N_DIR_SEPARATOR_S "/"
#define N_SEARCHPATH_SEPARATOR ':'
#define N_SEARCHPATH_SEPARATOR_S ":"


#define N_NSIZE_MODIFIER "l"
#define N_NSSIZE_MODIFIER "l"
#define N_NSIZE_FORMAT "lu"
#define N_NSSIZE_FORMAT "li"

#define n_assert_not_reached()          N_STMT_START { (void) 0; } N_STMT_END

typedef nuint32 NQuark;

#endif

