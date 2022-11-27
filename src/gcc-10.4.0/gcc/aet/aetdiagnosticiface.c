/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "filenames.h"
#include "file-find.h"
#include "simple-object.h"
#include "lto-section-names.h"
#include <dirent.h>

#include "diagnostic.h"
#include "demangle.h"
#include "obstack.h"
#include "intl.h"
#include "version.h"
#include "pretty-print.h"

#define FUNC_NAME "aet_demangle_text"
typedef int (*aet_demangle_text)(const char *start,int length,char *newStr);
static aet_demangle_text aetDemangleFunc=(void*)0;

#define GLOBAL_DEFAULT "GLOBAL DEFAULT"
#define LOCAL_DEFAULT "GLOBAL DEFAULT"


typedef struct _FuncInfo{
    unsigned long address;
    int tag;
}FuncInfo;

static FuncInfo *getAddress(char *add)
{
    char *first=strstr(add,":");
    if(!first)
        return NULL;
    printf("first is:%s\n",first);
    //char *second=first+2; //跳过 冒号和空本;
    char *second=strstr(first," ");
    printf("second is:%s\n",second);
    char *three=second+1;
    printf("three is:%s\n",three);
    char *four=strstr(three," ");
    printf("four is:%s\n",four);
    //char *ret=three+(strlen(three)-strlen(four));
    char *ret= xstrndup(three,(strlen(three)-strlen(four)));
    printf("ret is:%s\n",ret);
    FuncInfo *info=xmalloc(sizeof(FuncInfo));
    info->address=strtol(ret,NULL,16);
    printf("info is ---- %lu\n",info->address);
    if(strstr(add,"GLOBAL DEFAULT"))
        info->tag=0;
    else if(strstr(add,"LOCAL DEFAULT"))
        info->tag=1;
    void *dd=(void*)info->address;
    printf("xx--- %p %p\n",(void*)info->address,dd);
    return info;
}

static int getExecName(char *name,int name_size)
{
    char path[1024]={0};
    int ret = readlink("/proc/self/exe",path,sizeof(path)-1);
    if(ret == -1){
        int pid=getpid();
                char fileName[1024];
                sprintf(fileName,"/proc/%d/exe",pid);
                 ret = readlink(fileName,path,sizeof(path)-1);
                 if(ret==-1){
                     printf("--angian-- get exec name fail!!\n");
                     return -1;
                 }

    }
    path[ret]= '\0';
    return 0;
}

//getFuncAddress  地址:  4096: 0000000000e623e0    26 FUNC    GLOBAL DEFAULT   13 _Z32aet_get_diagnostic_unmangle_textPKc
//getFuncAddress  地址: 22992: 000000000061fde0     1 FUNC    LOCAL  DEFAULT   13 _GLOBAL__sub_I__Z32aet_get_diagnostic_unmangle_textPKc
//getFuncAddress  地址: 35849: 0000000000e623e0    26 FUNC    GLOBAL DEFAULT   13 _Z32aet_get_diagnostic_unmangle_textPKc
static int getFuncAddress(FuncInfo **infos)
{
        //char *cmd="readelf -S /home/sns/workspace/testblock/bin/libapptest.so | grep -P \"(bss|data)\"";
        int pid=getpid();
        char fileName[1024];
        sprintf(fileName,"/proc/%d/exe",pid);
        char cmd[512];
        sprintf(cmd,"readelf -s -W %s | grep %s",fileName,FUNC_NAME);
        FILE *fd = popen(cmd, "r");
        int count=0;
        char tempBuff[1024];
        if(fd){
            while(TRUE){
              char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
              if(ret==NULL){
                 break;
              }
              if(ret!=NULL){
                  FuncInfo *info=getAddress(ret);
                  if(info){
                      infos[count++]=info;
                  }
               }
            }
            fclose(fd);
        }
        return count;
}

static unsigned long getMap(char *map,char *exec)
{
    char *first=strstr(map,exec);
    if(!first)
        return 0 ;
    char *second=strstr(map," ");
    char *three= xstrndup(map,(strlen(map)-strlen(second)));
    char *four=strstr(three,"-");
    char *five= xstrndup(three,(strlen(three)-strlen(four)));
    unsigned long address=strtol(five,NULL,16);
    return address;
}

static int readmaps(unsigned long *values)
{
        //char *cmd="readelf -S /home/sns/workspace/testblock/bin/libapptest.so | grep -P \"(bss|data)\"";
        int count=0;
        char exec[1024];
        getExecName(exec,1024);

        int pid=getpid();
        char fileName[1024];
        sprintf(fileName,"/proc/%d/maps",pid);
        char cmd[512];
        sprintf(cmd,"cat %s",fileName);
        FILE *fd = popen(cmd, "r");
        printf("readmaps is 00 :%s %p\n",cmd,fd);
        char tempBuff[1024];
        if(fd){
            while(TRUE){
              char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
              if(ret==NULL){
                 break;
              }
              if(ret!=NULL){
                  printf("readmaps 找到了变量 is 11 :%s\n",ret);
                  unsigned long rev= getMap(ret,"/cc1");
                  if(rev!=0)
                      values[count++]=rev;
               }
            }
            fclose(fd);
        }
        return count;
}

static void printfAddressInfo( FuncInfo **infos,int cc,unsigned long *values,int len)
{
    int i,j;
    for(i=0;i<len;i++){
        for(j=0;j<cc;j++){
           unsigned long v=values[i]+infos[j]->address;
           void *dd=(void*)v;
           printf("address info i:%d j:%d %p %p %lu\n",i,j,(void*)v,dd,v);
        }
    }
}

/**
 * 从当前可执行文件中找到符号aet_demangle_text的地址。
 * 再从/proc/pid/maps中找出代码段所在的地址。
 * 两个地址相加算出函数aet_demangle_text在进行中的虚拟地址。
 */
static unsigned long getFuncpointerAddress()
{
        FuncInfo *infos[20];
        int count=getFuncAddress(infos);
        char exec[1024];
        getExecName(exec,1024);
        unsigned long values[10];
        int mapsCount=readmaps(values);
        printfAddressInfo(infos,count,values,mapsCount);
}


static char *aetDemangleFuncPointer=NULL;

int pp_demangle_text_by_aet(const char *start,int length,char *newStr)
{
    if(aetDemangleFuncPointer==NULL){
        aetDemangleFuncPointer=getenv("initAddressDiagnosticCallback");
        if(aetDemangleFuncPointer!=NULL){
          unsigned long address=atol(aetDemangleFuncPointer);
          aetDemangleFunc=address;
        }
    }
    if(aetDemangleFunc)
        return aetDemangleFunc(start,length,newStr);
    return 0;
}




