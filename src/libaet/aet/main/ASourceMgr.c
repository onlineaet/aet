#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include "../lang/AAssert.h"
#include "../lang/AString.h"

#include "../time/Time.h"

#include "ASourceMgr.h"
#include "ArrayStorage.h"
#include "IdleSource.h"
#include "TimeoutSource.h"


#define SYSDEF_POLLNVAL 32

/**
 * 消除资源的回调，用回调的方法，解耦了EventSource与ASourceMgr的相互引用。
 * 这样，EventSource可以直接调用自身的destroy方法。
 */

static inline aboolean SOURCE_DESTROYED(EventSource *source)
{
    return ((source->flags & HookFlagMask.ACTIVE) == 0);
}

static inline aboolean SOURCE_BLOCKED(EventSource *source)
{
    return  ((source->flags & Flags.BLOCKED) != 0);
}


impl$ SourceDispatch{
    SourceDispatch(){
        source=NULL;
        depth=0;
    }
};

static void sourceDispatchFree_cb(apointer dispatch)
{
    ((SourceDispatch *)dispatch)->unref();
}


impl$ ASourceMgr{

    static ASourceMgr *getDefault(){
       static ASourceMgr *defaultSourceMgr = NULL;
       if (a_once_init_enter (&defaultSourceMgr)){
          ASourceMgr *mgr=NULL;
          mgr = new$ ASourceMgr();
          a_once_init_leave (&defaultSourceMgr, mgr);
       }
       return defaultSourceMgr;
    }

    static auint addIdle(EventSourceCallback callback,apointer userData){
        return addIdle(Priority.DEFAULT_IDLE,callback,userData,NULL);
    }

    static auint  addIdle(int priority,EventSourceCallback callback,apointer userData,ADestroyNotify notify){
      a_return_val_if_fail (callback != NULL, 0);
      EventSource *source = new$ IdleSource();
      if (priority != Priority.DEFAULT_IDLE)
          source->setPriority(priority);
      source->setCallback(callback, userData, notify);
      auint id = getDefault()->add(source);
      source->unref();
      return id;
    }

    static auint  addTimeout(auint interval,EventSourceCallback callback,apointer userData){
        return addTimeout(Priority.DEFAULT,interval,callback,userData,NULL);
    }

    static auint  addTimeout(int priority,auint interval,EventSourceCallback callback,apointer userData,ADestroyNotify notify){
          EventSource *source;
          auint id;
          a_return_val_if_fail (callback != NULL, 0);
          source = new$ TimeoutSource(interval);
          if (priority != Priority.DEFAULT)
            source->setPriority(priority);
          source->setCallback(callback, userData, notify);
          id = getDefault()->add(source);
          source->unref();
          return id;
    }

    /**
     * 每个线程一个
     */
    static SourceDispatch *getDispatch(){
      static AThreadSpecific threadPrivate={ NULL, (sourceDispatchFree_cb)};
      SourceDispatch *dispatch=NULL;
      dispatch = a_thread_specific_get (&threadPrivate);

      if (!dispatch){
          dispatch = new$ SourceDispatch();
          a_thread_specific_set (&threadPrivate, dispatch);
          dispatch=a_steal_pointer (&dispatch);
      }
      return dispatch;
    }

    static EventSource *getCurrentSource(){
        SourceDispatch *dispatch = getDispatch ();
        return dispatch->source;
    }


    ASourceMgr (){
      static asize initialised;
      if (a_once_init_enter (&initialised)){
          a_once_init_leave (&initialised, TRUE);
      }
      mutex=new$ AMutex();
      cond =new$ ACond();
      owner = NULL;//线程
      waiters = new$ ASList();
      storage=new$ ArrayStorage();
      poll=new$ EventPoll();
      wakeupSource=new$ WakeupSource("WakeupSource");
      storage->add(wakeupSource);
      poll->add(wakeupSource);

    }

    aboolean acquire(){
      aboolean result = FALSE;
      AThread *selfThread = AThread.current();
      mutex.lock();
      if (!owner){
          self->owner = selfThread;
          a_assert (ownerCount == 0);
      }
      if (owner == selfThread){
          ownerCount++;
          result = TRUE;
      }
      mutex.unlock();
      return result;
    }

    aboolean wait(){
      aboolean result = FALSE;
      AThread *selfThread =AThread.current();
      if (owner && owner != selfThread){
          waiters->add(self);
          cond.wait(&mutex);
          waiters->remove(self);
      }

      if (!owner){
          owner = selfThread;
          a_assert (ownerCount == 0);
      }

      if (owner == selfThread){
          ownerCount++;
          result = TRUE;
      }
      return result;
    }

