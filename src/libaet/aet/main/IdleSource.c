#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "IdleSource.h"




impl$ IdleSource{

    IdleSource(char *name){
        super$(name);
        setPriority(Priority.DEFAULT_IDLE);
    }

    IdleSource(EventSourceFuncs *funcs){
        super$(funcs);
        setPriority(Priority.DEFAULT_IDLE);
        setName("IdleSource");
    }

    IdleSource(){
        self("IdleSource");
    }

    int getFd(){
        return -1;
    }

    int getEvents(){
        return -1;
    }


    aboolean  prepare(int *timeout){
        *timeout = 0;
        return TRUE;
    }

    aboolean check(){
        return TRUE;
    }

    aboolean  dispatch(){
         aboolean again;
         if (!callback){
             a_warning ("IdleSource 没有回调函数。");
             return FALSE;
         }
         again = callback (callbackUserData);
         return again;
    }

};


