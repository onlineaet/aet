#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <aet/time/Time.h>
#include <aet/lang/AAssert.h>
#include <aet/lang/AThread.h>
#include <aet/lang/System.h>

#include "AThreadPool.h"

typedef struct
{
  AThreadPool *pool;
  AThread *thread;
  AError *error;
} SpawnThreadData;

static apointer spawn_cb(apointer data);

static const apointer wakeup_thread_marker={'0','1','2','7','r','2','a'};

/**
 * 管理整个进程中由线程池生成的线程
 */
class$ UnusedThread{
    aint wakeupSerial;
    AAsyncQueue *unusedQueue;
    int unusedThreads ;
    int maxUnusedThreads;
    int killUnusedThreads;
    auint maxIdleTime;
    ACond spawnCond;
    AAsyncQueue *spawnQueue;
    ASchedSettings schedulerSetting;
    aboolean haveSchedulerSettings ;
    AMutex lock;
    AThread *spawnThread;

    private$ AThreadPool *waitPool();
    private$ aboolean addPool(AThreadPool *pool);
    private$ void  lock();
    private$ void  unlock();
    private$ void  wait();
    private$ void  init(aboolean excutive);
    private$ void  setMaxUnusedThreads(int maxThreads);
    private$ int   getMaxUnusedThreads();
    private$ auint getUnusedThreads();
    private$ void  stopUnusedThreads();
    private$ void  setMaxIdleTime(auint interval);
    private$ auint getMaxIdleTime();
};

impl$ UnusedThread{

    UnusedThread(){
         haveSchedulerSettings = FALSE;
         unusedQueue = new$ AAsyncQueue();
         spawnQueue=NULL;
         spawnThread=NULL;
         unusedThreads = 0;
         wakeupSerial = 0;
         maxUnusedThreads = 2;
         killUnusedThreads = 0;
         maxIdleTime = 15 * 1000;
         lock=new$ AMutex();
    }

    void setMaxUnusedThreads(int maxThreads){
      a_return_if_fail (maxThreads >= -1);
      a_atomic_int_set (&maxUnusedThreads, maxThreads);
      if (maxThreads != -1){
          maxThreads -= a_atomic_int_get (&unusedThreads);
          if (maxThreads < 0){
              a_atomic_int_set (&killUnusedThreads, -maxThreads);
              a_atomic_int_inc (&wakeupSerial);
              unusedQueue->lock();
              do{
                  unusedQueue->pushUnlocked(wakeup_thread_marker);
              } while (++maxThreads);
              unusedQueue->unlock();
          }
       }
    }

    int getMaxUnusedThreads(){
      return a_atomic_int_get (&maxUnusedThreads);
    }

    auint getUnusedThreads(void){
      return (auint) a_atomic_int_get (&unusedThreads);
    }

    void stopUnusedThreads(){
      auint oldval;
      oldval = getMaxUnusedThreads ();
      setMaxUnusedThreads(0);
      setMaxUnusedThreads(oldval);
    }

    void setMaxIdleTime(auint interval) {
      auint i;
      a_atomic_int_set (&maxIdleTime, interval);
      i = (auint) a_atomic_int_get(&unusedThreads);
      if (i > 0){
          a_atomic_int_inc (&wakeupSerial);
          unusedQueue->lock();
          do{
              unusedQueue->pushUnlocked(wakeup_thread_marker);
          }while (--i);
          unusedQueue->unlock();
      }
    }

    auint getMaxIdleTime(){
      return (auint) a_atomic_int_get (&maxIdleTime);
    }

    aboolean addPool(AThreadPool *pool){
        aboolean sucess=FALSE;
        unusedQueue->lock();
        //a_debug("addPool to UnusedThread--- %d",unusedQueue->lengthUnlocked());
        if (unusedQueue->lengthUnlocked() < 0){
            unusedQueue->pushUnlocked(pool);
            sucess = TRUE;
        }
        unusedQueue->unlock();
        return sucess;
    }

    void lock(){
        spawnQueue->lock();
    }
    void unlock(){
        spawnQueue->unlock();
    }

    void wait(){
        AMutex *mutex= spawnQueue->getMutex();
        spawnCond.wait(mutex);
    }

