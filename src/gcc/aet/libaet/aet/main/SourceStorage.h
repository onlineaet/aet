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

#ifndef __AET_MAIN_SOURCE_STORAGE_H__
#define __AET_MAIN_SOURCE_STORAGE_H__

#include "../../aet.h"
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

