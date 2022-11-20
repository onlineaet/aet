/*
 * basetype.c
 *
 *  Created on: 2020年11月12日
 *  Author: 张春雷
 */

#ifndef __A_BASE_H__
#define __A_BASE_H__

#define xmalloc(s) malloc(s)
#define xrealloc(s,a) realloc(s,a)
#define xstrerror(s) strerror(s)
#define xcalloc(s,a) calloc(s,a)
#define ISDIGIT(s) isdigit(s)
#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <stddef.h>


#define A_MINFLOAT	FLT_MIN
#define A_MAXFLOAT	FLT_MAX
#define A_MINDOUBLE	DBL_MIN
#define A_MAXDOUBLE	DBL_MAX
#define A_MINSHORT	SHRT_MIN
#define A_MAXSHORT	SHRT_MAX
#define A_MAXUSHORT	USHRT_MAX
#define A_MININT	INT_MIN
#define A_MAXINT	INT_MAX
#define A_MAXUINT	UINT_MAX
#define A_MINLONG	LONG_MIN
#define A_MAXLONG	LONG_MAX
#define A_MAXULONG	ULONG_MAX

typedef signed char aint8;
typedef unsigned char auint8;
typedef signed short aint16;
typedef unsigned short auint16;
typedef signed int aint32;
typedef unsigned int auint32;


typedef signed long long aint64;
typedef unsigned long long auint64;

#define A_NINT64_CONSTANT(val)	(val##L)
#define A_NUINT64_CONSTANT(val)	(val##UL)



typedef signed long assize;
typedef unsigned long asize;


#define A_MAXSIZE	A_MAXULONG
#define A_MINSSIZE	A_MINLONG
#define A_MAXSSIZE	A_MAXLONG

typedef aint64 noffset;
#define A_MINOFFSET	A_MININT64
#define A_MAXOFFSET	A_MAXINT64




#define APOINTER_TO_INT(p)	((aint)  (along) (p))
#define APOINTER_TO_UINT(p)	((auint) (aulong) (p))

#define AINT_TO_POINTER(i)	((apointer) (along) (i))
#define AUINT_TO_POINTER(u)	((apointer) (aulong) (u))

typedef signed long aintptr;
typedef unsigned long auintptr;

#define A_ATOMIC_LOCK_FREE

typedef char   achar;
typedef short  ashort;
typedef long   along;
typedef int    aint;
typedef aint   aboolean;

typedef unsigned char   auchar;
typedef unsigned short  aushort;
typedef unsigned long   aulong;
typedef unsigned int    auint;

typedef float   afloat;
typedef double  adouble;

#define A_MININT8	((aint8) (-A_MAXINT8 - 1))
#define A_MAXINT8	((aint8)  0x7f)
#define A_MAXUINT8	((auint8) 0xff)


#define  A_MININT16	((aint16) (-A_MAXINT16 - 1))
#define  A_MAXINT16	((aint16)  0x7fff)
#define  A_MAXUINT16	((auint16) 0xffff)



#define A_MININT32	((aint32) (-A_MAXINT32 - 1))
#define A_MAXINT32	((aint32)  0x7fffffff)
#define A_MAXUINT32	((auint32) 0xffffffff)

/**
* A_MININT64: (value -9223372036854775808)
*
* The minimum value which can be held in a #aint64.
*/
#define A_MININT64	((aint64) (-A_MAXINT64 - A_NINT64_CONSTANT(1)))
#define A_MAXINT64	A_NINT64_CONSTANT(0x7fffffffffffffff)
#define A_MAXUINT64	A_NUINT64_CONSTANT(0xffffffffffffffff)

typedef void* apointer;
typedef const void *aconstpointer;

typedef aint            (*ACompareFunc)         (aconstpointer  a,aconstpointer  b);
typedef aint            (*ACompareDataFunc)     (aconstpointer  a,aconstpointer  b,apointer       user_data);
typedef aboolean        (*AEqualFunc)           (aconstpointer  a,aconstpointer  b);
typedef void            (*ADestroyNotify)       (apointer data);
typedef void            (*AFunc)                (apointer data,apointer user_data);
typedef auint           (*AHashFunc)            (aconstpointer  key);
typedef void            (*AHFunc)               (apointer key,apointer value,apointer user_data);
typedef apointer	    (*ACopyFunc)            (aconstpointer  src,apointer       data);

#define A_STMT_START  do
#define A_STMT_END    while (0)

#define A_LIKELY(expr) (expr)
#define A_UNLIKELY(expr) (expr)