    void init(aboolean excutive){
        lock.lock();
        if (!excutive && !haveSchedulerSettings && !spawnQueue){
            if (AThread.getShedulerSettings(&schedulerSetting)){
                haveSchedulerSettings = TRUE;
            }else{
                spawnQueue = new$ AAsyncQueue();
                spawnCond=new$ ACond();
                spawnThread=new$ AThread("pool-spawner", spawn_cb, NULL);
            }
        }
        lock.unlock();
    }

    AThreadPool *waitPool(){
      AThreadPool *pool=NULL;
      int local_wakeup_thread_serial;
      auint local_max_unused_threads;
      int local_max_idle_time;
      int last_wakeup_thread_serial;
      aboolean have_relayed_thread_marker = FALSE;

      local_max_unused_threads = (auint) a_atomic_int_get (&maxUnusedThreads);
      local_max_idle_time = a_atomic_int_get (&maxIdleTime);
      last_wakeup_thread_serial = a_atomic_int_get (&wakeupSerial);

      a_atomic_int_inc (&unusedThreads);
      AThread *selfThread=AThread.current();
      do{
          if ((auint) a_atomic_int_get (&unusedThreads) >= local_max_unused_threads){
              /* 如果这是一个多余的线程，停止它。 */
              pool = NULL;
          }else if (local_max_idle_time > 0){
              /* If a maximal idle time is given, wait for the given time. */
              a_debug("线程 %p 在全局线程池中等 %f 秒。",selfThread, local_max_idle_time / 1000.0);
              pool = unusedQueue->pop(local_max_idle_time * 1000);
          }else{
              /* If no maximal idle time is given, wait indefinitely. */
              a_warning("线程 %p 在全局线程池中等。", selfThread);
              pool = unusedQueue->pop();
          }

          if (pool == wakeup_thread_marker){
              local_wakeup_thread_serial = a_atomic_int_get (&wakeupSerial);
              if (last_wakeup_thread_serial == local_wakeup_thread_serial){
                  if (!have_relayed_thread_marker){
                        a_debug("thread %p relaying wakeup message to waiting thread with lower serial.",selfThread);
                        unusedQueue->push(wakeup_thread_marker);
                        have_relayed_thread_marker = TRUE;

                        /*如果已中继唤醒标记，则此线程
                        *将让开100微秒以
                        *避免再次收到此标记。
                        */
                        AThread.sleep(100);
                   }
              }else{
                   if (a_atomic_int_add (&killUnusedThreads, -1) > 0){
                      pool = NULL;
                      break;
                   }
                   a_warning("thread %p updating to new limits.",selfThread);
                   local_max_unused_threads = (auint) a_atomic_int_get (&maxUnusedThreads);
                   local_max_idle_time = a_atomic_int_get (&maxIdleTime);
                   last_wakeup_thread_serial = local_wakeup_thread_serial;
                   have_relayed_thread_marker = FALSE;
              }
           }
        }while (pool == wakeup_thread_marker);

        a_atomic_int_add (&unusedThreads, -1);
        return pool;
    }
};

static UnusedThread *unusedThread=new$ UnusedThread();

static void freePool(AThreadPool *pool)
{
   // printf("内部清除xx-----00\n");

      a_return_if_fail (pool->running == FALSE);
      a_return_if_fail (pool->numThreads == 0);
      //printf("内部清除xx-----11\n");

      pool->queue->remove(AUINT_TO_POINTER (1));
      //printf("内部清除xx-----22\n");

      pool->queue->unref();
      pool->unref();
     // printf("内部清除xx-----33\n");

}

