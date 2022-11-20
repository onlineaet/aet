#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include "System.h"

/**
 * objectSize=0;由编译器来设大小
 */
impl$ System{

    /**
     * 取墙上时间。
     * 从UTC 1970-01-01开始的微秒数
     */
    static aint64 currentTime(){
        struct timeval val;
        gettimeofday (&val, NULL);
        return (((aint64) val.tv_sec) * 1000000) + val.tv_usec;
    }

    static int getCpuCores(){
        if(CPU_CORES>0)
            return CPU_CORES;
        int ret;
        int len = 1024;   //cpu信息最多10个字节
        char buffer[len];
        int fd = open("/proc/cpuinfo", O_RDONLY);
        if(fd<0){
            return -1;
        }
        ret = read(fd, buffer, len);
        close(fd);
        if(ret < 0){
            printf("Read error\n");
            return -1;
        }
        buffer[ret] = '\0';
        char *str1=strstr(buffer,"cpu cores");
        if(str1){
            char *str2=strstr(str1,":");
            if(str2){
               char *str3=  strtok(str2+1, "\n");
               CPU_CORES=atoi(str3);
               //printf("xxx iss %s %d\n",str3,atoi(str3));
            }

        }
        return CPU_CORES;
    }

    static int getCpuThreads(){
        if(CPU_THREADS>0)
                 return CPU_THREADS;
        CPU_THREADS= sysconf(_SC_NPROCESSORS_CONF);
        return CPU_THREADS;
    }



};
