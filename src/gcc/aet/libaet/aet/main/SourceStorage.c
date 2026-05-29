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
