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


