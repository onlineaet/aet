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

#ifndef __AET_UTIL_A_THREAD_POOL_H__
#define __AET_UTIL_A_THREAD_POOL_H__

#include "../../aet.h"
#include "AMutex.h"
#include "ACond.h"
#include "AQueue.h"
#include "AAsyncQueue.h"


package$ aet.util;



public$ class$ AThreadPool{
    public$ static void  setMaxUnusedThreads(int maxThread);
    public$ static int   getMaxUnusedThreads();
    public$ static auint getUnusedThreads();
    public$ static void  stopUnusedThreads();
    public$ static void  setMaxIdleTime(auint interval);
    public$ static auint getMaxIdleTime();
    AFunc func;
    char *name;
    apointer userData;
    aboolean exclusive;//独占
    AAsyncQueue *queue;//任务队列
    ACond cond;
    int maxThreads;
    auint numThreads;
    aboolean running;
    aboolean immediate;
    aboolean waiting;
    ACompareDataFunc sortFunc;
    apointer sortUserData;
    public$  AThreadPool(char *name,AFunc func,apointer  userData,int maxThreads,aboolean exclusive,AError  **error);
    private$ apointer waitNewTask();
    private$ void     wakeupAndStopAll();
    public$  aboolean push(apointer data,AError **error);
    public$  void     free(aboolean immediate,aboolean wait_);
    public$  void     setSortFunction(ACompareDataFunc  func,apointer userData);
    public$  auint    getThreadCount();
    public$  auint    getUnprocessed ();
    public$  int      getMaxThreads();
    public$  aboolean setMaxThreads(int max_threads,AError **error);

};




#endif /* __N_MEM_H__ */