    void lock(){
        mutex.lock();
    }

    void unlock(){
        mutex.unlock();
    }

    void  release(){
      mutex.lock();
      ownerCount--;
      if (ownerCount == 0){
          owner = NULL;
          ASourceMgr *waiter=waiters->getLast();
          if(waiter){
              aboolean internalWaiter= (waiter == self);
              if(!internalWaiter)
                  waiter->lock();
              waiter->cond.signal();
              if (!internalWaiter)
                  waiter->unlock();
          }
       }
       mutex.unlock();
    }

    aboolean getTimeout(int *priority,int *needTimeout,aint64 *needStartTime,int *timeFresh){
           int lastTimeout = *needTimeout;
           int i;
           int nReady = 0;
           int current_priority = A_MAXINT;
           EventSource *source=NULL;
           aboolean time_is_fresh=FALSE;
           aint64 startTime=*needStartTime;
          // printf("getTimeout 00-----\n");

           storage->initIter();
           while( storage->nextIter(&source)){
               //printf("getTimeout 11-----source: %p name:%s id:%d\n",source,source->name,source->id);

                int source_timeout = -1;
                if ((nReady > 0) && (source->priority > current_priority))
                    break;
                if (!(source->flags & Flags.READY)){
                   // printf("getTimeout 22-----source: %p name:%s id:%d\n",source,source->name,source->id);

                    aboolean result;
                    result = ((EventOps *)source)->prepare(&source_timeout);
                    //printf("getTimeout 22wwww-----source: %p name:%s id:%d result:%d %lli\n",source,source->name,source->id,result,source->ready_time);

                    if (result == FALSE && source->ready_time != -1){
                       // printf("getTimeout 33-----source: %p name:%s id:%d\n",source,source->name,source->id);

                        if (!time_is_fresh){
                            startTime =Time.monotonic();
                            time_is_fresh = TRUE;
                        }
                        if (source->ready_time <= startTime){
                            source_timeout = 0;
                            result = TRUE;
                        }else{
                            aint64 timeout;
                            /* rounding down will lead to spinning, so always round up */
                            timeout = (source->ready_time - startTime + 999) / 1000;
                            if (source_timeout < 0 || timeout < source_timeout)
                              source_timeout = MIN (timeout, A_MAXINT);
                        }
                        //printf("getTimeout 33-----source: %p name:%s id:%d result:%d\n",source,source->name,source->id,result);

                     }

                     if (result) {
                        EventSource *ready_source = source;
                        ready_source->flags |= Flags.READY;
                     }
                 }

                if (source->flags & Flags.READY) {
                    //printf("getTimeout 44-----source: %p name:%s id:%d\n",source,source->name,source->id);

                    nReady++;
                    current_priority = source->priority;
                    lastTimeout = 0;
                }

                if (source_timeout >= 0){
                   if (lastTimeout < 0)
                       lastTimeout = source_timeout;
                   else
                       lastTimeout = MIN (lastTimeout, source_timeout);
                }
            }//end for
            if (priority)
              *priority = current_priority;
            if (needTimeout)
              *needTimeout = lastTimeout;
            if(needStartTime)
              *needStartTime=startTime;
            if(timeFresh)
              *timeFresh=time_is_fresh;
            //printf("getTimeout over :%d %lli fresh:%d nReady:%d\n",lastTimeout,startTime,time_is_fresh,nReady);

            return (nReady > 0);
     }


     /**
      * prepare是为了获得超时间
      */
    aboolean prepare (int  *priority){
          auint i;
          int n_ready = 0;
          int current_priority = A_MAXINT;
          mutex.lock();
          self->eventTime.timeout=-1;
          self->eventTime.time_is_fresh=FALSE;
          if (in_check_or_prepare){
              a_warning ("已经在准备(prepare())或检查(check())状态了。");
              unlock();
              return FALSE;
          }
          //printf("prepare-----\n");
          aboolean ret=getTimeout(priority,&self->eventTime.timeout,&self->eventTime.time,&self->eventTime.time_is_fresh);
          mutex.unlock();
          return ret;
    }

