#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <errno.h>
#include "../lang/AAssert.h"
#include "../time/Time.h"
#include "ArrayStorage.h"
#include "../util/ASList.h"
#include "../lang/AQSort.h"


typedef struct _SourceList SourceList;
struct _SourceList
{
  EventSource *sources[1001];
  int priority;
  EventSource *datas[1001];
  int count;
};

#define PRIO_TO_INDEX(priority)  (priority<0?abs(priority)+Priority.LOW:priority)

static void freeSource_cb(EventSource *source)
{
    //printf("source -- %p ref:%u\n",source,source->refCount);
    source->unref();
}

static int compare_cb (aconstpointer p1, aconstpointer p2, apointer data)
{
  const aint *i1 = p1;
  const aint *i2 = p2;
  return *i1 - *i2;
}

impl$ ArrayStorage{

    ArrayStorage (){
       sourceCount=0;
       nextId=1;
       initIterValue=0;
       pollInitIterValue=0;
       actualPrioCount=0;
    }



    void addIndex(int priority){
        int i;
        for(i=0;i<actualPrioCount;i++){
            if(prioIndex[i]==priority)
                return;
        }
        prioIndex[actualPrioCount++]=priority;
        AQSort.sort(prioIndex, actualPrioCount, sizeof (int), compare_cb, NULL);
    }

    void removeIndex(int priority){
        int i;
        int index=A_MAXINT;
        for(i=0;i<actualPrioCount;i++){
            if(prioIndex[i]==priority){
                index=i;
                break;
            }
        }
        for(i=index+1;i<actualPrioCount;i++){
            prioIndex[i-1]=prioIndex[i];
        }
        actualPrioCount--;
    }

    void addToSourceList(EventSource *source,int priority){
          int index=PRIO_TO_INDEX(priority);
          SourceList *source_list =(SourceList *)prioArray[index];
          if(!source_list){
              source_list = a_slice_new0 (SourceList);
              source_list->priority = priority;
              prioArray[index] =source_list;
          }
          source_list->sources[source->getId()]=source->ref();
          source_list->datas[source_list->count++]=source;
          addIndex(priority);
    }



    //存在三个地方
    void addToHash(EventSource *source,auint id){
        sourcesHash->put(AUINT_TO_POINTER (id), source->ref());
    }
    /**
     * 加source到mgr
     */
    void addToSourceList(EventSource *source){
        addToSourceList(source,source->getPriority());
    }

    void addToArray(EventSource *source){
        source->memPos=sourceCount;
        sourcesArray[sourceCount++]=source->ref();
    }

    void addToEpollArray(EventSource *source){
       if(source->havePoll()){
           source->memPollPos=epollSourceCount;
           epollSources[epollSourceCount++]=source->ref();
       }
    }

    auint  addUnlocked (EventSource *source){
          auint id;
          auint max=getMaxSource();
          do{
            id = nextId++;
            if(id>max){
                printf("绕回nextid=0 max:%u\n",max);
                nextId=0;
                id=0;
            }
          }while (id == 0 || sourcesHash->contains(AUINT_TO_POINTER (id)));
          //printf("addUnlocked---- source:%p id:%d\n",source,id);
          source->setId(id);
          addToHash(source,id);
          addToSourceList (source);
          addToArray(source);
          addToEpollArray(source);
          return source->getId();
    }

    auint add(EventSource *source){
          auint result = 0;
          a_return_val_if_fail (source != NULL, 0);
          mutex.lock();
          result = addUnlocked (source);
          mutex.unlock();
          return result;
    }

    aboolean removeFromHash(EventSource *source){
         auint id=source->getId();
         aboolean ok=sourcesHash->remove(AUINT_TO_POINTER (id));
         if(ok){
             source->unref();
         }
         return ok;
    }

