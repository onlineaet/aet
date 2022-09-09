#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <string.h>
#include <fcntl.h>
#include <execinfo.h>

#include "../io/AFile.h"
#include "AString.h"
#include "ABackTrace.h"

static  ABackTrace *trace=new$ ABackTrace();


/* 打印调用栈的最大深度 */
#define DUMP_STACK_DEPTH_MAX 32

typedef struct _BtSymb{
   char *fileName;
   char *absAdd;//绝对地址
   char *relativeAdd;//相对地址
}BtSymb;


/**
 * 获得可执行的文件名
 */
static char *getExec(){
     char *path = "/proc/self/exe";
     size_t linksize = 1024;
     char   exeName[1024];
     char *fileName=NULL;
     int len=readlink(path,exeName, linksize);
     if(len>0){
        exeName[len]='\0';
        fileName=a_strdup(exeName);
     }else{
        fileName=a_strdup("未知的进程名");
     }
     printf("Application File  name = %s\n",fileName );
     len=strlen(fileName);
     if(fileName[len-1]=='\n' || fileName[len-1]==' ')
         fileName[len-1]='\0';
     AString *str=new$ AString(fileName);
     str->trim();
     return str->unrefStr();
}

static void callbackSignal_cb (int signal){
    char *processName=getExec();
    printf("\n段错误: pid:%lu %s\n",getpid(),processName);
    a_log_print_stack();
    exit(0);
}

void a_log_print_stack()
{
    trace->backtrace();
}

/**
 * backtrace_symbols获取得字符串格式：
 * 文件+(+相对地址+)+" "+[+绝对地址+]
 * /usr/lib/x86_64-linux-gnu/libc.so.6(+0x430c0) [0x7f89592a70c0]
 */
impl$ ABackTrace{

    ABackTrace(){
        mapsAddress=NULL;
        signal(SIGSEGV, callbackSignal_cb);
    }
    /**
     * 解析字符串"/usr/lib/x86_64-linux-gnu/libc.so.6(+0x430c0) [0x7f89592a70c0]"
     */
    BtSymb *createBtSymb(char *stackStr){
        AString *str=new$ AString(stackStr);
        int end=str->lastIndexOf("]");
        if(end<0)
            goto out;

        int start=str->lastIndexOf("[");
        if(start<0)
            goto out;
        AString *str1=str->substring(start+1,end);//取得绝对地址
        end=str->lastIndexOf(")");
        if(end<0){
           str1->unref();
           goto out;
        }
        start=str->lastIndexOf("+");
        AString *str2=NULL;
        if(start>=0){
            str2=str->substring(start+1,end);//相对地址
        }
        end=str->lastIndexOf("(");
        AString *str3=str->substring(0,end);//文件名
        BtSymb *item=a_slice_new(BtSymb);
        item->fileName=str3->unrefStr();
        item->absAdd=str1->unrefStr();
        item->relativeAdd=str2?str2->unrefStr():NULL;
        str->unref();
        return item;
      out:
         str->unref();
         return NULL;
    }

    static int readContent(char *fileName,char *add,char **strs){
        if(add==NULL)
            return 0;
        char cmd[1024];
        sprintf(cmd,"addr2line %s -e %s -f -C",add,fileName);
        FILE *fd = popen(cmd, "r");
        //printf("readContent is 00 :%s\n",cmd);
        char tempBuff[1024];
        int dataLen=0;
        int count=0;
        if(fd){
            while(TRUE){
              char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
              if(ret==NULL){
                 break;
              }
              if(ret!=NULL){
                  //printf("readContent bbbb 找到了变量 is 11 :%s\n",ret);
                  strs[count++]=a_strdup(ret);
              }
            }
            fclose(fd);
        }
        return count;
    }

    static char *getMaps(){
        char cmd[64] = {0x00};
        sprintf(cmd,"cat /proc/%d/maps", getpid());
        FILE *fd = popen(cmd, "r");
        char tempBuff[1024];
        int count=0;
        AString *strs=new$ AString("");
        if(fd){
           while(TRUE){
             char *ret=fgets(tempBuff, sizeof(tempBuff), fd); //将刚刚FILE* stream的数据流读取到buf中
             if(ret==NULL){
                break;
             }
             if(ret!=NULL){
                 strs->append(ret);
             }
           }
           fclose(fd);
        }
        return strs->unrefStr();
    }

    static void createMapsAddress(char *maps){
        char **values=a_strsplit (maps,"\n",-1);
        auint len=a_strv_length(values);
        int i;
        if(mapsAddress!=NULL){
            a_free(mapsAddress);
            mapsAddress=NULL;
            self->len=0;
        }
        mapsAddress=a_malloc0(sizeof(auint64)*len*2);
        self->len=len;
        for(i=0;i<len;i++){
            char *str=values[i];
            if(!str || strlen(str)==0)
                continue;
            char *before=strstr(str,"-")+1;
            char *second=strstr(before," ")+1;
            int pos=strlen(before)-strlen(second);
            int pos1=strlen(str)-strlen(before);
            before[pos]='\0';
            unsigned long long max=strtoull(before,NULL,16);
            maps[pos1-1]='\0';
            unsigned long long min=strtoull(str,NULL,16);
            //printf("maps address i:%d %llu %llx %llu %llx\n",i,min,min,max,max);
            mapsAddress[i*2]=min;
            mapsAddress[i*2+1]=max;
        }
    }