static void bindCpu(AThreadPool *self)
{
       cpu_set_t mask;
       cpu_set_t get;
       char buf[256];
//       int i=2;
//       if(numThreads==1)
//           i=2;
//       else if(numThreads==2)
//           i=4;
       int i=self->numThreads*2+1;
       int j;
       int num= System.getCpuThreads();
       AThread *thread=AThread.current();
       pthread_t   systemThread=thread->getSystemThread();

       printf("system has %d processor(s) bind:%d %p\n", num,i,thread);
          CPU_ZERO(&mask);
          CPU_SET(i,&mask);
          if(pthread_setaffinity_np(systemThread, sizeof(mask),&mask)< 0){
              fprintf(stderr,"set thread affinity failed\n");
          }
          CPU_ZERO(&get);
          if(pthread_getaffinity_np(systemThread, sizeof(get),&get)< 0){
              fprintf(stderr,"get thread affinity failed\n");
          }
          for(j= 0; j< num; j++){
              if(CPU_ISSET(j,&get)){
                  printf("thread %d is running in processor %d numThread:%d %p\n",(int)pthread_self(), j,self->numThreads,thread);
              }
          }
}


static apointer  proxy_cb (apointer userData)
{
    AThreadPool *pool=userData;
    AThread *selfThread=AThread.current();
    bindCpu(pool);
   // a_debug("g_thread_pool_thread_proxy 00 %p rnning:%d immediate:%d self:%p\n",pool,pool->running,pool->immediate,selfThread);
    pool->queue->lock();
   // a_debug("g_thread_pool_thread_proxy 11 %p rnning:%d immediate:%d self:%p\n",pool,pool->running,pool->immediate,selfThread);
    while (TRUE){
        apointer task;

       // a_debug("g_thread_pool_thread_proxy 22 %p rnning:%d immediate:%d self:%p\n",pool,pool->running,pool->immediate,selfThread);
        task = pool->waitNewTask ();
       // a_debug("g_thread_pool_thread_proxy 33 %p rnning:%d immediate:%d self:%p task:%p\n",pool,pool->running,pool->immediate,selfThread,task);

        if (task){
            if (pool->running || !pool->immediate){
                /*接收到任务且线程池处于活动状态，
                *所以执行这个函数。*/
                pool->queue->unlock();
                //a_debug ("thread %p in pool %p calling func.",AThread.current(), pool);
               // a_debug("g_thread_pool_thread_proxy 44 %p rnning:%d immediate:%d self:%p task:%p\n",pool,pool->running,pool->immediate,selfThread,task);

                pool->func(task, pool->userData);
                pool->queue->lock();
            }
         }else{
            /* 没任务了,线程进池中 */
            aboolean free_pool = FALSE;
           // a_debug("thread %p leaving pool %p for global pool.",AThread.current(), pool);
            pool->numThreads--;
            if (!pool->running){
                if (!pool->waiting){
                    if (pool->numThreads == 0){
                        /* If the pool is not running and no other
                         * thread is waiting for this thread pool to
                         * finish and this is the last thread of this
                         * pool, free the pool.
                         */
                        free_pool = TRUE;
                     }else{
                        /* If the pool is not running and no other
                         * thread is waiting for this thread pool to
                         * finish and this is not the last thread of
                         * this pool and there are no tasks left in the
                         * queue, wakeup the remaining threads.
                         */
                        if (pool->queue->lengthUnlocked() ==(int) -pool->numThreads)
                            pool->wakeupAndStopAll();
                     }
                }else if (pool->immediate ||pool->queue->lengthUnlocked() <= 0){
                    /* If the pool is not running and another thread is
                     * waiting for this thread pool to finish and there
                     * are either no tasks left or the pool shall stop
                     * immediately, inform the waiting thread of a change
                     * of the thread pool state.
                     */
                    pool->cond.broadcast();
                }
            }

            pool->queue->unlock();
            //a_debug("free_pool 00 %d thread:%p\n",free_pool,selfThread);

            if (free_pool){
               // a_debug("free_pool 11 真正释放 %d thread:%p\n",free_pool,selfThread);
                freePool(pool);
            }
            //a_debug("free_pool 22 %d thread:%p\n",free_pool,selfThread);

            if ((pool = unusedThread->waitPool()) == NULL){
               // a_debug("free_pool 33 pool==NULL %d thread:%p\n",free_pool,selfThread);

                 break;
            }
            //a_debug("free_pool 44 %d thread:%p\n",free_pool,selfThread);

            pool->queue->lock();
           // a_debug("free_pool 55 %d thread:%p\n",free_pool,selfThread);

            //a_debug("thread %p entering pool %p from global pool.",AThread.current(), pool);

            /* pool->num_threads++ is not done here, but in
             * g_thread_pool_start_thread to make the new started
             * thread known to the pool before itself can do it.
             */
          }
      }
   // a_debug("proxy_cb 退出了----\n");
      return NULL;
}