#define A_STRUCT_OFFSET(struct_type, member) \
      ((along) offsetof (struct_type, member))

#ifndef NULL
#  ifdef __cplusplus
#  define NULL        (0L)
#  else /* !__cplusplus */
#  define NULL        ((void*) 0)
#  endif /* !__cplusplus */
#endif

//#define	FALSE	(0)
//#define	TRUE	(!FALSE)

#undef TRUE
#undef FALSE
#ifndef FALSE
#define FALSE   (0)         //GCC 已有
#endif
#ifndef TRUE
#define TRUE    (!FALSE)    //GCC 已有
#endif


#define a_abort() abort ()

#define ALIB_SIZEOF_VOID_P 8
#define ALIB_SIZEOF_LONG   8
#define ALIB_SIZEOF_SIZE_T 8
#define ALIB_SIZEOF_SSIZE_T 8


#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define A_APPROX_VALUE(a, b, epsilon) \
  (((a) > (b) ? (a) - (b) : (b) - (a)) < (epsilon))
#define A_GNUC_UNUSED  __attribute__((__unused__))

#define A_GNUC_PURE __attribute__((__pure__))
#define A_GNUC_MALLOC __attribute__((__malloc__))
#define A_GNUC_NO_INLINE __attribute__((noinline))
#define A_GNUC_ALLOC_SIZE(x) __attribute__((__alloc_size__(x)))
#define A_GNUC_ALLOC_SIZE2(x,y) __attribute__((__alloc_size__(x,y)))
#define A_GNUC_EXTENSION __extension__

#define A_N_ELEMENTS(arr)		(sizeof (arr) / sizeof ((arr)[0]))

#define A_GNUC_FALLTHROUGH __attribute__((fallthrough))

#define A_DIR_SEPARATOR '/'
#define A_DIR_SEPARATOR_S "/"
#define A_SEARCHPATH_SEPARATOR ':'
#define A_SEARCHPATH_SEPARATOR_S ":"


#define A_GINT64_CONSTANT(val)	(val##L)
#define A_GUINT64_CONSTANT(val)	(val##UL)

#define A_AINT64_MODIFIER "l"
#define A_AINT64_FORMAT "li"
#define A_AUINT64_FORMAT "lu"

#define A_ASIZE_MODIFIER "l"
#define A_ASSIZE_MODIFIER "l"
#define A_ASIZE_FORMAT "lu"
#define A_ASSIZE_FORMAT "li"


#define A_GNUC_PRINTF( format_idx, arg_idx )    \
  __attribute__((__format__ (gnu_printf, format_idx, arg_idx)))

#define A_PASTE_ARGS(identifier1,identifier2) identifier1 ## identifier2
#define A_PASTE(identifier1,identifier2)      A_PASTE_ARGS (identifier1, identifier2)
#define A_STATIC_ASSERT(expr) typedef char A_PASTE (_AStaticAssertCompileTimeAssertion_, __COUNTER__)[(expr) ? 1 : -1] A_GNUC_UNUSED

#define alib_typeof(t) __typeof__ (t)

#define A_STRFUNC     ((const char*) (__FUNCTION__))
#define A_STRINGIFY(macro_or_string)	A_STRINGIFY_ARG (macro_or_string)
#define	A_STRINGIFY_ARG(contents)	#contents
#define A_STRLOC	__FILE__ ":" A_STRINGIFY (__LINE__)

#define A_GNUC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

/**
 * 泛型声明只能是 A-Z,
 * 在编译时转成aet_generic_A、aet_generic_B等类型
 */
typedef void* aet_generic_A;
typedef void* aet_generic_B;
typedef void* aet_generic_C;
typedef void* aet_generic_D;
typedef void* aet_generic_E;
typedef void* aet_generic_F;
typedef void* aet_generic_G;
typedef void* aet_generic_H;
typedef void* aet_generic_I;
typedef void* aet_generic_J;
typedef void* aet_generic_K;
typedef void* aet_generic_L;
typedef void* aet_generic_M;
typedef void* aet_generic_N;
typedef void* aet_generic_O;
typedef void* aet_generic_P;
typedef void* aet_generic_Q;
typedef void* aet_generic_R;
typedef void* aet_generic_S;
typedef void* aet_generic_T;
typedef void* aet_generic_U;
typedef void* aet_generic_V;
typedef void* aet_generic_W;
typedef void* aet_generic_X;
typedef void* aet_generic_Y;
typedef void* aet_generic_Z;

#endif

