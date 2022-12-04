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

    public$ unsigned long getInterfaceOffset(int index){
       if(interfaceCount==0){
          printf("获取接口在类中的偏移地址出错,类%s没有接口 index:%d\n",name,index);
          exit(0);
          return 0;
       }
       if(index<0 || index>=interfaceCount){
           printf("获取接口在类中的偏移地址出错,类%s实现了%d个接口 index:%d\n",name,interfaceCount,index);
           exit(0);
          return 0;
       }
       return interfacesOffset[index];
    }


    AClass(){
        name=NULL;
        packageName=NULL;
        parent=NULL;
        interfaceCount=0;
    }


};