    int query(int max_priority,int *timeout,struct epoll_event *epollEvents,int n_fds){
           EventSource *source=NULL;
           aushort events;
           int count=0;
           int i;
           storage->pollInitIter();
           while( storage->pollNextIter(&source)){
               int fd=source->getFd();
               int sourceEvents=source->getEvents();
               if (source->priority > max_priority)
                          continue;
               events = sourceEvents & ~(EPOLLERR|EPOLLHUP|SYSDEF_POLLNVAL);
               epollEvents[count].data.fd=fd;
               epollEvents[count].data.u64=(uint64_t)(uint32_t)fd;
               epollEvents[count].events=(events & EPOLLIN  ? EPOLLIN  : 0) | (events & EPOLLOUT ? EPOLLOUT : 0);
               epollEvents[count].events  |=EPOLLET;
               count++;
           }
           if (timeout){
             *timeout =self->eventTime.timeout;
             if (*timeout != 0)
                 self->eventTime.time_is_fresh = FALSE;
          }
          return count;
    }

    static int  poll (int fds,int  timeout){
      if (fds || timeout != 0){
          //printf("poll is ----- timeout:%d\n",timeout);
          int eventCount= poll->wait(timeout);
          //printf("poll is 111----- timeout:%d\n",timeout);
          int ret, errsv;
          errsv = errno;
          if (ret < 0 && errsv != EINTR) {
             a_warning ("poll(2) failed due to: %s.",AString.errToStr(errsv));
          }
          return eventCount;
       }
       return 0;
    }

     /**
      * 检查epoll wait后返回的事件中有没有fd指定的读事件
      */
    aboolean haveReadEvent(int fd){
        int err=0;
        int event=poll->getEvent(fd,&err);
        //printf("haveReadEvent---- %d %d\n",event,event|EPOLLIN);
        if(err){
            a_warning("有一个source发生错误了:fd:%d\n",fd);
        }
        return event|EPOLLIN;

    }

    aboolean foreachCheck(int max_priority){
              EventSource *source=NULL;
              storage->initIter();
              int nReady=0;
              while( storage->nextIter(&source)){
                  if (SOURCE_DESTROYED (source) || SOURCE_BLOCKED (source))
                     continue;
                  if ((nReady > 0) && (source->priority > max_priority))
                     break;
                  if(source==self->wakeupSource){
                      continue;
                  }
                  if (!(source->flags & Flags.READY)){
                       aboolean result;
                       //printf("foreachCheck 11 :%d\n",nReady);
                       result = ((EventOps *)source)->check();
                       if (result == FALSE ){
                          // printf("foreachCheck 22 :%d\n",nReady);
                           if(source->havePoll()){
                               int fd=source->getFd();
                               if( haveReadEvent(fd))
                                   result=TRUE;
                           }
                       }
                       if (result == FALSE && source->ready_time != -1){
                            if (!eventTime.time_is_fresh){
                                eventTime.time =Time.monotonic();
                                eventTime.time_is_fresh = TRUE;
                             }
                             if (source->ready_time <= eventTime.time)
                               result = TRUE;
                       }

                        if (result) {
                            //printf("foreachCheck 33 :%d\n",nReady);
                           EventSource *ready_source = source;
                           ready_source->flags |= Flags.READY;
                        }
                    }//end if

                   if (source->flags & Flags.READY) {
                       // printf("foreachCheck 44 :%d source:%p name:%s id:%d prio:%d\n",nReady,source,source->name,source->id,source->priority);
                        pendingDispatches[pendingDispatchesCount++]=source;
                        nReady++;
                        max_priority = source->priority;
                   }

               }//end while
              // printf("check over ---- %d %d\n",nReady,pendingDispatchesCount);
               return (nReady > 0);
        }


    aboolean check(int max_priority,struct epoll_event *epollEvents,int fdCount,int eventCount){
      int wakeupFd=wakeupSource->getFd();
      int i;
      //poll->process(eventCount);

      for (i = 0; i <fdCount; i++){
          struct epoll_event *ev=&epollEvents[i];
          if (ev->data.fd == wakeupFd){
             // printf("g_wakeup_acknowledge---00 eventCount:%d %p\n",eventCount,self->wakeupSource);
              int err=0;
              int events=poll->getEvent(wakeupFd,&err);
              if(events & EPOLLIN){
                // printf("g_wakeup_acknowledge---11 events:%d err:%d\n",events,err);
                 wakeupSource->acknowledge();
                 break;
              }
          }
       }
       aboolean ok=foreachCheck(max_priority);
       return ok;
    }