    void removeFromSourceList(EventSource *source){
        auint id=source->getId();
        int  prio=source->getPriority();
        int index=PRIO_TO_INDEX(prio);
        //printf("SOURCE_DESTROYED -22-- source:%p  prio:%d index:%d id:%d\n",source,prio,index,id);
        SourceList *sourceList=(SourceList *)prioArray[index];
       // printf("SOURCE_DESTROYED -33-- source:%p prio:%d index:%d %p\n",source,prio,index,sourceList);
        EventSource *src=sourceList->sources[id];
        if(src!=source){
            a_error("移走的source不是目标source src:%p remove:%p",src,source);
        }
        src->unref();
        sourceList->sources[id]=NULL;
        int i;
        int pos=A_MAXINT;
        for(i=0;i<sourceList->count;i++){
            if(sourceList->datas[i]==source){
                pos=i;
                break;
            }
        }
        for(i=pos+1;i<sourceList->count;i++){
            sourceList->datas[i-1]=sourceList->datas[i];
         }
        sourceList->count--;
        if(sourceList->count==0){
            removeIndex(prio);
        }
    }

    void removeFromArray(EventSource *source){
        int i;
        int index=source->memPos;
        EventSource *src=sourcesArray[index];
        //printf("removeFromArray----memPos: %d id:%d\n",index,source->id);
        if(src!=source){
           a_error("removeFromArray:移走的source不是目标source src:%p remove:%p",src,source);
        }
        src->unref();
        sourcesArray[index]=NULL;
        for(i=index+1;i<sourceCount;i++){
            EventSource *item=sourcesArray[i];
            sourcesArray[i-1]=item;
            item->memPos=i-1;
        }
        sourceCount--;

    }

    /**
     * 从poll数组移走source
     */
    void removeFromPollArray(EventSource *source){
        if(source->havePoll()){
            int i;
            int index=source->memPollPos;
            EventSource *src=epollSources[index];
            if(src!=source){
              a_error("removeFromPollArray:移走的source不是目标source src:%p remove:%p",src,source);
            }
            src->unref();
            epollSources[index]=NULL;
            for(i=index+1;i<epollSourceCount;i++){
                EventSource *item=epollSources[i];
                epollSources[i-1]=item;
                source->memPollPos=i-1;
            }
            epollSourceCount--;
        }
    }

    aboolean remove(EventSource *source){
        mutex.lock();
        aboolean re=removeFromHash(source);
        removeFromSourceList(source);
        removeFromArray(source);
        removeFromPollArray(source);
        mutex.unlock();
        return re;
    }

    void removeAll(){
        mutex.lock();
        HashIter iter=new$ HashIter();
        sourcesHash->iterInit(&iter);
        apointer key=NULL;
        apointer value=NULL;
        ASList list=new$ ASList();
        while(sourcesHash->iterNext(&iter,&key,&value)){
             EventSource *source=(EventSource *)value;
            // printf("removeall --00-- :%d %s ref:%u source:%p %d\n",source->id,source->name,source->refCount,source,count++);
             removeFromSourceList(source);
             removeFromArray(source);
             removeFromPollArray(source);
            // printf("removeall --11-- :%d %s ref:%u source:%p %d\n",source->id,source->name,source->refCount,source,count++);
             list.add(source);
             //source->unref();//从hashtable中减一个引用
        }
        sourcesHash->removeAll();
        list.foreach(freeSource_cb,NULL);
        mutex.unlock();
    }


    void setPriority(EventSource *source,int newPriority){
        a_return_if_fail (source != NULL);
        mutex.lock();
        removeFromSourceList(source);
        addToSourceList(source,newPriority);
        mutex.unlock();
    }

    void initIter(){
        initIterValue=0;
        initIterCount=0;
    }

    aboolean nextIter(EventSource **source){
         if(initIterValue>=actualPrioCount){
            *source=NULL;
            return FALSE;
         }
         int prio=prioIndex[initIterValue];
         int index= PRIO_TO_INDEX(prio) ;
         SourceList *list= prioArray[index];
         *source=list->datas[initIterCount];
         initIterCount++;
         if(initIterCount>=list->count){
             initIterCount=0;
             initIterValue++;
         }
         return TRUE;
    }

    aboolean pollNextIter(EventSource **source){
         if(pollInitIterValue>=epollSourceCount){
            *source=NULL;
            return FALSE;
         }
        *source=epollSources[pollInitIterValue++];
         return TRUE;
    }

    void pollInitIter(){
        pollInitIterValue=0;
    }

    ~ArrayStorage(){
        removeAll();
     }

};