    void fillExecFile(BtSymb *symb,AString *result,int index){
         char *infosOne[3];
         int count1=readContent(symb->fileName,symb->absAdd,infosOne);
         char *infosTwo[3];
         int count2=readContent(symb->fileName,symb->relativeAdd,infosTwo);
         if(count1==0 && count2==0){
             result->appendPrintf("%02d  %s",index+1,symb->absAdd);
             result->appendPrintf("\t ??\n");
         }else if(count1!=0 && count2==0){
             result->appendPrintf("%02d  %s  %s",index+1,symb->absAdd,infosOne[0]);
             result->appendPrintf("\t %s",infosOne[1]);
             a_free(infosOne[0]);
             a_free(infosOne[1]);
         }else if(count1==0 && count2!=0){
             result->appendPrintf("%02d  %s  %s",index+1,symb->absAdd,infosTwo[0]);
             result->appendPrintf("\t %s",infosTwo[1]);
             a_free(infosTwo[0]);
             a_free(infosTwo[1]);
         }else{
             int i;
             int compare1=0;
             for(i=0;i<count1;i++){
                 char *tt=infosOne[i];
                 if(strstr(tt,"?"))
                     compare1++;
             }
             int compare2=0;
             for(i=0;i<count2;i++){
               char *tt=infosTwo[i];
               if(strstr(tt,"?"))
                   compare2++;
             }
             char *func=infosOne[0];
             char *line=infosOne[1];
             char *add=symb->absAdd;
             if(compare2<compare1){
                 func=infosTwo[0];
                 line=infosTwo[1];
                 add=symb->relativeAdd;
             }
             result->appendPrintf("%02d  %s  %s",index+1,add,func);
             if(strstr(line,"?")){
                 char *lc=strstr(line,":");
                 if(lc!=NULL)
                    result->appendPrintf("\t %s%s",symb->fileName,lc);
                 else
                    result->appendPrintf("\t %s:?\n",symb->fileName);
             }else{
                 result->appendPrintf("\t %s",line);
             }
             a_free(infosOne[0]);
             a_free(infosOne[1]);
             a_free(infosTwo[0]);
             a_free(infosTwo[1]);
         }
    }

    aint64 getStartAdd(char *addStr){
        int i;
        unsigned long long add=strtoull(addStr,NULL,16);
        for(i=0;i<self->len;i++){
            if(add>=mapsAddress[i*2] && add<=mapsAddress[i*2+1]){
                //printf("maps address i:%d %llu %llx %llu %llx\n",i,mapsAddress[i*2],mapsAddress[i*2],mapsAddress[i*2+1],mapsAddress[i*2+1]);
                return add-mapsAddress[i*2];
            }
        }
        return -1;
    }

    static void format(BtSymb *symb,char *execFile,char *maps,AString *result,int index,char *stackString){
          AFile *f=new$ AFile(symb->fileName);
          AFile *canonical=f->getCanonicalFile();
          char *file=canonical->getAbsolutePath();
          //printf("格式化 stackString 。%s stackString:%s execFile:%s %d %d\n",file,stackString,execFile,strlen(file),strlen(execFile));
          if(strcmp(file,execFile)){
              aint64 add= getStartAdd(symb->absAdd);
              if(add<0){
                  result->appendPrintf("%02d  %s\n",index+1,stackString);
              }else{
                  char addStr[128];
                  sprintf(addStr,"0x%llx",add);
                  char *infosOne[3];
                  int count= readContent(symb->fileName,addStr,infosOne);
                  if(count>0){
                      result->appendPrintf("%02d  %s  %s",index+1,symb->absAdd,infosOne[0]);
                      if(strstr(infosOne[1],"?"))
                        result->appendPrintf("\t %s:?\n",symb->fileName);
                      else
                        result->appendPrintf("\t %s",infosOne[1]);
                      a_free(infosOne[0]);
                      a_free(infosOne[1]);
                  }else{
                      result->appendPrintf("%02d  %s\n",index+1,stackString);
                  }
              }
          }else{
             fillExecFile(symb,result,index);
          }
          f->unref();
          canonical->unref();
    }

    /* 打印调用栈函数 */
    void backtrace(){
       a_log_pause();
       void *stack_trace[DUMP_STACK_DEPTH_MAX] = {0};
       char **stack_strings = NULL;
       int stack_depth = 0;
       int i = 0;
       /* 获取栈中各层调用函数地址 */
       stack_depth = backtrace(stack_trace, DUMP_STACK_DEPTH_MAX);
       /* 查找符号表将函数调用地址转换为函数名称 */
       stack_strings = (char **)backtrace_symbols(stack_trace, stack_depth);
       if (NULL == stack_strings) {
           printf("打印栈中函数调用时，没有足够的内存。");
           exit(1);
       }
       char *exeFileName=getExec();//"/home/sns/workspace/test/Debug/test";
       if(exeFileName==NULL){
           printf("打印栈中函数调用时，没有找到可执行文件名。");
           exit(1);
       }
       AString *result=new$ AString("");
       char *maps=getMaps();
       createMapsAddress(maps);
       a_free(maps);
        /* 打印调用栈 */
        for (i = 0; i < stack_depth; ++i) {
            char *stack=stack_strings[i];
            BtSymb *symb=createBtSymb(stack);
            if(!symb){
                printf("不能转成函数名文件名还有地址。%s\n",stack);
                result->appendPrintf("%02d  %s\n",i+1,stack);
            }else{
                //printf("转成文件名 相对地址 绝对地址 %s %s %s\n",symb->fileName,symb->relativeAdd,symb->absAdd);
                format(symb,exeFileName,maps,result,i,stack);
            }
            if(symb->fileName)
                a_free(symb->fileName);
            if(symb->absAdd)
                a_free(symb->absAdd);
            if(symb->relativeAdd)
                a_free(symb->relativeAdd);
            a_slice_free(BtSymb,symb);
        }
        /* 获取函数名称时申请的内存需要自行释放 */
        free(stack_strings);
        stack_strings = NULL;
        printf("栈信息:\n%s\n",result->getString());
        a_log_resume();
    }
};



