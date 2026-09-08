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
#define INCLUDE_UNIQUE_PTR
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "filenames.h"
#include "file-find.h"
#include "simple-object.h"
#include "lto-section-names.h"
#include <dirent.h>

#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include "simple-object.h"
#include <utime.h>

#include "collect2.h"
#include "collect2-aix.h"
#include "collect-utils.h"
#include "diagnostic.h"
#include "demangle.h"
#include "obstack.h"
#include "intl.h"
#include "version.h"
#include "toplev.h"
#include "aetmicro.h"
#include "nlib.h"

/**
 * 实现collect2.h声明的函数
 * extern char  **aet_collect(int type,const char *prog,char ** ld_argv,const char *atsuffix);
 */
static int    getOuputFile(char *basePath,char **objs,char *match);
static char  *getObjRootPath(char *oFile);
static int    createAetLib(char **appendArgs,char **ld_argv,int argc,int usemtcs);
static void   collectUseLibFile(const char *prog,char **ld_argv,
                  const char *atsuffix,char *objectRootPath,int usemtcs);
static int    getGccInstallPath(char *path);
static char  *compileSingleFile(char *gcc,char *objectRootPath,
                  char *src,char *dest,char *cfile,char **argv,int count);


#define NULL (void*)0
/**
  * 得到当前时间的毫秒数
  */
static inline  nuint64 gettime()
{
   struct timeval tve;
   gettimeofday(&tve,NULL);
   return tve.tv_sec*1000+tve.tv_usec/1000;
}

struct command
{
  const char *prog;     /* program name.  */
  const char **argv;        /* vector of args.  */
};

static int getArgc(char **ld_argv)
{
    int count=0;
    while(ld_argv[count]){
  	  count++;
    }
    return count;
}

/**
 * 字符串转成按delimiter分隔的字符串数组。
 */
static int  gsplit (const char *string,const char *delimiter,char **buffers,int length)
{
   if(!string)
      return 0;
   char *s;
   const char *remainder;
   remainder = string;
   s = strstr (remainder, delimiter);
   int count=0;
   if (s){
      int  delimiter_len = strlen (delimiter);
      while (s){
         int len;
         len = s - remainder;
         char *item = XNEWVEC (char, len + 1);
         memset(item,0,len+1);
         strncpy(item,remainder,len);
         item[len]='\0';
         if(count>=length){
             printf("gsplit时数据数据溢出 length:%d\n",length);
             abort();
         }
         buffers[count++]=item;
         remainder = s + delimiter_len;
         s = strstr (remainder, delimiter);

      }
   }
   if (*string){
      int len=strlen(remainder);
      if(len>0){
         char *item = XNEWVEC (char, len + 1);
         memset(item,0,len+1);
         strncpy(item,remainder,len);
         item[len]='\0';
         buffers[count++]=item;
      }
   }
   return count;
}

/**
 * 获取文件最后的修改时间
 */
static unsigned long long  getLastModified(char *file)
{
   struct stat64 sb;
   unsigned long long rv=0;
   if (stat64(file, &sb) == 0){
      rv = sb.st_mtime;
   }
   return rv;
}

static void getOFileName(char *cFile,char *oFile)
{
   sprintf(oFile,"%s",cFile);
   oFile[strlen(oFile)-1]='o';
}

/**
 * 替换编译参数中的c file 文件
 */
static char **replaceParmByCFileAndOFile(char *compileParm,char *cFile ,char *oFile,int *paramCount)
{
   static char * SEPARATION ="#$%"; //与gcc.c中的一样
   char **items=xmalloc(sizeof(char*)*256);
   int argc=  gsplit (compileParm,SEPARATION,items,256);
   //fprintf(stderr,"replaceParmByCFileAndOFile--00  %d\n",argc);

   if(items[argc-1]==NULL || !strcmp(items[argc-1],"")){
      //printf("从compileParm取出的最后一个参数是空的或长度是0 %s 参数个数:%d\n",items[argc-1],argc);
      argc--;
   }
   int i;
   int replace=0;
   for(i=0;i<argc;i++){
      if(strcmp(items[i],"-c")==0 && strcmp(items[i+1],"-o")==0){
         if(endswith(items[i+2],".o") && endswith(items[i+3],".c")){
            free(items[i+2]);
            items[i+2]=xstrdup(oFile);
            free(items[i+3]);
            items[i+3]=xstrdup(cFile);
            replace=1;
            break;
         }
      }else if(strcmp(items[i],"-o")==0 && strcmp(items[i+2],"-c")==0){
         if(endswith(items[i+1],".o") && endswith(items[i+3],".c")){
            free(items[i+1]);
            items[i+1]=xstrdup(oFile);
            free(items[i+3]);
            items[i+3]=xstrdup(cFile);
            replace=1;
            break;
         }
      }
   }
   if(!replace){
      error("解析参数，编译源文件与输出文件与现有的模式不匹配。1.-c -o xxx.o xxx.c 2.-o xxx.o -c xxx.c\n");
   }
   *paramCount=argc;
   return items;
}

