

#ifndef __AET_UTIL_A_THREAD_POOL_H__
#define __AET_UTIL_A_THREAD_POOL_H__

#include <stdarg.h>
#include <objectlib.h>
#include "AMutex.h"
#include "ACond.h"
#include "AQueue.h"
#include "AAsyncQueue.h"


package$ aet.util;



public$ class$ AThreadPool{
    public$ static void  setMaxUnusedThreads(int maxThread);
    public$ static int   getMaxUnusedThreads();
    public$ static auint getUnusedThreads();
    public$ static void  stopUnusedThreads();
    public$ static void  setMaxIdleTime(auint interval);
    public$ static auint getMaxIdleTime();
    AFunc func;
    char *name;
    apointer userData;
    aboolean exclusive;//独占
    AAsyncQueue *queue;//任务队列
    ACond cond;
    int maxThreads;
    auint numThreads;
    aboolean running;
    aboolean immediate;
    aboolean waiting;
    ACompareDataFunc sortFunc;
    apointer sortUserData;
    public$  AThreadPool(char *name,AFunc func,apointer  userData,int maxThreads,aboolean exclusive,AError  **error);
    private$ apointer waitNewTask();
    private$ void     wakeupAndStopAll();
    public$  aboolean push(apointer data,AError **error);
    public$  void     free(aboolean immediate,aboolean wait_);
    public$  void     setSortFunction(ACompareDataFunc  func,apointer userData);
    public$  auint    getThreadCount();
    public$  auint    getUnprocessed ();
    public$  int      getMaxThreads();
    public$  aboolean setMaxThreads(int max_threads,AError **error);

};




#endif /* __N_MEM_H__ */


