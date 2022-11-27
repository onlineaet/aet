#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "EventPoll.h"

impl$ EventPoll{

    EventPoll(){
      epoll=new$ APoll(64);
      eventCount=0;
    }

    /**
      * 与wait可能不在同一线程中被调用
      */
    int add(EventSource *source){
        if(!source->havePoll())
            return -1;
        int fd=source->getFd();
        int events=source->getEvents();
        if(fd<0 || events<0){
            a_error("加入的事件源没有文件描述符或epoll事件。fd:%d events:%d name:%s",fd,events,source->name);
            return -1;
        }
        //printf("add event fd:%d name:%s id:%d events:%d\n",fd,source->name,source->id,events);
        int re=  epoll->add(fd,events);
        //printf("add event fd:%d name:%s id:%d re:%d\n",fd,source->name,source->id,re);
        return re;

    }

    /**
     * 与wait可能不在同一线程中被调用
     */
    int delete(EventSource *source){
        if(!source->havePoll())
            return -1;
        int fd=source->getFd();
        int events=source->getEvents();
        if(fd<0 ){
            a_error("加入的事件源没有文件描述符。fd:%d events:%d name:%s",fd,events,source->name);
            return -1;
        }
        return  epoll->delete(fd);
    }

    /**
     * 在线程中
     */
    int wait(int timeout){
       // printf("start wait timeout:%d\n",timeout);
        eventCount=0;
        eventCount= epoll->wait(events,1024,timeout);
        //printf("start wait xxx timeout:%d eventCount:%d\n",timeout,eventCount);

        return eventCount;
    }

    int getEvent(int destFd,int *err){
        int i;
        //printf("getEvent ---- destFd:%d eventCount:%d\n",destFd,eventCount);
        for (i = 0; i < eventCount; i++) {
            a_warning("epoll.c wait_od line:%d ",__LINE__);
            struct epoll_event *ev =&self->events[i];
            int fd = ev->data.fd;
            a_warning("epoll.c wait_od line:%d fd:%x",__LINE__,fd);
            int what=ev->events;
            int got=0;
            aboolean error=FALSE;
            if (what & (EPOLLHUP|EPOLLERR)) {
               got = EPOLLIN | EPOLLOUT;
               error=TRUE;
               a_warning("epoll -error - event is %p fd:%d got %d",fd,got);
            } else {
               if (what & EPOLLIN){
                  got=EPOLLIN;
               }
               if (what & EPOLLOUT){
                  got=EPOLLOUT;
               }
            }
            if(fd==destFd){
                *err=error?1:0;
                return got;
            }
            a_warning("epoll ##$$## epoll tt -- event is fd:%x got: %d",fd,got);
         }
         return 0;
    }



    int process(int eventcnt){
      int i;
      for (i = 0; i < eventcnt; i++) {
          a_warning("epoll.c wait_od line:%d ",__LINE__);
          struct epoll_event *ev =&self->events[i];
          int fd = ev->data.fd;
          a_warning("epoll.c wait_od line:%d fd:%x",__LINE__,fd);
          int what=ev->events;
          int got=0;
          if (what & (EPOLLHUP|EPOLLERR)) {
             got = EPOLLIN | EPOLLOUT;
             a_warning("epoll -error - event is %p fd:%d got %d",fd,got);
          } else {
             if (what & EPOLLIN){
                got=EPOLLIN;
             }
             if (what & EPOLLOUT){
                got=EPOLLOUT;
             }
          }
          a_warning("liuyue ##$$## epoll tt -- event is fd:%x got: %d",fd,got);
       }
       return TRUE;
    }

    int modify (int fd, int events){
        return epoll->modify(fd,events);
    }


    ~EventPoll(){
        epoll->unref();
    }

};


