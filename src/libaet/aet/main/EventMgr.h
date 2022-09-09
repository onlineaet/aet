
#ifndef __AET_MAIN_EVENT_MGR_H__
#define __AET_MAIN_EVENT_MGR_H__

#include <alib.h>
#include <objectlib.h>

package$ aet.main;




public$ interface$ EventMgr{

    void   destroy(apointer source);
    void   setPriority(apointer source,int priority);
    void   setReadyTime(apointer eventSource,aint64 readyTime);
    aint64 getTime();

};


#endif /* __N_MEM_H__ */