static apointer spawn_cb(apointer data)
{
  while (TRUE){
      SpawnThreadData *spawn_thread_data;
      AThread *thread = NULL;
      AError *error = NULL;
      const char *prgname ="unknown";// g_get_prgname ();
      char name[16] = "pool";

      if (prgname)
        a_snprintf (name, sizeof (name), "pool-%s", prgname);
      printf("spawn_cb ----\n");
      unusedThread->spawnQueue->lock();
      /* Spawn a new thread for the given pool and wake the requesting thread
       * up again with the result. This new thread will have the scheduler
       * settings inherited from this thread and in extension of the thread
       * that created the first non-exclusive thread-pool. */
      spawn_thread_data =unusedThread->spawnQueue->popUnlocked ();
      thread = new$ AThread (name, proxy_cb,spawn_thread_data->pool);

      spawn_thread_data->thread = thread;//g_steal_pointer (&thread);
      spawn_thread_data->error =NULL;// g_steal_pointer (&error);
      unusedThread->spawnCond.broadcast();
      unusedThread->spawnQueue->unlock();
   }
  printf("spawn_cb ----exits\n");

   return NULL;
}

static aint64 totalTime=0;
static int counts=0;


impl$ AThreadPool{

    static void setMaxUnusedThreads(int maxThread){
        unusedThread->setMaxUnusedThreads(maxThread);
    }

    static int getMaxUnusedThreads(){
       return unusedThread->getMaxUnusedThreads();
    }

    static auint getUnusedThreads(){
        return unusedThread->getUnusedThreads();
    }

    static void stopUnusedThreads(){
        unusedThread->stopUnusedThreads();
    }

    static void setMaxIdleTime(auint interval){
        unusedThread->setMaxIdleTime(interval);
    }

    static auint getMaxIdleTime(){
        return unusedThread->getMaxIdleTime();
    }

    apointer waitNewTask(){
      apointer task = NULL;
      if (self->running || (!self->immediate && queue->lengthUnlocked() > 0)){
           /* This thread pool is still active. */
           if (maxThreads != -1 && numThreads > (auint) maxThreads){
              /* This is a superfluous thread, so it goes to the global pool. */
               a_debug ("superfluous thread %p in pool %p.",AThread.current(), self);
            }else if (exclusive){
              /* Exclusive threads stay attached to the pool. */
              task =queue->popUnlocked();
              a_debug("thread %p in exclusive pool %p waits for task (%d running, %d unprocessed).",
                          AThread.current(), self,numThreads,queue->lengthUnlocked());
            }else{
              /* A thread will wait for new tasks for at most 1/2
               * second before going to the global pool.
               */
                a_debug("thread %p in pool %p waits for up to a 1/2 second for task (%d running, %d unprocessed).",
                          AThread.current(), self,numThreads,queue->lengthUnlocked());
                task = queue->popUnlocked (Time.UsecPerSec/2);
            }
      }else{
          /* This thread pool is inactive, it will no longer process tasks. */
          a_debug("pool %p not active, thread %p will go to global pool (running: %s, immediate: %s, len: %d).",
                      self,  AThread.current(),running ? "true" : "false",immediate ? "true" : "false",
                              queue->lengthUnlocked());
      }
      return task;
    }

