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
#include "ACond.h"


impl$ ACond{

    ACond(){
        self->i[0] = 0;
    }

    void wait (AMutex *mutex){
      auint sampled = (auint) a_atomic_int_get (&self->i[0]);
      mutex->unlock();
      syscall (__NR_futex, &self->i[0], (asize) FUTEX_WAIT_PRIVATE, (asize) sampled, NULL);
      mutex->lock();
    }


    void signal(){
        a_atomic_int_inc (&self->i[0]);
        syscall (__NR_futex, &self->i[0], (asize) FUTEX_WAKE_PRIVATE, (asize) 1, NULL);
    }

    void  broadcast (){
        a_atomic_int_inc (&self->i[0]);
        syscall (__NR_futex, &self->i[0], (asize) FUTEX_WAKE_PRIVATE, (asize) INT_MAX, NULL);
    }


    aboolean wait(AMutex *mutex,aint64  endTime){
        struct timespec now;
        struct timespec span;
        auint sampled;
        int res;
        aboolean success;

        if (endTime < 0)
          return FALSE;

        clock_gettime (CLOCK_MONOTONIC, &now);
        span.tv_sec = (endTime / 1000000) - now.tv_sec;
        span.tv_nsec = ((endTime % 1000000) * 1000) - now.tv_nsec;
        if (span.tv_nsec < 0){
            span.tv_nsec += 1000000000;
            span.tv_sec--;
        }

        if (span.tv_sec < 0)
          return FALSE;
        sampled = self->i[0];
        mutex->unlock();
        res = syscall (__NR_futex, &self->i[0], (asize) FUTEX_WAIT_PRIVATE, (asize) sampled, &span);
        success = (res < 0 && errno == ETIMEDOUT) ? FALSE : TRUE;
        mutex->lock();
        return success;
    }

    ~ACond(){
     }
};
