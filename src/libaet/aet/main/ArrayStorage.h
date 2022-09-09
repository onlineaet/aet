

#ifndef __AET_MAIN_ARRAY_STORAGE_H__
#define __AET_MAIN_ARRAY_STORAGE_H__

#include <stdarg.h>
#include <objectlib.h>
#include "../util/AMutex.h"
#include "../util/ACond.h"
#include "../lang/AThread.h"
#include "../util/ASList.h"
#include "../util/AHashTable.h"
#include "../io/APoll.h"
#include "../util/AArray.h"
#include "EventOps.h"
#include "EventSource.h"
#include "SourceStorage.h"

package$ aet.main;



public$ class$ ArrayStorage extends$ SourceStorage{

     EventSource *sourcesArray[1024];//保存source的数组,加在最后，移走重新排列
     auint        sourceCount;
     void        *prioArray[Priority.LOW-Priority.HIGH+1];//每个优先级对应一个eventsource *;
     int          prioIndex[Priority.LOW-Priority.HIGH+1];
     int          actualPrioCount;
     EventSource *epollSources[1024];
     auint        epollSourceCount;
     auint        initIterValue;
     auint        initIterCount;
     auint        pollInitIterValue;

     public$ ArrayStorage();
};




#endif /* __N_MEM_H__ */

