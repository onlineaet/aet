#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include "APoll.h"

impl$ APoll{
     aboolean init(int size){
       int fd = epoll_create1(EPOLL_CLOEXEC);
       if (fd < 0 && (errno == EINVAL || errno == ENOSYS))
            fd = epoll_create (256);
       if (fd < 0)
          return FALSE;
       fcntl (fd, F_SETFD, FD_CLOEXEC);
       self->events = a_malloc0_n (size, sizeof (struct epoll_event));
       self->nevents = size;
       self->fd=fd;
       return TRUE;
    }

    APoll(int size){
	  lock=new$ AMutex();
      self->checktimeout=0;
      self->fdCount=0;
      init(size);
   }


    int wait( struct epoll_event *events,int count,int timeout){
          int eventcnt=epoll_wait (self->fd,events, count, timeout);
//          int i;
//          for (i = 0; i < eventcnt; i++) {
//              a_warning("epoll.c wait_od line:%d ",__LINE__);
//              struct epoll_event *ev = self->events + i;
//              int fd = ev->data.fd;
//              a_warning("epoll.c wait_od line:%d fd:%x",__LINE__,fd);
//              int what=ev->events;
//              int got=0;
//              aboolean error=FALSE;
//              if (what & (EPOLLHUP|EPOLLERR)) {
//                 got = EPOLLIN | EPOLLOUT;
//                 error=TRUE;
//                 a_warning("epoll -error - event is %p fd:%d got %d",fd,got);
//              } else {
//                 if (what & EPOLLIN){
//                    got=EPOLLIN;
//                 }
//                 if (what & EPOLLOUT){
//                    got=EPOLLOUT;
//                 }
//              }
//              a_warning("liuyue ##$$## epoll tt -- event is fd:%x got: %d",fd,got);
//           }
//           return TRUE;
             return eventcnt;
    }


    /**
     * 加入要监听的fd和监听的事件
     */
    int  add(int fd,int events){
           struct epoll_event ev;
           memset(&ev, 0, sizeof(ev));
           ev.data.fd=fd;
           ev.data.u64= (uint64_t)(uint32_t)fd;
           ev.events   = (events & EPOLLIN  ? EPOLLIN  : 0)
                           | (events & EPOLLOUT ? EPOLLOUT : 0);
            ev.events   |=EPOLLET;
            a_info("epoll add line:%d  ",__LINE__);
            int re= epoll_ctl (self->fd, EPOLL_CTL_ADD,fd, &ev);
            a_info("epoll add line:%d  re:%d",__LINE__,re);
            if(re==0){
                a_info("epoll add line:%d  ",__LINE__);
                goto out;
            }

            if (errno == ENOENT){
                a_info("epoll add line:%d  ",__LINE__);
                if (!events)
                  goto out;
                re= epoll_ctl (self->fd, EPOLL_CTL_ADD,fd, &ev);
            } else if (errno == EEXIST) {
                  a_info("epoll add line:%d  ",__LINE__);
                  epoll_ctl (self->fd, EPOLL_CTL_DEL, fd, NULL);
                  re=EEXIST;
                  goto out;
            } else if (errno == EPERM){
            }
    out:
            return re;
    }


    int modify (int fd, int events){
         int res=0;
         struct epoll_event ev;
         memset(&ev, 0, sizeof(ev));
         unsigned char oldmask=0;
         ev.data.fd=fd;
         ev.data.u64 = (uint64_t)(uint32_t)fd;
         ev.events   = (events & EPOLLIN  ? EPOLLIN  : 0)
                           | (events & EPOLLOUT ? EPOLLOUT : 0);
         ev.events   |=EPOLLET;
         int re= epoll_ctl (self->fd, EPOLL_CTL_MOD,fd, &ev);
         if(re==0)
             return 0;
         int err=errno;
         a_warning("change_cb  fd:%d read:%d write:%d events:%d re:%d err:%d",
                        fd,EPOLLIN,EPOLLOUT,events,re,err);
        if (err == ENOENT){
                  if (!epoll_ctl (self->fd, EPOLL_CTL_ADD, fd, &ev))
                      return -1;
        } else if (err == EEXIST){
                  if (oldmask == events)
                      return 0;
                  if (!epoll_ctl (self->fd, EPOLL_CTL_MOD, fd, &ev))
                       return -1;
        } else if (err == EPERM)  {
                  return 0;
        }else if(err==EBADF){
                    a_warning("change_cb 00 fd:%d read:%d write:%d events:%d re:%d err:%d",
                            fd,EPOLLIN,EPOLLOUT,events,re,err);
                    return -EBADF;
         }
         return 0;
    }


    /**
     * 删除epoll中监视的fd
     */
    int   delete(int fd){
         int re=epoll_ctl (self->fd, EPOLL_CTL_DEL,fd, NULL);
         if (errno == ENOENT || errno == EBADF || errno == EPERM) {
            a_warning("Epoll on fd %d gave %s: DEL was unnecessary.",fd,strerror(errno));
            re=0;
         }
         return re;
    }



    ~APoll (){
        lock.lock();
        self->fdCount=0;
        close (fd);
        lock.unlock();
    }

};


