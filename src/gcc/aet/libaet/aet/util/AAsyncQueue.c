/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../time/Time.h"
#include "../lang/AAssert.h"

#include "AAsyncQueue.h"

typedef struct _QueueSort
{
  ACompareDataFunc func;
  apointer         userData;
} QueueSort;

static int compare_cb(apointer v1,apointer v2,QueueSort *sd)
{
    return -sd->func (v1, v2, sd->userData);
}

impl$ AAsyncQueue{

    AAsyncQueue(){
       self(NULL);
    }

    AAsyncQueue(ADestroyNotify freeFunc){
        mutex=new$ AMutex();
        cond=new$ ACond();
        queue=new$ AQueue();
        waitingThreads=0;
        self->freeFunc=freeFunc;
    }

    void lock (){
       mutex.lock();
    }

    void unlock (){
       mutex.unlock();
    }

    void pushUnlocked(apointer data){
      a_return_if_fail (data);
      queue->pushHead(data);
      if (waitingThreads > 0)
          cond.signal();
    }

    void push(apointer data){
      a_return_if_fail (data);
      mutex.lock();
      pushUnlocked (data);
      mutex.unlock();
    }


    apointer popUnlocked (aboolean wait,aint64 endTime){
      apointer retval;
      if (!queue->peekLastLink() && wait){
          self->waitingThreads++;
          while (!queue->peekLastLink ()){
            if (endTime == -1)
              cond.wait(&mutex);
            else{
              if (!cond.wait(&mutex,endTime))
                 break;
            }
          }
          self->waitingThreads--;
      }

      retval = queue->popLast();
      a_assert (retval || !wait || endTime > 0);
      return retval;
    }

    apointer pop(){
      apointer retval;
      mutex.lock();
      retval = popUnlocked (TRUE, -1);
      mutex.unlock();
      return retval;
    }

    apointer popUnlocked (){
        return popUnlocked(TRUE,-1);
    }


    /**
     * 单位微秒
     */
    apointer pop (auint64 timeout){
      aint64 endTime = Time.monotonic() + timeout;
      apointer retval;
      mutex.lock();
      retval = popUnlocked (TRUE, endTime);
      mutex.unlock();
      return retval;
    }

   apointer popUnlocked(auint64 timeout){
      aint64 endTime = Time.monotonic() + timeout;
      return popUnlocked (TRUE, endTime);
    }

    int length(){
      int retval;
      mutex.lock();
      retval = queue->length() - waitingThreads;
      mutex.unlock();
      return retval;
    }

    int lengthUnlocked(){
        return queue->length() - waitingThreads;
    }

    void sortUnlocked(ACompareDataFunc  func,apointer userData){
      QueueSort sd;
      a_return_if_fail (func != NULL);
      sd.func = func;
      sd.userData = userData;
      queue->sort((ACompareDataFunc)compare_cb,&sd);
    }

    void sort(ACompareDataFunc  func,apointer userData){
      a_return_if_fail (func != NULL);
      mutex.lock();
      sortUnlocked (func, userData);
      mutex.unlock();
    }

    apointer tryPop(){
      apointer retval;
      mutex.lock();
      retval = popUnlocked (FALSE, -1);
      mutex.unlock();
      return retval;
    }

    apointer tryPopUnlocked(){
      apointer retval;
      retval = popUnlocked (FALSE, -1);
      return retval;
    }

    aboolean remove(apointer item){
      aboolean ret;
      a_return_val_if_fail (item != NULL, FALSE);
      mutex.lock();
      ret = removeUnlocked (item);
      mutex.unlock();
      return ret;
    }

    aboolean removeUnlocked(apointer item){
      a_return_val_if_fail (item != NULL, FALSE);
      return queue->remove(item);
    }

    void pushSortedUnlocked(apointer data,ACompareDataFunc func,apointer userData){
        QueueSort sd;
        sd.func = func;
        sd.userData = userData;
        queue->insertSorted(data,compare_cb,&sd);
        if(waitingThreads > 0)
          cond.signal();
    }

    void pushSorted(apointer data,ACompareDataFunc func,apointer userData){
        mutex.lock();
        pushSortedUnlocked(data, func, userData);
        mutex.unlock();
    }

    void pushFront(apointer data){
       a_return_if_fail (data != NULL);
       mutex.lock();
       pushFrontUnlocked (data);
       mutex.unlock();
    }

    void pushFrontUnlocked(apointer data){
      a_return_if_fail (data != NULL);
      queue->push (data);
      if(waitingThreads > 0)
        cond.signal();
    }

    AMutex *getMutex(){
        return &mutex;
    }

   ~AAsyncQueue(){
       if (freeFunc)
           queue->foreach((AFunc)freeFunc, NULL);
       mutex.unref();
       cond.unref();
       queue->unref();
   }


};
