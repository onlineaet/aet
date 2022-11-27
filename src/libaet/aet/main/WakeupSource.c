#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include  <sys/eventfd.h>
#include <sys/epoll.h>

#include "WakeupSource.h"

impl$ WakeupSource{

    WakeupSource(char *name){
        super$(name);
        self->fd= eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);
        self->havePoll=TRUE;
    }

    void signal(){
          int res;
          auint64 one = 1;
          //printf("g_wakeup_acknowledge --write %p\n",self);

          do
            res = write (fd, &one, sizeof one);
          while (A_UNLIKELY (res == -1 && errno == EINTR));
    }

    void  acknowledge (){
       char buffer[16];
       //printf("g_wakeup_acknowledge --read %p\n",self);

       /* read until it is empty */
       while (read (fd, buffer, sizeof buffer) == sizeof buffer);
    }

    int getFd(){
        return fd;
    }

    int getEvents(){
        //int rre=EPOLLIN|EPOLLOUT|EPOLLERR;
       // printf("reesss %d\n",rre);
        return EPOLLIN;

       // return EPOLLIN|EPOLLOUT|EPOLLERR;
    }

    aboolean  prepare(int *timeout){
        *timeout = -1;
        return FALSE;
    }

    aboolean check(){
        return TRUE;
    }

    aboolean  dispatch(){
         return TRUE;
    }

    ~WakeupSource(){
       close(fd);
    }

};


