

#ifndef __AET_MAIN_SOURCE_STORAGE_H__
#define __AET_MAIN_SOURCE_STORAGE_H__

#include <objectlib.h>
#include "../util/AMutex.h"
#include "../util/ACond.h"
#include "../lang/AThread.h"
#include "../util/ASList.h"
#include "../util/AHashTable.h"
#include "../util/AArray.h"
#include "EventOps.h"
#include "EventSource.h"

package$ aet.main;



public$ abstract$ class$ SourceStorage{

     AMutex      mutex;
     AHashTable  *sourcesHash;
     private$     auint maxSource;
     protected$   auint nextId;//SOURCE的ID

     public$  SourceStorage();
     public$  EventSource         *find(auint id);
     public$  abstract$ auint      add(EventSource *source);
     public$  abstract$ aboolean   remove(EventSource *source);
     public$  abstract$ void       setPriority(EventSource *source,int priority);
     public$  abstract$ void       removeAll();

     public$  abstract$  aboolean  nextIter(EventSource **source);
     public$  abstract$  void      initIter();
     public$  abstract$  aboolean  pollNextIter(EventSource **source);
     public$  abstract$  void      pollInitIter();
     public$  void                 setMaxSource(auint max);
     public$  auint                getMaxSource();
     public$  void                 setStartId(auint startId);

};




#endif /* __N_MEM_H__ */