/**
 * 保存的编译参数
 * 1.-c -o xxx.o xxx.c
 * 2.-o xxx.o -c xxx.c
 */
static void createCmdForIfaceCompile(char *cFile,char *oFile,char *compileParm,struct command *cmds,int index)
{
    int argc=0;
    char **items=replaceParmByCFileAndOFile(compileParm,cFile,oFile,&argc);
    char **real_argv = XCNEWVEC (char *, argc+2);
    const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
    int i;
    for(i=0;i<argc;i++){
        argv[i] = xstrdup(items[i]);
    }
   // fprintf(stderr,"createCmdForIfaceCompile-- %d\n",argc);
    argv[argc] = xstrdup("-Dnclcompileyes");
    argv[argc+1] = (char *) 0;
    cmds[index].prog=argv[0];
    cmds[index].argv=argv;
    for(i=0;i<argc;i++)
       free(items[i]);
    free(items);
//    for(i=argc-5;i<argc+2;i++){
//        printf("createCmdForIfaceCompile---eee-- %d %s\n",i,argv[i]);
//    }
}

/**
 * 读文件
 */
static int readFile(char *fileName,char *buffer,int size)
{
   FILE *fp=fopen(fileName,"r");
   if(!fp){
      return 0;
   }
   int rev = fread(buffer, sizeof(char), size, fp);
   fclose(fp);
   if(rev<=0)
      return  0;
   buffer[rev]='\0';
   return rev;
}

/**
 * 获取在basePath目录下的符合包含有字符math的输出.o文件
 * _RandomGenerator_impl_iface.o
 */
static int getOuputFile(char *basePath,char **objs,char *match)
{
   DIR *dir;
   struct dirent *ptr;
   if ((dir=opendir(basePath)) == NULL){
      perror("Open dir error...");
      exit(1);
   }
   int count=0;
   while ((ptr=readdir(dir)) != NULL){
      if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
         continue;
      else if(ptr->d_type == 8) {   ///file
         if(strstr(ptr->d_name,match) && endswith(ptr->d_name,".o")){
            //printf("aetcollect.c getOutputFile 获取指定后缀名的 .o文件 match:%s name:%s/%s\n",match,basePath,ptr->d_name);
            char *ret=xmalloc(strlen(basePath)+strlen(ptr->d_name)+2);
            sprintf(ret,"%s/%s",basePath,ptr->d_name);
            objs[count++]=ret;
         }
      }else if(ptr->d_type == 10)    ///link file
         printf("d_name:%s/%s\n",basePath,ptr->d_name);
      else if(ptr->d_type == 4){//dir
         continue;
      }
   }
   closedir(dir);
   return count;
}

/**
 * 加入节到生成的目标中。原方案是:改elf头中的9-16字节，但加载时出错。
 * 可以快速判断是不是aet文件。
 */
static char *note_aet=R"%%%(
   .section .note.aet, "a", @note
   .align 4
   .long 4              /* namesz */
   .long 16             /* descsz */
   .long 0x01           /* type */
   
   .asciz "AET"
   .align 4
   
   .long  0x61746531    /* magic是 '0x61746531=aet1'*/
   .short 0x0100        /* version */
   .short 0x0001        /* ABI */
   .long 0
   .section .note.GNU-stack,"",@progbits
   )%%%";

static char *mainCode=R"%%%(
   int main()
   {
     return 0;
   }
   )%%%";

static char *compileNote(char *gcc,char *objectRootPath)
{
   char dest[255];
   sprintf(dest,"%s/note_aet.o",objectRootPath);
   if(file_exists(dest))
      return xstrdup(dest);
   char src[255];
   sprintf(src,"%s/note_aet.S",objectRootPath);
   FILE *fp=fopen(src,"w");
   fwrite(note_aet,1,strlen(note_aet),fp);
   fclose(fp);
   //gcc -c aet_note.S -o aet_note.o

   char **real_argv = XCNEWVEC (char *, 6);
   const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
   argv[0] = gcc;
   argv[1] = "-c";
   argv[2] = src;
   argv[3] = "-o";
   argv[4] = dest;
   argv[5] = (char *) 0;
   struct pex_obj *pex;
   pex = collect_execute (gcc, real_argv, NULL, NULL,PEX_LAST | PEX_SEARCH,false, NULL);
   int ret = collect_wait (gcc, pex);
   if (ret){
      error ("gcc returned %d exit status", ret);
      exit (ret);
   }
   return xstrdup(dest);
}

