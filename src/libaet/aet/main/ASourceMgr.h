

#ifndef __AET_MAIN_A_SOURCE_MGR_H__
#define __AET_MAIN_A_SOURCE_MGR_H__

#include <stdarg.h>
#include <objectlib.h>
#include "../util/AMutex.h"
#include "../util/ACond.h"
#include "../lang/AThread.h"
#include "../util/ASList.h"
#include "../util/AHashTable.h"
#include "../io/APipe.h"
#include "../util/AArray.h"
#include "EventOps.h"
#include "EventSource.h"
#include "SourceStorage.h"
#include "WakeupSource.h"
#include "EventPoll.h"
#include "EventMgr.h"

package$ aet.main;

class$ SourceDispatch{
    EventSource *source;
    int depth;
};

/**
 * 与glib的gmain比较，少了1.事件源没有父子。2.一个线程绑定一个mgr,一个mgr同时只能在一个线程里
 *
 */
public$ class$ ASourceMgr implements$ EventMgr{
     protected$ static ASourceMgr *getDefault();
     public$ static  auint addIdle(EventSourceCallback callback,apointer userData);
     public$ static  auint addIdle(int priority,EventSourceCallback callback,apointer userData,ADestroyNotify notify);
     public$ static  auint addTimeout(auint interval,EventSourceCallback callback,apointer userData);
     public$  static auint addTimeout(int priority,auint interval,EventSourceCallback callback,apointer userData,ADestroyNotify notify);
     public$ static  EventSource *getCurrentSource();
     public$ static  SourceDispatch *getDispatch();

     SourceStorage *storage;
     EventPoll *poll;
     AMutex mutex;
     ACond cond;
     AThread *owner;
     auint ownerCount;
     ASList *waiters;
     struct {
         int timeout;
         aint64   time;
         aboolean time_is_fresh;
     }eventTime;

     EventSource *pendingDispatches[1024];
     int pendingDispatchesCount;

     WakeupSource *wakeupSource;

     int in_check_or_prepare;

    public$ ASourceMgr();
    protected$ aboolean acquire();//是否已获取mgr运行的线程。
    protected$ void     lock();
    protected$ void     unlock();
    protected$ aboolean wait();
    protected$ void     release();
    protected$ aboolean iterate (aboolean mayBlock,aboolean dispatch,AThread *current);
    public$    aboolean iterate (aboolean mayBlock);

    protected$ aboolean pending();
    public$    auint    add(EventSource *source);
    public$    EventSource *find(auint id);
    public$    aboolean remove(EventSource *source);
    public$    aboolean remove(auint id);
    public$    aboolean removeAll();
    protected$ void     wakupSignal();
    public$    aboolean isOwner();
    public$    void invoke(EventSourceCallback callback,apointer data);
    public$    void invoke(int priority,EventSourceCallback callback,apointer data,ADestroyNotify notify);
    public$    void setMaxSource(auint maxSource);//允许的最大eventsource数量。
    public$    void setStartId(auint startId);
    public$    void free();

};




#endif /* __N_MEM_H__ */

