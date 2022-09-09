#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <errno.h>
#include "../lang/AAssert.h"
#include "SourceStorage.h"



impl$ SourceStorage{

    SourceStorage (){
       mutex=new$ AMutex();
       sourcesHash = new$ AHashTable(NULL, NULL);
       maxSource=2048;
    }

    EventSource *find(auint id){
        EventSource *source=NULL;
        a_return_val_if_fail (id > 0, NULL);
        mutex.lock();
        source =sourcesHash->get(AUINT_TO_POINTER (id));
        printf("find id:%d %p\n",id,source);
        mutex.unlock();
        return source;
    }

    void  setMaxSource(auint max){
        maxSource=max;
    }

    auint getMaxSource(){
        return maxSource;
    }

    void setStartId(auint startId){
        nextId=startId;
    }




   ~SourceStorage(){
       sourcesHash->unref();
    }


};