static char *compileMain(char *gcc,char *objectRootPath)
{
   char dest[255];
   sprintf(dest,"%s/temp_main.o",objectRootPath);
   if(file_exists(dest))
      return xstrdup(dest);
   char src[255];
   sprintf(src,"%s/temp_main.c",objectRootPath);
   FILE *fp=fopen(src,"w");
   fwrite(mainCode,1,strlen(mainCode),fp);
   fclose(fp);

   char **real_argv = XCNEWVEC (char *, 6);
   const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
   argv[0] = gcc;
   argv[1] = "-c";
   argv[2] = src;
   argv[3] = "-o";
   argv[4] = dest;
   argv[5] = (char *) 0;
   struct pex_obj *pex;
   pex = collect_execute (gcc, real_argv, NULL, NULL,PEX_LAST | PEX_SEARCH,false, NULL);
   int ret = collect_wait (gcc, pex);
   if (ret){
      error ("gcc returned %d exit status", ret);
      exit (ret);
   }else{
      ;//printf("编译完成了最小的main文件 成功了。\n");
   }
   return xstrdup(dest);
}

//用户链接参数中没有 -noaetinclude
static bool isNoInclude()
{
   char *ok=getenv ("GCC_AET_NO_INCLUDE");
   return ok!=NULL;
}

static char *getObjRootPath(char *oFile)
{
   //排除temp
   if(startswith(oFile,"/temp"))
      return NULL;
   char *temp = strrchr(oFile,'/');
   if(temp==NULL)
      return NULL;
   char len=strlen(oFile)-strlen(temp)+1;
   char ret[len+1];
   memcpy(ret,oFile,len);
   ret[len]='\0';
   char *path= xstrdup(ret);
   return path;
}


#define CHECK_FILE_LIST_NAME_NEW                "aet_iface_check_list.o"
#define IFACE_IMPL_FILE_LIST_NAME_NEW           "aet_iface_impl_list.o"

#define SAVE_LIB_PARM_FILE_NEW                  "aet_collect2_lib_name.o"

#define AET_FUNC_WITH_GB_FILE_LIST_NAME_NEW     "func_with_gb_list_file.o"   //泛型块函数文件殂表
#define GENERIC_MODEL_INDEX_FILE_LIST_NAME_NEW  "generic_model_index.o"  //新建泛型对象，调用泛型函数的文件名列表
#define AET_MTCS_LINK_FILE_LIST_NAME_NEW        "mtcs_link_func_index_file.o" //保存需要链接的mtcs函数文件名


/**
 *
 * 下面代码模拟链接，获取所有库并保存在文件 SAVE_LIB_PARM_FILE (aet_collect2_ld_lib_name.o)
 * 在编译middlefile.c前，打开库文件 SAVE_LIB_PARM_FILE 读取它的内容
 */
static void collectUseLibFile(const char *prog,char **ld_argv,
      const char *atsuffix,char *objectRootPath,int usemtcs)
{
   char *gcc = c_file_name;
   char outFileName[255];
   sprintf(outFileName,"%s/%s",objectRootPath,SAVE_LIB_PARM_FILE_NEW);
   // 1. 首先获取库文件列表
   //printf("collectUseLibFile 00 outFileName:%s objectPath:%s %s\n",outFileName,objectRootPath,getenv ("COLLECT_GCC"));
   int argc = 0;
   // 1. 计算参数个数
   while (ld_argv[argc])
      argc++;
   char *appends[10];
   int appendCount=createAetLib(appends,ld_argv,argc,usemtcs);
  // printf("1. 计算参数个数 :%d prog:%d 加libaet:%d\n",argc,prog,appendCount);
   // 2. 分配新数组 (原参数 + 3: --trace, -o, /dev/null)
   char **real_argv = XCNEWVEC (char *, argc+2+1+appendCount);
   char ** trace_argv = CONST_CAST2 (const char **, char **,real_argv);

   int i;
   int count=0;
   // 3. 复制程序名
   trace_argv[count++] = xstrdup(ld_argv[0]);
   // 4. 添加 --trace
   trace_argv[count++] = xstrdup("--trace");

   for(i=0;i<appendCount;i++)
      trace_argv[count++] = appends[i];

   char *mainobj=compileMain(gcc,objectRootPath);
   trace_argv[count++] = mainobj;

   for(i=1;i<argc;i++){
      char *item=ld_argv[i];
      if (strcmp(item, "-o") == 0) {
         trace_argv[count++]=xstrdup(item);
         trace_argv[count++]=xstrdup("/dev/null");
         i++;
      }else {
         if(startswith(item,objectRootPath) && endswith(item,".o"))
            continue;
         else
            trace_argv[count++]=xstrdup(item);
      }
   }
   // 5. 执行
   struct pex_obj *pex = collect_execute(prog, trace_argv,
         outFileName, "/dev/null",PEX_LAST | PEX_SEARCH,  HAVE_GNU_LD && at_file_supplied, atsuffix);
   collect_wait(prog, pex);
   // 6. 清理
   for (int i = 0; i<argc+2; i++) {
      if(trace_argv[i]!=NULL){
         free(trace_argv[i]);
      }
   }
   free(trace_argv);
   //重要，否则可能报multiple definition of `main';...temp_main.o:temp_main.c
   //因为temp_main.c 可能创建在输出对象的路径里。
   remove(mainobj);
}

