#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#include "AMutex.h"


impl$ AMutex{


    AMutex(){
        self->i[0] = 0;
    }

   void clear (){
      if A_UNLIKELY (self->i[0] != 0){
         fprintf (stderr, "AMutex释放时出错，由于没有初始化或被锁住了。\n");
         a_abort ();
      }
   }

   void lockSlowpath(){
     /* Set to 2 to indicate contention.  If it was zero before then we
      * just acquired the lock.
      *
      * Otherwise, sleep for as long as the 2 remains...
      */
     while (exchange_acquire (&self->i[0], 2) != 0)
       syscall (__NR_futex, &self->i[0], (asize) FUTEX_WAIT_PRIVATE, (asize) 2, NULL);
   }

    void  unlockSlowpath (auint prev){
     /* We seem to get better code for the uncontended case by splitting
      * this out...
      */
      if A_UNLIKELY (prev == 0){
         fprintf (stderr, "Attempt to unlock mutex that was not locked\n");
         a_abort ();
      }
      syscall (__NR_futex, &self->i[0], (asize) FUTEX_WAKE_PRIVATE, (asize) 1, NULL);
   }

   void lock(){
     /* 0 -> 1 and we're done.  Anything else, and we need to wait... */
     if A_UNLIKELY (a_atomic_int_add (&self->i[0], 1) != 0)
        lockSlowpath ();
   }

   void unlock (){
     auint prev;
     prev = exchange_release (&self->i[0], 0);
     /* 1-> 0 and we're done.  Anything else and we need to signal... */
     if A_UNLIKELY (prev != 1)
        unlockSlowpath (prev);
   }

   aboolean trylock(){
     auint zero = 0;

     /* We don't want to touch the value at all unless we can move it from
      * exactly 0 to 1.
      */
     return compare_exchange_acquire (&self->i[0], &zero, 1);
   }

   ~AMutex(){
       //printf("freee amutx-- %p\n",self);
       clear();
    }


};
