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

#ifndef __AET_UTIL_A_ASYNC_QUEUE_H__
#define __AET_UTIL_A_ASYNC_QUEUE_H__

#include "../../aet.h"
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
    public$ void      pushFront(apointer data);
    public$ void      pushFrontUnlocked(apointer data);
    public$  void     pushUnlocked(apointer data);
    public$ apointer  popUnlocked (aboolean wait,aint64 endTime);
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


