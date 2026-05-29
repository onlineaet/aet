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


