
#ifndef __A_ATOMIC_H__
#define __A_ATOMIC_H__



#include "abase.h"



aint                    a_atomic_int_get(const volatile aint *atomic);
void                    a_atomic_int_set(volatile aint  *atomic,aint newval);
void                    a_atomic_int_inc(volatile aint  *atomic);
aboolean                a_atomic_int_dec_and_test(volatile aint  *atomic);
aboolean                a_atomic_int_compare_and_exchange(volatile aint  *atomic,aint oldval,aint newval);
aint                    a_atomic_int_add(volatile aint  *atomic,aint val);
auint                   a_atomic_int_and(volatile auint *atomic, auint val);
auint                   a_atomic_int_or(volatile auint *atomic,auint val);
auint                   a_atomic_int_xor(volatile auint *atomic,auint val);
apointer                a_atomic_pointer_get(const volatile void *atomic);
void                    a_atomic_pointer_set(volatile void  *atomic,apointer newval);
aboolean                a_atomic_pointer_compare_and_exchange(volatile void  *atomic,apointer oldval,apointer newval);
assize                  a_atomic_pointer_add(volatile void  *atomic, assize val);
asize                   a_atomic_pointer_and(volatile void  *atomic,  asize val);
asize                   a_atomic_pointer_or(volatile void  *atomic,asize val);
asize                   a_atomic_pointer_xor(volatile void  *atomic, asize val);

#define a_atomic_int_get(atomic) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    aint gaig_temp;                                                          \
    (void) (0 ? *(atomic) ^ *(atomic) : 1);                                  \
    __atomic_load ((aint *)(atomic), &gaig_temp, __ATOMIC_SEQ_CST);          \
    (aint) gaig_temp;                                                        \
  }))

#define a_atomic_int_set(atomic, newval) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    aint gais_temp = (aint) (newval);                                        \
    (void) (0 ? *(atomic) ^ (newval) : 1);                                   \
    __atomic_store ((aint *)(atomic), &gais_temp, __ATOMIC_SEQ_CST);         \
  }))


#define a_atomic_pointer_get(atomic)                                       \
  (A_GNUC_EXTENSION ({                                                     \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));               \
    alib_typeof (*(atomic)) gapg_temp_newval;                              \
    alib_typeof ((atomic)) gapg_temp_atomic = (atomic);                    \
    __atomic_load (gapg_temp_atomic, &gapg_temp_newval, __ATOMIC_SEQ_CST); \
    gapg_temp_newval;                                                      \
  }))
#define a_atomic_pointer_set(atomic, newval)                                \
  (A_GNUC_EXTENSION ({                                                      \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));                \
    alib_typeof ((atomic)) gaps_temp_atomic = (atomic);                     \
    alib_typeof (*(atomic)) gaps_temp_newval = (newval);                    \
    (void) (0 ? (apointer) * (atomic) : NULL);                              \
    __atomic_store (gaps_temp_atomic, &gaps_temp_newval, __ATOMIC_SEQ_CST); \
  }))


#define a_atomic_int_inc(atomic) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ *(atomic) : 1);                                  \
    (void) __atomic_fetch_add ((atomic), 1, __ATOMIC_SEQ_CST);               \
  }))
#define a_atomic_int_dec_and_test(atomic) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ *(atomic) : 1);                                  \
    __atomic_fetch_sub ((atomic), 1, __ATOMIC_SEQ_CST) == 1;                 \
  }))
#define a_atomic_int_compare_and_exchange(atomic, oldval, newval) \
  (A_GNUC_EXTENSION ({                                                       \
    aint gaicae_oldval = (oldval);                                           \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ (newval) ^ (oldval) : 1);                        \
    __atomic_compare_exchange_n ((atomic), &gaicae_oldval, (newval), FALSE, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? TRUE : FALSE; \
  }))
#define a_atomic_int_add(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ (val) : 1);                                      \
    (aint) __atomic_fetch_add ((atomic), (val), __ATOMIC_SEQ_CST);           \
  }))
#define a_atomic_int_and(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ (val) : 1);                                      \
    (auint) __atomic_fetch_and ((atomic), (val), __ATOMIC_SEQ_CST);          \
  }))
#define a_atomic_int_or(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ (val) : 1);                                      \
    (auint) __atomic_fetch_or ((atomic), (val), __ATOMIC_SEQ_CST);           \
  }))
#define a_atomic_int_xor(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (aint));                     \
    (void) (0 ? *(atomic) ^ (val) : 1);                                      \
    (auint) __atomic_fetch_xor ((atomic), (val), __ATOMIC_SEQ_CST);          \
  }))

#define a_atomic_pointer_compare_and_exchange(atomic, oldval, newval) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof (oldval) == sizeof (apointer));                  \
    alib_typeof ((oldval)) gapcae_oldval = (oldval);                         \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));                 \
    (void) (0 ? (apointer) *(atomic) : NULL);                                \
    __atomic_compare_exchange_n ((atomic), &gapcae_oldval, (newval), FALSE, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? TRUE : FALSE; \
  }))

#define a_atomic_pointer_add(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));                 \
    (void) (0 ? (apointer) *(atomic) : NULL);                                \
    (void) (0 ? (val) ^ (val) : 1);                                          \
    (assize) __atomic_fetch_add ((atomic), (val), __ATOMIC_SEQ_CST);         \
  }))
#define a_atomic_pointer_and(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    volatile asize *gapa_atomic = (volatile asize *) (atomic);               \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));                 \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (asize));                    \
    (void) (0 ? (apointer) *(atomic) : NULL);                                \
    (void) (0 ? (val) ^ (val) : 1);                                          \
    (asize) __atomic_fetch_and (gapa_atomic, (val), __ATOMIC_SEQ_CST);       \
  }))
#define a_atomic_pointer_or(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    volatile asize *gapo_atomic = (volatile asize *) (atomic);               \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));                 \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (asize));                    \
    (void) (0 ? (apointer) *(atomic) : NULL);                                \
    (void) (0 ? (val) ^ (val) : 1);                                          \
    (asize) __atomic_fetch_or (gapo_atomic, (val), __ATOMIC_SEQ_CST);        \
  }))
#define a_atomic_pointer_xor(atomic, val) \
  (A_GNUC_EXTENSION ({                                                       \
    volatile asize *gapx_atomic = (volatile asize *) (atomic);               \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (apointer));                 \
    A_STATIC_ASSERT (sizeof *(atomic) == sizeof (asize));                    \
    (void) (0 ? (apointer) *(atomic) : NULL);                                \
    (void) (0 ? (val) ^ (val) : 1);                                          \
    (asize) __atomic_fetch_xor (gapx_atomic, (val), __ATOMIC_SEQ_CST);       \
  }))

#include <stdatomic.h>

#define exchange_acquire(ptr, new) \
  atomic_exchange_explicit((atomic_uint *) (ptr), (new), __ATOMIC_ACQUIRE)
#define compare_exchange_acquire(ptr, old, new) \
  atomic_compare_exchange_strong_explicit((atomic_uint *) (ptr), (old), (new), \
                                          __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)

#define exchange_release(ptr, new) \
  atomic_exchange_explicit((atomic_uint *) (ptr), (new), __ATOMIC_RELEASE)
#define store_release(ptr, new) \
  atomic_store_explicit((atomic_uint *) (ptr), (new), __ATOMIC_RELEASE)


#endif /* __A_ATOMIC_H__ */





