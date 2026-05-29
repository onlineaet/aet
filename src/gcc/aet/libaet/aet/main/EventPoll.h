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

#ifndef __AET_MAIN_EVENT_POLL_H__
#define __AET_MAIN_EVENT_POLL_H__

#include "../../aet.h"
#include "EventSource.h"
#include "../io/APoll.h"

package$ aet.main;



public$ class$ EventPoll{
    public$ static int MAX_EVENTS=1024;
    APoll *epoll;
    struct epoll_event events[1024];
    int eventCount;
    int add(EventSource *source);
    int delete(EventSource *source);
    int wait(int timeout);
    int process(int eventcnt);
    int getEvent(int destFd,int *err);
    int modify (int fd, int events);


};



#endif /* __N_MEM_H__ */