    aboolean  startThread(AError **error){
      aboolean success = FALSE;
      aint64 t1,t2,t3;
      t1=Time.monotonic();
      /* 运行的线程已达到设定的最大值 */
      if (maxThreads != -1 && numThreads >= (auint)maxThreads){
          printf(" 运行的线程已达到设定的最大值:maxThreads:%d numThreads:%d\n",numThreads);
        return TRUE;
      }
      success=unusedThread->addPool(self);
      t2=Time.monotonic();
      if (!success){
          char threadName[256] ;
          AThread *thread=NULL;
          sprintf(threadName,"%s-%d",name?name:"null",self->numThreads);
         // printf("startThread ----11 %d\n",exclusive);
          /* 没有线程被找到，开始一个新线程 */
           if (exclusive){
              /*对于独占线程池，直接从new（）和
              *我们只需启动继承调度程序设置的新线程
              *来自当前线程。
              */
              thread = new$ AThread(threadName, proxy_cb, (apointer)self);
             printf("startThread ----22 %d thread:%p\n",exclusive,thread);
           }else{
              /* For non-exclusive thread-pools this can be called at any time
               * when a new thread is needed. We make sure to create a new thread
               * here with the correct scheduler settings: either by directly
               * providing them if supported by the GThread implementation or by
               * going via our helper thread.
               */
              if (unusedThread->haveSchedulerSettings){
                  thread = new$ AThread(threadName, proxy_cb, (apointer)self, 0, &unusedThread->schedulerSetting);
              }else{
                  SpawnThreadData spawnData = { (AThreadPool *) self, NULL, NULL };
                  unusedThread->lock();
                  unusedThread->spawnQueue->pushUnlocked (&spawnData);
                  while (!spawnData.thread && !spawnData.error)
                      unusedThread->wait();
                  thread = spawnData.thread;
                  if (!thread){
                      a_error("找不到未用的线程 thread==NULL");
                  }
                  unusedThread->unlock();
              }
           }

          if (thread == NULL)
            return FALSE;
         // printf("startThread ----33 %d %p %d\n",exclusive,thread,thread->refCount);
          thread->unref();
          t3=Time.monotonic();
          totalTime+=(t3-t1);
          counts++;
          //printf("startThread ----%lli %lli %p %lli %d %f\n",(long long)(t3-t2),(long long)(t2-t1),thread,totalTime,counts,(totalTime*1.0)/counts);
        }

      /* See comment in g_thread_pool_thread_proxy as to why this is done
       * here and not there
       */
      self->numThreads++;
      return TRUE;
    }

    void  wakeupAndStopAll(){
      auint i;
      a_return_if_fail (running == FALSE);
      a_return_if_fail (numThreads != 0);
      immediate = TRUE;
      /*
       * So here we're sending bogus data to the pool threads, which
       * should cause them each to wake up, and check the above
       * pool->immediate condition. However we don't want that
       * data to be sorted (since it'll crash the sorter).
       */
      for (i = 0; i <numThreads; i++)
         queue->pushUnlocked(AUINT_TO_POINTER (1));
    }



    AThreadPool(char *name,AFunc func,apointer  userData,int maxThreads,aboolean exclusive,AError  **error){

        a_return_val_if_fail (func, NULL);
        a_return_val_if_fail (!exclusive || maxThreads != -1, NULL);
        a_return_val_if_fail (maxThreads >= -1, NULL);
        self->func = func;
        self->userData = userData;
        self->exclusive = exclusive;
        self->queue = new$ AAsyncQueue();
        self->cond=new$ ACond();
        self->maxThreads = maxThreads;
        self->numThreads = 0;
        self->running = TRUE;
        self->immediate = FALSE;
        self->waiting = FALSE;
        self->sortFunc = NULL;
        self->sortUserData = NULL;
        if(name!=NULL)
            self->name=a_strdup(name);
        else
            self->name=NULL;
        unusedThread->init(exclusive);
        if (self->exclusive){
            queue->lock();
            printf("AThreadPool --- 33----exclusive:%d\n",exclusive);
            while (self->numThreads < (auint) self->maxThreads){
                AError *curError = NULL;
                printf("AThreadPool --- 44----exclusive:%d numThreads:%d maxThreads:%d\n",exclusive,numThreads,maxThreads);
                if (!startThread(&curError)){ //启动线程
                    a_error_transfer(error, curError);
                    break;
                  }
            }
            queue->unlock();
        }
    }