static bool haveParam(char *param,char **ld_argv,int argc)
{
   int i;
   for(i=0;i<argc;i++){
      if(strcmp(param,ld_argv[i])==0){
         //用户已经加入库路径了
         return true;
      }
   }
   return false;
}

static bool haveCudaLibPath(char **ld_argv,int argc)
{
   int i;
   for(i=0;i<argc;i++){
      if(startswith(ld_argv[i],"-L")==0){
         if(strstr(ld_argv[i],"cuda"))
            return true;
      }
   }
   return false;
}

/**
 * 如果用户参数中没有 -noaetinclude 自动加入libaet.so libaet_cuda.so到链接器中。
 * 如果用户加了，跳过。
 * libaet.so libaet_cudao.so 安装在gcc-aet的 lib64/目录下。
 */
static int createAetLib(char **appendArgs,char **ld_argv,int argc,int usemtcs)
{
   //用户参数中没有 -noaetinclude，现在加入libaet.so libaet_cuda.so
   bool addLibAet=false;
   bool addLibAetCuda=false;
   if(isNoInclude())
      return 0;
   //在编译aet时也编译libaet,这时 gcc还未安装，并且 gcc是xgcc,所以在编库时不能进入到这里。
   char aetInstallPath[PATH_MAX];
   int ret =getGccInstallPath(aetInstallPath);
   if(ret==0){
      printf("严重错误 不是gcc :%s\n",c_file_name);
      exit(0);
      return 0;
   }

   if(!haveParam("-laet",ld_argv,argc)){
      addLibAet=true;
   }

   if(!haveParam("-laet_cuda",ld_argv,argc)){
      addLibAetCuda=true;
      char fileName[256];
      sprintf(fileName,"%s/lib64/libaet_cuda.so",aetInstallPath);
      if(!file_exists(fileName))
         addLibAetCuda=false;
   }
   if(addLibAetCuda && usemtcs==0)
      addLibAetCuda = false;
   int count=0;
   if(addLibAet || addLibAetCuda){
      char lPath[512];
     // sprintf(lPath,"-L%s/lib64",aetInstallPath);

      sprintf(lPath,"-rpath=%s/lib64",aetInstallPath);
      if(!haveParam(lPath,ld_argv,argc)){
         appendArgs[count++] = xstrdup(lPath);
      }
      if(addLibAet)
         appendArgs[count++] = xstrdup("-laet");
      if(addLibAetCuda){
         appendArgs[count++] = xstrdup("-laet_cuda");
         if(!haveCudaLibPath(ld_argv,argc)){
            if(file_exists("/usr/local/cuda/")){
               appendArgs[count++] = xstrdup("-L/usr/local/cuda/lib64");
            }else if (file_exists("/opt/cuda/")){
               appendArgs[count++] = xstrdup("-L/opt/cuda/lib64");
            }
         }
         if(!haveParam("-lcuda",ld_argv,argc)){
            appendArgs[count++] = xstrdup("-lcuda");
         }
         if(!haveParam("-lcudart",ld_argv,argc)){
            appendArgs[count++] = xstrdup("-lcudart");
         }
         if(!haveParam("-lnvJitLink",ld_argv,argc)){
            appendArgs[count++] = xstrdup("-lnvJitLink");
         }
      }
   }
   return count;
}

static int getGccInstallPath(char *path)
{
   char *exe=c_file_name;
   while(1){
      char *temp = strrchr(exe,'/');
      if(temp){
         int len=strlen(exe)-strlen(temp);
         memcpy(path,exe,len);
         path[len]='\0';
         char file[512];
         sprintf(file, "%s/libexec/gcc", path);
         if(file_exists(file)){
            return 1;
         }
         exe=path;
      }else{
         printf("没找到安装路径\n");
         break;
      }
   }
   return 0;
}

#define ADDITIONAL_MIDDLE_AET_FILE_NEW  "temp_func_track_45.c"


