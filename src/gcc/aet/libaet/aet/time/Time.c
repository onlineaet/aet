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

#include <sys/time.h>
#include <time.h>
#include "Time.h"


impl$ Time{

    /**
     * 取墙上时间。
     * 从UTC 1970-01-01开始的微秒数
     */
    static aint64 currentTime(){
        struct timeval val;
        gettimeofday (&val, NULL);
        return (((aint64) val.tv_sec) * 1000000) + val.tv_usec;
    }

    /**
     * 单调时间
     * 返回微秒
     */
    static aint64 monotonic(void){
       struct timespec ts;
       int result;
       result = clock_gettime (CLOCK_MONOTONIC, &ts);
       if A_UNLIKELY (result != 0)
          a_error ("不支持CLOCK_MONOTONIC");
       return (((aint64) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
    }
};
