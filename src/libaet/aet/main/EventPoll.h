

#ifndef __AET_MAIN_EVENT_POLL_H__
#define __AET_MAIN_EVENT_POLL_H__

#include <objectlib.h>
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