static char * compileSingleFile(char *gcc,char *objectRootPath,char *src,
      char *dest,char *cfile,char **appendArgv,int appendCount)
{
    int fsrc=file_exists(src);
    int fdest=file_exists(dest);
    int action=0;//0 .不编 不加 1.不编但加到lib中，2 编译
    if(!fsrc && fdest){
      //不编译，不加.o到ld中
      action=0;
    }else if(fsrc && !fdest){
      //需要编译 加.o到ld中
      action=2;
    }else if(fsrc && fdest){
      //都存在,比较时间
      unsigned long long st= getLastModified(src);
      unsigned long long dt= getLastModified(dest);
      if(st>dt){
         //要编译 加.o到ld中
         action=2;
      }else{
         //不编译 加.o到ld中
         action=1;
      }
    }else{
      //都不存在,不编译，不加.o到ld中
      action=0;
    }
    //printf("compileSingleFile --- %s %s %d\n",src,dest,action);
    if(action==2){
      unsigned long long second=0;
      unsigned long long ns=0;

      if(cfile){
         //原始文件是.o改为.c，改之前记录最后修改时间
         struct stat64 sb;
         unsigned long long rv=0;
         if (stat64(src, &sb) == 0){
            second = sb.st_mtime;
            ns = sb.st_mtim.tv_nsec;
         }
         int ret=rename(src,cfile);
         if(ret!=0){
            printf("出错 rename :%s %s\n",src,cfile);
         }
      }
      struct pex_obj *pex;
      char **real_argv = XCNEWVEC (char *, 6+appendCount);
      const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
      argv[0] = gcc;
      int i;
      for(i=0;i<appendCount;i++)
         argv[1+i]=appendArgv[i];
      argv[1+i] = "-o";
      argv[2+i] = dest;
      argv[3+i] = "-c";
      argv[4+i] = cfile?cfile:src;
      argv[5+i] = (char *) 0;
      pex = collect_execute (gcc, real_argv, NULL, NULL,PEX_LAST | PEX_SEARCH,false, NULL);
      int ret = collect_wait (gcc, pex);
      if (ret){
         error ("gcc returned %d exit status", ret);
         exit (ret);
      }else{
         printf("编译 %s 成功了!\n",src);
      }
      if(cfile){
         rename(cfile,src);
         struct timeval tv[2];
         struct stat64 sb;
         tv[0].tv_sec = second;
         tv[0].tv_usec = ns/1000;
         tv[1].tv_sec = second;
         tv[1].tv_usec = ns/1000;
         if (utimes(src, tv) < 0){
            printf("出错了 改回时间\n");
         }
      }
    }
    char *addObject=NULL;
    if(action!=0){
      addObject=xstrdup(dest);
    }
    //printf("compileSingleFile action %d src:%s obj:%s\n",action,src,addObject);
    return addObject;
}

/**
 * 打印链接参数
 */
static void printLdArgv(char **ld_argv,char *explain)
{
   int i=0;
   while(ld_argv[i]!=(char*)0){
      printf("输出链接参数 %s:%d %s\n",explain,i,ld_argv[i]);
      i++;
   }
}

////////////////////------------------------------
//中间过程的核心功能 target = -shared 或 -static 或可执行文件
static char *compileMiddleFile(char *gcc,char *objectRootPath,
      char *collectFileList,int target,unsigned long long lastTime)
{
   char src[255];
   sprintf(src,"%s/%s",objectRootPath,ADDITIONAL_MIDDLE_AET_FILE_NEW);
   char dest[255];
   sprintf(dest,"%s/%s",objectRootPath,ADDITIONAL_MIDDLE_AET_FILE_NEW);
   int destLen=strlen(dest);
   dest[destLen-1]='o';

   if(file_exists(dest)){
      unsigned long long st= getLastModified(dest);
      //项目中.o的时间比 temp_func_track_45.o 新，重新编译 temp_func_track_45.o
      if(lastTime<st){
         //printf("最后的.o文件的修改时间小于 temp_func_track_45.o 的时间，不需要编译 temp_func_track_45\n");
         return NULL;
      }
      remove(dest);
   }
   char writeContent[1024];
   int compileType = 0;
   sprintf(writeContent,"%s %d %d\n",RID_AET_GOTO_STR,GOTO_COMPILE_TYPE,compileType);
   FILE *fw=fopen(src,"w");
   fwrite(writeContent,1,strlen(writeContent),fw);
   fclose(fw);

   //写入xxx.collect.o的文件列表到文件
   char file[255];
   sprintf(file,"%s/collect_files.o",objectRootPath);
   FILE *fd=fopen(file,"w");
   fwrite(collectFileList,1,strlen(collectFileList),fd);
   fclose(fd);

   char libparams[255];
   sprintf(libparams,"-Daetlib%s/%s",objectRootPath,SAVE_LIB_PARM_FILE_NEW);
   char collectFile[255];
   sprintf(collectFile,"-Daetcollect%s",file);//在middlefile.c中处理的文件列表。
   char targetTag[20];
   sprintf(targetTag,"-Daettarget%d",target);//在middlefile.c中处理的文件列表。
   char *argv[3];
   argv[0]=libparams;
   argv[1]=collectFile;
   argv[2]=targetTag;
   char *objectFile= compileSingleFile(gcc,objectRootPath,src,dest,NULL,argv,3);
   return objectFile;
}

/**
 * objfiles是在aetcollect.c中根据保存在节aetporg中的o文件创建的全路径名。
 * objfile可能不是全路径名，所以加入 endswith比较。
 */
static char* getParams(char *ofile)
{
   char newfile[256];
   sprintf(newfile,"%s.collect.o",ofile);//来自middlefile.c中的saveFile
   if(!file_exists(newfile))
      sprintf(newfile,"%s.mtcs_collect.o",ofile);
   FILE *fp=fopen(newfile,"r");
   if(!fp)
      return NULL;
   char content[1024];
   int rev = fread(content,1,1024,fp);
   fclose(fp);
   content[rev]='\0';
   char *key="params=";
   char *str=strstr(content,key);
   if(!str){
      printf("严重错误，退出:%s\n",content);
      exit(0);
   }
   str=str+strlen(key);
   char *rex=strstr(str,"\n");
   int len=strlen(str)-strlen(rex);
   char *value=xmalloc(len+1);
   memcpy(value,str,len);
   value[len]='\0';
   return value;
}
/**
 * 编译实现接口的.c文件
 * IFACE_IMPL_FILE_LIST_NAME 记录所有需要编译的接口.c文件
 */
