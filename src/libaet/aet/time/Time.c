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