    void excute(){
      SourceDispatch  *current = getDispatch();
      auint i;
      for (i = 0; i < pendingDispatchesCount; i++){
          EventSource *source =pendingDispatches[i];
          pendingDispatches[i] = NULL;
          a_assert (source);
         // printf("excute pendingDispatches source:%p name:%s sourceId: %d\n",source,source->name,source->id);
          source->flags &= ~Flags.READY;
          if (!SOURCE_DESTROYED (source)){
              aboolean was_in_call;
              apointer user_data = NULL;
              aboolean need_destroy;
              was_in_call = source->flags & HookFlagMask.IN_CALL;
              source->flags |= HookFlagMask.IN_CALL;
              EventSource *prev_source;

              prev_source = current->source;
              current->source = source;
              current->depth++;
              need_destroy = !((EventOps *)source)->dispatch();
              current->source = prev_source;
              current->depth--;
              if (!was_in_call)
                source->flags &= ~HookFlagMask.IN_CALL;
              //printf("need_destroy --- %p %d %d\n",source,need_destroy,SOURCE_DESTROYED (source));
              if (need_destroy && !SOURCE_DESTROYED (source)){
                  source->destroy();
              }
          }
       }
       pendingDispatchesCount=0;
    }


    aboolean iterate (aboolean block,aboolean dispatch,AThread *current){
      int max_priority=0;
      int timeout=0;
      aboolean some_ready=FALSE;
      self->pendingDispatchesCount=0;
      int nfds, allocated_nfds;
     // printf("iterate 00cccc block:%d %lli\n",block,Time.monotonic()/1000);

      unlock();
     // printf("iterate 00bbbb block:%d %lli\n",block,Time.monotonic()/1000);

      if (!acquire ()){
          aboolean got_ownership;
          lock();
          if (!block)
            return FALSE;

          got_ownership = wait();
          if (!got_ownership)
             return FALSE;
      }else
          lock();

      unlock();
      //printf("iterate 00 block:%d %lli\n",block,Time.monotonic()/1000);
      prepare (&max_priority);
      //printf("iterate 11 max_priority:%d %lli\n",max_priority,Time.monotonic()/1000);

      struct epoll_event epollEvents[1024];
      int fdCount=query(max_priority,&timeout,epollEvents,1024);
     // printf("iterate 22 max_priority:%d fdCount:%d %d time:%lli\n",max_priority,fdCount,timeout,Time.monotonic()/1000);
      if(!block)
        timeout=0;
      int eventCount= poll(fdCount,timeout);
      //printf("iterate ---22 max_priority:%d fdCount:%d %d time:%lli\n",max_priority,fdCount,timeout,Time.monotonic()/1000);

      //printf("iterate 33 max_priority:%d fdCount:%d timeout:%d some_ready:%d eventCount:%d\n",max_priority,fdCount,timeout,some_ready,eventCount);

      some_ready = check( max_priority, epollEvents, fdCount,eventCount);
     // printf("iterate 44 max_priority:%d fdCount:%d timeout:%d some_ready:%d eventCount:%d pendingDispatchesCount:%d\n",
           //   max_priority,fdCount,timeout,some_ready,eventCount,pendingDispatchesCount);

      if (dispatch)
          excute();

      release();
      mutex.lock();
      return some_ready;
    }

    aboolean iterate (aboolean mayBlock){
      aboolean retval;
      mutex.lock();
      retval = iterate (mayBlock, TRUE, AThread.current());
      mutex.unlock();
      return retval;
    }

    void  wakupSignal(){
        wakeupSource->signal();
    }


    auint add(EventSource *source){
      auint result = 0;
      if(source->eventMgr!=NULL){
          a_warning("事件源已加入到资源管理器了。%p %p\n",self,source);
          return 0;
      }
      a_return_val_if_fail (source != NULL, 0);
      mutex.lock();
      result = storage->add(source);
      if(result){
         source->setEventMgr((EventMgr*)self);
         poll->add(source);
      }
      if (result && owner && owner != AThread.current())
          wakupSignal();
      mutex.unlock();
      return result;
    }

    aboolean pending() {
      aboolean retval;
      mutex.lock();
      retval = iterate (FALSE, FALSE, AThread.current());
      mutex.unlock();
      return retval;
    }

    EventSource *find(auint id){
        EventSource *source=NULL;
        a_return_val_if_fail (id > 0, NULL);
        mutex.lock();
        source =storage->find(id);
        mutex.unlock();
        return source;
    }


   static  void removePollUnlocked(EventSource *source){

   }