static char **compileIface(char *gcc,char *objectRootPath,int *objCount)
{
   char ifaceCompileFileName[256];
   sprintf(ifaceCompileFileName,"%s/%s",objectRootPath,IFACE_IMPL_FILE_LIST_NAME);
   char buffer[1024*10];
   buffer[0]='\0';//必须加否则buffer内存不可知
   readFile(ifaceCompileFileName,buffer,1024*10);

   //headImplCFiles 存放的内容在由ifaceimpl.c中createCFileSource_new创建的
   //比如 /home/sns/workspace/ai/pc-build/debug/_RandomGenerator_2962277235__impl_iface.c\
   //$#@/home/sns/workspace/ai/pc-build/debug/ai0.o
   char *headImplCFiles[200];
   int count=  gsplit (buffer,"\n",headImplCFiles,200);
   struct command *commands;  /* each command buffer with above info.  */
   commands = (struct command *) alloca (count * sizeof (struct command));
   int   n_commands=0;
   int i;
   for(i=0;i<count;i++){
      if(strlen(headImplCFiles[i])==0)
         continue;
      char *dependOFile = strstr(headImplCFiles[i],"$#@");
      //依赖的对象文件
      dependOFile=dependOFile+strlen("$#@");
      char temp[512];
      int len=strlen(headImplCFiles[i])-strlen(dependOFile)-strlen("$#@");
      memcpy(temp,headImplCFiles[i],len);
      temp[len]='\0';
      char *cFile=temp;
      char oFile[512];
      char *compileParm = getParams(dependOFile);
      getOFileName(cFile,oFile);
      createCmdForIfaceCompile(cFile,oFile,compileParm,commands,n_commands++);
   }

   struct pex_obj *pexes[n_commands];
   for(i=0;i<n_commands;i++){
      pexes[i]=pex_init (0,gcc,NULL);
      if (pexes[i] == NULL){
         remove(ifaceCompileFileName);
         fatal_error (input_location, "%<pex_init%> failed: %m");
      }
   }

   for (i = 0; i < n_commands; i++){
      const char *errmsg;
      int err;
      const char *string = commands[i].argv[0];
      errmsg = pex_run (pexes[i], PEX_LAST | PEX_SEARCH,string,
            CONST_CAST (char **, commands[i].argv),NULL, NULL, &err);
      if (errmsg != NULL){
         remove(ifaceCompileFileName);
         errno = err;
         fatal_error (input_location,err ? "cannot execute %qs: %s: %m": "cannot execute %qs: %s",string, errmsg);
      }
   }
   int ok=-1;
   for (i = 0; i < n_commands; i++){
      int status=0;
      if (!pex_get_status (pexes[i], 1, &status)){
         remove(ifaceCompileFileName);
         fatal_error (input_location, "failed to get exit status: %m");
      }
      pex_free (pexes[i]);
      if(status==0){
         printf("编译接口文件成功---%s\n",headImplCFiles[i]);
      }else{
         printf("编译接口文件失败---%s status:%d\n",headImplCFiles[i],status);
         ok=i;
      }
   }
   if(ok>=0)
      fatal_error (input_location, "编译接口文件失败---:%qs",headImplCFiles[ok]);

   char suffix[256];
   sprintf(suffix,"%s.o",IFACE_FILE_SUFFIX);
   char **objects=(char **)xmalloc(sizeof(char *)*100);
   *objCount=getOuputFile(objectRootPath,objects,suffix);
   return objects;
}

/**
 * blockFileName 泛型块.c文件 不是直接编译，是在编译srcFile时加入到cpp_buffer最后
 * srcFile 源.c文件
 * oFile 源.c文件的输出文件
 * srcFile对应的编译参数存在文件 oFile+parm.o文件中。在genericinfo.c generic_info_save中写入参数
 */
static void secondCompileGeneric(char *blockFileName,char *srcFile,char *oFile,
      char *params,struct command *cmds,int index)
{
   static char * SEPARATION ="#$%"; //与gcc.c中的一样
   char *items[1024];
   int argc=  gsplit (params,SEPARATION,items,1024);
   if(items[argc-1]==NULL || !strcmp(items[argc-1],"")){
      printf("取出的最后一个参数是空的或长度是0 %s 参数个数:%d\n",srcFile,items[argc-1],argc);
      argc--;
   }
   char **real_argv = XCNEWVEC (char *, argc+2);
   const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
   int i;
   for(i=0;i<argc;i++){
      argv[i] = items[i];
   }
   gcc_assert(strcmp(items[argc-1],srcFile)==0);

   char dcl[512];
   sprintf(dcl,"-Dnclcompilefile%s",blockFileName);
   argv[argc] = xstrdup(dcl);
   argv[argc+1] = (char *) 0;
   cmds[index].prog=items[0];
   cmds[index].argv=argv;
   //for(i=0;i<argc;i++)
     // printf("secondCompileGeneric data i:%d %s\n",i,argv[i]);
}

