#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "DefaultSource.h"

impl$ DefaultSource{

    DefaultSource(char *name){
        super$(name);
    }

    DefaultSource(EventSourceFuncs *funcs){
        super$(funcs);
    }

    int getFd(){
        return -1;
    }

    int getEvents(){
        return -1;
    }

    aboolean  prepare(int *timeout){
        if(funcs &&  funcs->prepare)
            return funcs->prepare(self,timeout);
      return FALSE;
    }

    aboolean  check(){
        if(funcs && funcs->check)
            return funcs->check(self);
        return FALSE;

    }

    aboolean  dispatch(){
        if(funcs && funcs->dispatch)
            return funcs->dispatch(self);
        return TRUE;
    }

};


