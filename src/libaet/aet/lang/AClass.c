#include "AObject.h"

impl$ AClass{

    char *getName(){
       return name;
    }

    char *getPackage(){
        return packageName;
    }

    AClass *getParent(){
       return parent;
    }

    AClass **getInterfaces(int *count){
        if(interfaceCount==0)
            return NULL;
        *count=interfaceCount;
        return  interfaces;
    }


    AClass(){
        name=NULL;
        packageName=NULL;
        parent=NULL;
        interfaceCount=0;
    }


};