/**
 * 编译泛型有关的文件
 * fileName的内容如下：用逗号分开
 * .../_block_func__0.c,../ai0.c,.../ai0.o
 * 原_block_func__0.c不再单独编译，_block_func__0.c也被改名为_block_func__0.o
 */
static void compileGeneric(char *gcc,char *objectRootPath)
{
   //与genericcode.c中的generic_code_create_block_codes
   //创建保存块信息的文件名方法一样，在blockListFileName追加.o

   char fileName[512];
   //这是与aetcollect的协议 在ifaceimpl.c中也有类似
   sprintf(fileName,"%s/%s",objectRootPath,AET_GENERIC_BLOCK_FWGB_FILE_LIST);
   if(!file_exists(fileName)){
      printf("compileGeneric 第二次编译的文件不存在 %s\n",fileName);
      return NULL;
   }
   char fileList[1024*10];
   fileList[0]='\0';//必须加否则buffer内存不可知
   readFile(fileName,fileList,1024*10);

   char parmContent[1024*10];
   parmContent[0]='\0';//必须加否则buffer内存不可知

   char *cFiles[100];
   char *oFiles[100];
   int compileFileCount= gsplit (fileList,"\n",cFiles,100);
   struct command *commands;  /* each command buffer with above info.  */
   commands = (struct command *) alloca (compileFileCount * sizeof (struct command));
   int n_commands =0;      /* # of command.  */
   int i;
   for(i=0;i<compileFileCount;i++){
      char *fileName=cFiles[i];
      //有逗号说明要编译的文件由源文件来编，逗号来自middlefile.c的方法 createCompileUnitFile
      if(strstr(fileName,",")){
         char *items[3];
         int length= gsplit (fileName,",",items,3);
         //0 泛型块文件的文件名 1 源文件 2 源文件对应的输出o文件
         gcc_assert(length==3);
         int ret=remove((const char *)items[2]); //移走源文件的输出o文件
         char *params = getParams(items[2]);
         printf("compileGeneric 00 第二次编译的泛型文件:\n1.源文件:%s\n2.依赖的泛型块文件:\
               %s\n3.输出文件:%s\n移走.o文件ok:%d\n编译参数据：%s\n",
               items[1],items[0],items[2],ret,params);
         secondCompileGeneric(items[0],items[1],items[2],params,commands,n_commands++);
         oFiles[i]=items[2];
         free(params);
      }
   }

   struct pex_obj *pexes[n_commands];
   for(i=0;i<n_commands;i++){
      pexes[i]=pex_init (0,gcc,NULL);
      if (pexes[i] == NULL)
         fatal_error (input_location, "%<pex_init%> failed: %m");
   }

   for (i = 0; i < n_commands; i++){
      const char *errmsg;
      int err;
      const char *string = commands[i].argv[0];
      errmsg = pex_run (pexes[i], PEX_LAST | PEX_SEARCH,string,
            CONST_CAST (char **, commands[i].argv),NULL, NULL, &err);
      if (errmsg != NULL){
         errno = err;
         fatal_error (input_location,err ? "cannot execute %qs: %s: %m": "cannot execute %qs: %s",string, errmsg);
      }
   }

   int ok=-1;
   for (i = 0; i < n_commands; i++){
      int status=0;
      if (!pex_get_status (pexes[i], 1, &status)){
         fatal_error (input_location, "failed to get exit status: %m");
      }
      pex_free (pexes[i]);
      if(status==0){
         printf("编译泛型文件第二次成功%s pid:%d\n",oFiles[i],getpid());
      }else{
         printf("编译泛型文件第二次失败%s status:%d\n",oFiles[i],status);
         ok=i;
      }
   }
   if(ok>=0){
      fatal_error (input_location, "编译泛型文件第二次失败:%qs",oFiles[ok]);
   }
}

/**
 * 编译链接函数所在的文件,返回.o对象文件
 */
static char *compileLinkLibDevice(char *gcc,char *objectRootPath)
{
   //与mtcs_link_link生成的相同
   char compileFileName[255];
   sprintf(compileFileName,"%s/%s",objectRootPath,AET_MTCS_LINK_FILE_LIST_NAME);

   char src[512];
   sprintf(src,"%s",compileFileName);
   char dest[255];
   sprintf(dest,"%s.o",compileFileName);
   char cfile[255];
   sprintf(cfile,"%s",compileFileName);
   int len=strlen(cfile);
   cfile[len-1]='c';
   char *objectFile= compileSingleFile(gcc,objectRootPath,src,dest,cfile,NULL,0);
   return objectFile;
}

