

#ifndef __AET_MAIN_EVENT_SOURCE_H__
#define __AET_MAIN_EVENT_SOURCE_H__

#include <objectlib.h>
#include "EventOps.h"
#include "EventMgr.h"


package$ aet.main;


typedef aboolean (*EventSourceCallback) (apointer data);

typedef struct _EventSourceFuncs  EventSourceFuncs;

#define HOOK_FLAG_USER_SHIFT  (4)


public$ abstract$ class$ EventSource implements$ EventOps{

    public$ enum$ Flags{
       READY       = 1 << HOOK_FLAG_USER_SHIFT,
       CAN_RECURSE = 1 << (HOOK_FLAG_USER_SHIFT + 1),
       BLOCKED     = 1 << (HOOK_FLAG_USER_SHIFT + 2)
    };

    public$ enum$ Priority{
       HIGH=-50,
       DEFAULT=0,
       HIGH_IDLE=10,
       DEFAULT_IDLE=30,
       LOW=50
    };

    public$ enum$ HookFlagMask{
       ACTIVE=1<<0,
       IN_CALL=1<<1,
       MASK     = 0X0f
    };

    apointer callbackUserData;
    EventSourceCallback callback;
    ADestroyNotify  notify;
    EventSourceFuncs *funcs;
    EventMgr *eventMgr;
    int priority;
    auint flags;
    auint id;
    auint    memPos;//保存在数组的位置，为了加速移走
    auint    memPollPos;//保存在poll数组的位置，为了加速移走
    char    *name;
    aint64   ready_time;
    aboolean havePoll;//是不是有fd的source

    EventSource(char *name);
    EventSource(EventSourceFuncs *funcs);
    protected$ void  setId(auint id);
    public$ auint    getId();
    public$ int      getPriority();
    public$ void     setPriority(int priority);

    public$ void     setFuncs(EventSourceFuncs *funcs);
    public$ aboolean isActive();
    public$ aboolean canRecurse();
    public$ char        *getName();
    public$ void         setCanRecurse(aboolean can);
    public$ void         setName(char *name);
    protected$ void      setEventMgr(EventMgr *eventMgr);
    public$ void         destroy();
    public$ void         setCallback(EventSourceCallback callback,apointer data,ADestroyNotify  notify);

    protected$ aboolean  havePoll();//有打开文件的资源
    protected$ abstract$ int getFd();
    protected$ abstract$ int getEvents();
    public$   void       setReadyTime(aint64 readyTime);
    public$   aint64     getTime();
    public$   aboolean   isDestroyed();
    public$   aint64     getReadyTime();


};

struct _EventSourceFuncs  {
     aboolean (*prepare)   (EventSource *source,int  *timeout);
     aboolean (*check)     (EventSource *source);
     aboolean (*dispatch)  (EventSource *source);
};




#endif /* __N_MEM_H__ */