    aboolean push(apointer data,AError **error) {
      if(!running)
          return FALSE;
      aboolean result = TRUE;
      queue->lock();
      //printf("push---00 name:%s %p %d %p\n",name,AThread.current(),queue->lengthUnlocked(),self->sortFunc);
      if (queue->lengthUnlocked() >= 0){
          AError *curError = NULL;
          printf("push---11 name:%s %p %d\n",name,AThread.current(),queue->lengthUnlocked());

          if (!startThread(&curError)){
              a_error_transfer(error, curError);
              result = FALSE;
          }
          //printf("push---22 name:%s %p %d\n",name,AThread.current(),queue->lengthUnlocked());

      }
      if (self->sortFunc)
         queue->pushSortedUnlocked (data,self->sortFunc,sortUserData);
      else
         queue->pushUnlocked (data);
      queue->unlock();
      return result;
    }

    void internalClear(){
       // printf("内部清除----00-\n");

      a_return_if_fail (running == FALSE);
      a_return_if_fail (numThreads == 0);
      queue->remove(AUINT_TO_POINTER (1));
      queue->unref();
      unref();
    }

    void free(aboolean immediate,aboolean wait_){
      // printf("g_thread_pool_free is 00 :%d %d\n",running,immediate);
      a_return_if_fail (running);
      a_return_if_fail (immediate || maxThreads != 0 || queue->length() == 0);
      queue->lock();
      running = FALSE;
      self->immediate = immediate;
      waiting = wait_;
      if (wait_){
          while (queue->lengthUnlocked()!= (int)-numThreads && !(immediate && numThreads == 0)){
              //printf("g_thread_pool_free 22 name:%s wait wait %p %d %d\n",name,AThread.current(),running,queue->lengthUnlocked());
              cond.wait(queue->getMutex());
          }
      }

      if (immediate || queue->lengthUnlocked() == (int)-numThreads){
          /* No thread is currently doing something (and nothing is left
           * to process in the queue)
           */
          if (numThreads == 0){
              /* 没有线程了，可以清除 */
              queue->unlock();
              //printf("g_thread_pool_free 33 没有线程了，可以清除 name:%s %p %d %d\n",name,AThread.current(),running,queue->lengthUnlocked());

              internalClear();
              return;
           }
         // printf("g_thread_pool_free 44 name:%s %p %d %d\n",name,AThread.current(),running,queue->lengthUnlocked());

          wakeupAndStopAll();
          printf("g_thread_pool_free 55 name:%s %p %d %d\n",name,AThread.current(),running,queue->lengthUnlocked());

      }
      //printf("g_thread_pool_free 66 name:%s %p %d %d\n",name,AThread.current(),running,queue->lengthUnlocked());

      waiting = FALSE;
      queue->unlock();
      //printf("g_thread_pool_free name:%s 77\n",name);
    }

    void setSortFunction(ACompareDataFunc  func,apointer userData) {
        a_return_if_fail (running);
        queue->lock();
        sortFunc = func;
        sortUserData = userData;
        if (func)
          queue->sortUnlocked (sortFunc,sortUserData);
        queue->unlock();
    }

    auint getThreadCount(){
      auint retval;
      a_return_val_if_fail (running, 0);
      queue->lock();
      retval = numThreads;
      queue->unlock();
      return retval;
    }

    auint getUnprocessed (){
      int unprocessed;
      a_return_val_if_fail (running, 0);
      unprocessed =queue->length();
      return MAX (unprocessed, 0);
    }

    int getMaxThreads(){
      int retval;
      a_return_val_if_fail (running, 0);
      queue->lock();
      retval = maxThreads;
      queue->unlock();
      return retval;
    }

    aboolean setMaxThreads(int max_threads,AError **error) {
      int to_start;
      aboolean result;
      a_return_val_if_fail (running, FALSE);
      a_return_val_if_fail (!exclusive || max_threads != -1, FALSE);
      a_return_val_if_fail (max_threads >= -1, FALSE);
      result = TRUE;

      queue->lock();

      self->maxThreads = max_threads;

      if (exclusive)
        to_start = maxThreads - numThreads;
      else
        to_start = queue->lengthUnlocked();

      for ( ; to_start > 0; to_start--){
          AError *innerError = NULL;
          if (!startThread(&innerError)){
              a_error_transfer(error, innerError);
              result = FALSE;
              break;
          }
      }
      queue->unlock();
      return result;
    }

    ~AThreadPool(){
       // printf("~AThreadPool----释放-\n");
      // clear(TRUE,TRUE);
         if(name)
             a_free(name);
    }
};