    void  destroy(EventSource *source,aboolean  have_lock){
      if (!have_lock)
        mutex.lock ();
      if (!SOURCE_DESTROYED (source)){
          if (!SOURCE_BLOCKED (source)){
              removePollUnlocked (source);
          }
          storage->remove(source);
          poll->delete(source);
          source->flags &= ~HookFlagMask.ACTIVE;
      }
      if (!have_lock)
          mutex.unlock ();
    }

    aboolean remove(EventSource *source){
        a_return_val_if_fail (source != NULL,FALSE);
        //printf("remove ---- source:%p %d\n",source,(source->flags & HookFlagMask.ACTIVE));
        if (source->eventMgr)
            destroy(source, FALSE);
        else
          source->flags &= ~HookFlagMask.ACTIVE;
        return TRUE;
    }

     aboolean remove(auint id){
         EventSource *source=find(id);
         if(source==NULL)
             return FALSE;
         return remove(source);
     }


    void   destroy(apointer source){
        //printf("destroyFunc_cb --- mgr:%p source:%p\n",self,source);
        self->remove((EventSource*)source);
    }

    void   setPriority(apointer source,int newPriority){
          a_return_if_fail (source != NULL);
          mutex.lock();
          storage->setPriority((EventSource*)source, newPriority);
          mutex.unlock();
    }

    void   setReadyTime(apointer eventSource,aint64 readyTime){
        mutex.lock();

        EventSource *source=(EventSource *)eventSource;
        //printf("g_source_set_ready_time %p id:%d new:%lli old:%lli\n",source,source->id,readyTime/1000,source->ready_time/1000);
        if (source->ready_time == readyTime){
            mutex.unlock();
            return;
        }
        source->ready_time = readyTime;
        if (!SOURCE_BLOCKED (source)){
           // printf("g_source_set_ready_time 11 %p id:%d new:%lli old:%lli\n",source,source->id,readyTime/1000,source->ready_time/1000);
            wakeupSource->signal();
        }
        mutex.unlock();
    }

    aboolean removeAll(){
          mutex.lock();
          int i;
          for (i = 0; i <pendingDispatchesCount; i++)
              pendingDispatches[i]=NULL;
          //printf("asourcemg refcotn:%d\n",self->refCount);
          storage->removeAll();
          //printf("asourcemg r111 efcotn:%d\n",self->refCount);

          mutex.unlock();
          return TRUE;
    }

    aboolean isOwner(){
      aboolean is=FALSE;
      mutex.lock();
      is = owner == AThread.current();
      mutex.unlock();
      return is;
    }

    void invoke(int priority,EventSourceCallback callback,apointer data,ADestroyNotify notify){

      a_return_if_fail (callback != NULL);
      if (isOwner()){
          while (callback (data));
          if (notify != NULL)
            notify (data);
      }else{
          ASourceMgr *defaultMgr=getDefault();
          if(defaultMgr==self && acquire()){
              printf("invoke acquire ok\n");
              while (callback (data));
              release ();
              if (notify != NULL)
                 notify (data);
          }else{
              EventSource *source=new$ IdleSource();
              source->setPriority(priority);
              source->setCallback(callback, data, notify);
              add(source);
              source->unref();
          }
        }
    }

    void invoke(EventSourceCallback callback,apointer data){
        invoke (Priority.DEFAULT,callback,data,NULL);
    }

    /**
     * @implements EventMgr
     */
    aint64   getTime(){
         mutex.lock();
         //printf("gettime is -----eventTime.time_is_fresh:%d %lli\n",eventTime.time_is_fresh,eventTime.time/1000);
         if (!eventTime.time_is_fresh){
             eventTime.time = Time.monotonic();
             eventTime.time_is_fresh = TRUE;
         }
         aint64 result = eventTime.time;
         mutex.unlock();
         return result;
    }

    /**
     * 允许的最大eventsource数量。
     */
    void setMaxSource(auint maxSource){
        storage->setMaxSource(maxSource);
    }

    void setStartId(auint startId){
        storage->setStartId(startId);
    }

    void free(){
        removeAll();
        unref();
    }


    ~ASourceMgr(){
          printf("free ASourceMgr\n");
          auint i;
          for (i = 0; i <pendingDispatchesCount; i++)
              pendingDispatches[i]=NULL;
          mutex.lock();
          storage->removeAll();
          mutex.unlock();
          storage->unref();
          poll->unref();
          wakeupSource->unref();
    }

};
