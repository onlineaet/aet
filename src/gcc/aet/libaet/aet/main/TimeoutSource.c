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
#include <errno.h>
#include "../time/Time.h"

#include "TimeoutSource.h"

impl$ TimeoutSource{

    void setExpiration (aint64 currentTime){
      aint64 expiration = currentTime + (auint64) interval * 1000;//转成微秒
      setReadyTime (expiration);
    }


    TimeoutSource(auint interval){
        self->interval = interval;
        setExpiration (Time.monotonic());
    }


    int getFd(){
        return -1;
    }

    int getEvents(){
        return -1;
    }

    aboolean  prepare(int *timeout){
      return FALSE;
    }

    aboolean  check(){
        return FALSE;

    }

    aboolean  dispatch(){
        aboolean again;
        if (!callback){
              a_warning ("TimeoutSource 没有回调函数。");
              return FALSE;
        }
        again = callback (callbackUserData);
        if (again)
            setExpiration (getTime());
        return again;
    }

};


