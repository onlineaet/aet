

#ifndef __AET_UTIL_A_ASYNC_QUEUE_H__
#define __AET_UTIL_A_ASYNC_QUEUE_H__

#include <stdarg.h>
#include <objectlib.h>
#include "AMutex.h"
#include "ACond.h"
#include "AQueue.h"


package$ aet.util;



public$ class$ AAsyncQueue{
    AMutex mutex;
    ACond cond;
    AQueue *queue;
    ADestroyNotify freeFunc;
    auint waitingThreads;
    public$           AAsyncQueue(ADestroyNotify freeFunc);
    public$ void      lock ();
    public$ void      unlock ();
    public$ void      push(apointer data);
    private$ void     pushFront(apointer data);
    private$ void     pushFrontUnlocked(apointer data);
    protected$ void   pushUnlocked(apointer data);
    private$ apointer popUnlocked (aboolean wait,aint64 endTime);
    public$ apointer  pop();
    public$ apointer  popUnlocked ();
    public$ apointer  popUnlocked (auint64 timeout);
    public$ apointer  pop(auint64 timeout);
    public$ int       length();
    public$ int       lengthUnlocked();
    public$ void      sortUnlocked(ACompareDataFunc  func,apointer userData);
    public$ void      sort(ACompareDataFunc  func,apointer userData);
    public$ apointer  tryPop();
    public$ apointer  tryPopUnlocked();
    public$ aboolean  remove(apointer item);
    public$ aboolean  removeUnlocked(apointer item);
    public$ void      pushSortedUnlocked(apointer data,ACompareDataFunc func,apointer userData);
    public$ void      pushSorted(apointer data,ACompareDataFunc func,apointer userData);
    public$ AMutex   *getMutex();

};




#endif /* __N_MEM_H__ */


