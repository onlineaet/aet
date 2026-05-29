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

#ifndef __AET_MAIN_ARRAY_STORAGE_H__
#define __AET_MAIN_ARRAY_STORAGE_H__

#include "../../aet.h"
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

