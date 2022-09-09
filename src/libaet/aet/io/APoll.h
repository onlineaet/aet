#ifndef __AET_IO_A_POLL_H__
#define __AET_IO_A_POLL_H__

#include <sys/epoll.h>
#include <objectlib.h>
#include "../util/AMutex.h"

package$ aet.io;


public$ class$ APoll{
    int  fd;
    struct epoll_event *events;
    int nevents;
    int fdCount;
    AMutex lock;
    auint64 checktimeout;
    public$ APoll(int size);
    public$ int      wait(struct epoll_event *events,int count,int timeout);
    public$ int      add(int fd,int events);
    public$ int      modify (int fd, int events);
    public$ int      delete(int fd);

};



#endif /* __N_FILE_H__*/