/**
 * 加新的.o到最终目标
 * 两类文件的.o要加入到链结器中。
 * 1.temp_func_track_45.c 生成的.o文件
 * 2.接口的实现文件。
 */
static char **createNewArgv(char **ld_argv,char *middleFileObj,char *mtcsLinkObj,
      int ifaceObjectCount,char **ifaceObjects,char *noteAet,int usemtcs)
{
    int argc= getArgc(ld_argv);
    int total=argc;
    int i;
    if(middleFileObj!=NULL)
        total+=1;
    if(mtcsLinkObj!=NULL)
        total+=1;
    total+=ifaceObjectCount;
    char *appAetLibs[10];
    int appAetLibCount=createAetLib(appAetLibs,ld_argv,argc,usemtcs);
    total+=appAetLibCount;
    if(noteAet!=NULL)
       total+=1;//加入noteAet;
    total+=1;//放NULL

    char **real_argv = XCNEWVEC (char *, total);
    const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
    int count=0;
    for(i=0;i<argc;i++)
      argv[count++]=ld_argv[i];
    if(middleFileObj)
      argv[count++]=middleFileObj;
    if(mtcsLinkObj)
      argv[count++]=mtcsLinkObj;
    for(i=0;i<ifaceObjectCount;i++)
        argv[count++]=ifaceObjects[i];
    //加入缺省的库libaet.so和libaet_cudao.so
    for(i=0;i<appAetLibCount;i++)
       argv[count++]=appAetLibs[i];
    if(noteAet)
       argv[count++]=noteAet;
    argv[count++]=(char*)0;
    return argv;
}

/**
 * 目标文件是不是so文件
 */
static int isSoOrStaticTarget()
{
   char *p = getenv ("COLLECT_GCC_OPTIONS");
   if(!p)
      return 0;
   if(strstr(p,"-shared")){
      return 1;
   }
   if(strstr(p,"--static")){
      return 2;
   }
   return 0;
}

/**
 * 被collect2.c的do_link调用
 * 判断是不是编译aet
 * 写入库信息到文件
 * 编译类方法检查
 * 第二次编译泛型相关的文件
 * 在原链接参数中加入新编译的.o文件。
 */
char **aet_collect(const char *prog,char **ld_argv,const char *atsuffix)
{
   //printLdArgv(ld_argv,"之前");
   int count=0;
   int i=0;
   char *buffer=xmalloc(1000*255);
   unsigned long long maxtime = 0;
   char *objectRootPath = NULL;
   bool useMtcs = false;
   while(ld_argv[i]!=(char*)0){
      char *arg=ld_argv[i];
      if(endswith(arg,".o")){
         char newfile[256];
         sprintf(newfile,"%s.collect.o",arg);//来自middlefile.c中的saveFile
         bool haveCollect=false;
         if(!file_exists(newfile)){
            sprintf(newfile,"%s.mtcs_collect.o",arg);
            if(file_exists(newfile)){
               haveCollect = true;
               useMtcs = true;
            }
         }else{
            haveCollect = true;
         }
         if(haveCollect){
            strcat(buffer,newfile);
            strcat(buffer,"\n");
            unsigned long long lasttime =  getLastModified(newfile);
            if(lasttime>maxtime)
               maxtime = lasttime;
            if(!objectRootPath)
               objectRootPath =  getObjRootPath(arg);
            count++;
         }
      }
      i++;
   }
   if (count > 0){
      int target = isSoOrStaticTarget();
      //取第一个.o的路径作为对象路径。
      if(objectRootPath==NULL)
         objectRootPath=xstrdup("/temp/");
      char *gcc=c_file_name;
      //1.生成所需要的库文件
      collectUseLibFile(prog,ld_argv, atsuffix,objectRootPath,useMtcs);
      //2.编译temp_func_track_45.c
      char *middleObj = compileMiddleFile (gcc,objectRootPath,buffer,target,maxtime);
      //3.编译接口实现文件
      int ifaceImplCount = 0;
      char **ifaceimplsObjs= compileIface(gcc,objectRootPath,&ifaceImplCount);
      //4.泛型块和fwgb
      compileGeneric(gcc,objectRootPath);
      //编译4.mtcslink
      char *mtcsLinkObj=compileLinkLibDevice(gcc,objectRootPath);
      //编译5.生成section的新内容
      char *noteAet=compileNote(gcc,objectRootPath);
      //最后一步是生成新的链接参数列表。
      char **aetargv=createNewArgv(ld_argv,middleObj,mtcsLinkObj,
               ifaceImplCount,ifaceimplsObjs,noteAet,useMtcs);
      //把 temp_func_track_45.o改成最新的。下次make，如果没改变.o的时间就不用再编译了
      if(file_exists(middleObj)){
         utime(middleObj,NULL);
      }
      free(buffer);
      return aetargv;
   }
   free(buffer);
   //printLdArgv(ld_argv,"之后");
   return ld_argv;
}
