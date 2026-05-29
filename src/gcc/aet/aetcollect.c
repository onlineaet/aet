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
static void   getCompileType(char *content,int *types);
static char  *getObjRootPath(char *oFile);
static int    createAetLib_new(char **appendArgs,char **ld_argv,int argc,int usemtcs);
static void   collectUseLibFile_new(const char *prog,char **ld_argv,
                  const char *atsuffix,char *objectRootPath,int usemtcs);
static int    getGccInstallPath(char *path);
static char  *compileSingleFile_new(char *gcc,char *objectRootPath,
                  char *src,char *dest,char *cfile,char **argv,int count);
static char  *compileMiddleFile_new(char *gcc,char *objectRootPath,int compileType,char **argv,int argc);
/**
 * 编译泛型有关的文件
 */
static char **compileGeneric_new(char *gcc,char *objectRootPath,char *blockListFileName,int *objCount,
      char **objfiles,char **objcontents,int ofileCount);

static char **compileIface_new(char *gcc,char *objectRootPath,int *objCount,char *implFileList,
      char **objfiles,char **objcontents,int ofileCount);

static char *compileLinkLibDevice_new(char *gcc,char *objectRootPath,char *mtcsLinkFile);

static char **createNewArgv_new(char **ld_argv,char *middleFileObj,char *mtcsLinkObj,
      int ifaceObjectCount,char **ifaceObjects,int genCount,char **genObjs,char *noteAet,int usemtcs);

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
           // printf("aetcollect.c getOutputFile 获取指定后缀名的 .o文件 match:%s name:%s/%s\n",match,basePath,ptr->d_name);
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

static void compileBlockFunc(char *cFile,char *oFile,char *compileParm,struct command *cmds,int index)
{
    static char * SEPARATION ="#$%"; //与gcc.c中的一样
    char *items[1024];
    int argc=  gsplit (compileParm,SEPARATION,items,1024);
    if(items[argc-1]==NULL || !strcmp(items[argc-1],"")){
        //printf("从compileParm取出的最后一个参数是空的或长度是0 %s 参数个数:%d\n",items[argc-1],argc);
        argc--;
    }
    char **real_argv = XCNEWVEC (char *, argc+2);
    const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
    int i;
    for(i=0;i<argc;i++){
        argv[i] = items[i];
    }
    argv[argc-3] =xstrdup(oFile);//这里可能有问题 跳过-c参数， 如果没有-c,赋值是错的。
    argv[argc-1] =xstrdup(cFile);
    argv[argc] = xstrdup("-Dnclcompileyes");
    argv[argc+1] = (char *) 0;

    cmds[index].prog=items[0];
    cmds[index].argv=argv;
    //for(i=0;i<argc;i++)
      // printf("compileBlockFunc data i:%d %s\n",i,argv[i]);
}

/**
 * 加入节到生成的目标中。原方案是:改elf头中的9-16字节，但加载时出错。
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
   }else{
      printf("编译完成了note_aet.S 成功了\n");
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
      printf("编译完成了最小的main文件 成功了。\n");
   }
   return xstrdup(dest);
}

//用户链接参数中没有 -noaetinclude
static bool isNoInclude()
{
   char *ok=getenv ("GCC_AET_NO_INCLUDE");
   return ok!=NULL;
}

// 构造一个结构体存储最终结果，避免返回裸指针导致的内存泄漏
struct AetResult {
    std::string file_path;
    std::string content;
    bool found = false;
};

static char **batch_process_aet_with_pool(const char *prog,char **ld_argv, const char *atsuffix,
      const std::vector<std::string>& obj_files);

// 保持内部逻辑局部化，确保线程安全
static int process_aet_thread_safe (void *data, const char *name, off_t offset, off_t length)
{
   char *noteName = ".aetprog";
   if (strcmp (name, noteName) != 0)
      return 1;
   // 使用 pair 传递 fd 和结果字符串指针
   auto *cb_data = static_cast<std::pair<int, std::string*>*> (data);
   if (length <= 0)
      return 0;
   std::vector<char> buf(length + 1, '\0');
   if (lseek (cb_data->first, offset, SEEK_SET) < 0
   || read (cb_data->first, buf.data(), length) != length){
      return 0;
   }
   *(cb_data->second) = std::string(buf.data(), length);
   return 0;
}

// 单个文件的解析函数
static AetResult extract_aet_content_safe (const std::string& obj_file_path)
{
   AetResult result;
   result.file_path = obj_file_path;

   int fd = open (obj_file_path.c_str (), O_RDONLY | O_BINARY);
   if (fd < 0)
      return result;

   const char *errmsg;
   int err;
   simple_object_read *sobj = simple_object_start_read (fd, 0, "gcc", &errmsg, &err);
   if (!sobj){
      close (fd);
      return result;
   }

   std::pair<int, std::string*> cb_data(fd, &result.content);
   simple_object_find_sections (sobj, process_aet_thread_safe, &cb_data, &err);

   if (!result.content.empty())
      result.found = true;

   simple_object_release_read (sobj);
   close (fd);
   return result;
}


class LightThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::packaged_task<AetResult()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

public:
    // 构造函数：启动与 CPU 核心数相同的线程
    LightThreadPool() {
        size_t threads = std::thread::hardware_concurrency();
        if (threads == 0)
           threads = 4; // 保底 4 线程

        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::packaged_task<AetResult()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->cv.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task(); // 执行任务
                }
            });
        }
    }

    // 提交任务到队列
    std::future<AetResult> enqueue(std::string file_path) {
        std::packaged_task<AetResult()> task([file_path]() {
            return extract_aet_content_safe(file_path); // 调用之前实现的解析函数
        });

        auto res = task.get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (!stop) tasks.push(std::move(task));
        }
        cv.notify_one();
        return res;
    }

    // 析构函数：优雅停止所有线程
    ~LightThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable())
               worker.join();
        }
    }
};


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

static char *createRealPath(char *origName)
{
   char *realName = xmalloc(PATH_MAX);
   // 获取canonicalize名
   if (realpath(origName, realName) != NULL) {
      return realName;
   } else {
      sprintf(realName,"%s",origName);
      return realName;
   }
}

/**
 * 获取note中的字符串中的类型
 */
static char* getContentValue(char *content,char *key,int force)
{
   char *type1=strstr(content,"type=1");
   if(type1){
      char *str=strstr(type1,key);
      if(!str){
         if(force){
            printf("严重错误，退出:%s\n",content);
            exit(0);
         }else{
            return NULL;
         }
      }
      str=str+strlen(key);
      char *rex=strstr(str,"\n");
      int len=strlen(str)-strlen(rex);
      char *value=xmalloc(len+1);
      memcpy(value,str,len);
      value[len]='\0';
      return value;
   }
   return (char*)0;
}

//检查有没有COMPILE_IFACE COMPILE_BLOCK、...、COMPILE_IFACE_IMPL_CHECK
//如果有存入到 types，如查action=-1,说明没有COMPILE_IFACE等要求，但进入过aet
static void getCompileType(char *content,int *types,int *onlyEnterAet)
{
   char *typeStr = getContentValue(content,"action=",true);
   int type = atoi(typeStr);
   int result=*types;
   if(type>0){
      if((type&COMPILE_IFACE) && !(result&COMPILE_IFACE)){
         result+=COMPILE_IFACE;
      }
      if((type&COMPILE_BLOCK) && !(result&COMPILE_BLOCK)){
         result+=COMPILE_BLOCK;
      }
      if((type&COMPILE_NEW) && !(result&COMPILE_NEW)){
         result+=COMPILE_NEW;
      }
      if((type&COMPILE_MTCS_LINK) && !(result&COMPILE_MTCS_LINK)){
         result+=COMPILE_MTCS_LINK;
      }

      if((type&COMPILE_IFACE_IMPL_CHECK) && !(result&COMPILE_IFACE_IMPL_CHECK)){
         result+=COMPILE_IFACE_IMPL_CHECK;
      }
   }
   if(type==-1)
      *onlyEnterAet = -1;
   *types=result;
}

#define CHECK_FILE_LIST_NAME_NEW                "aet_iface_check_list.o"
#define IFACE_IMPL_FILE_LIST_NAME_NEW           "aet_iface_impl_list.o"
#define SAVE_LIB_PARM_FILE_NEW                  "aet_collect2_lib_name.o"
#define GENERIC_BLOCK_FILE_LIST_NAME_NEW        "generic_block_index_NEW.o"  //泛型块的文件名列表
#define GENERIC_MODEL_INDEX_FILE_LIST_NAME_NEW  "generic_model_index_NEW.o"  //新建泛型对象，调用泛型函数的文件名列表
#define AET_MTCS_LINK_FILE_LIST_NAME_NEW        "mtcs_link_func_index_file_NEW.o" //保存需要链接的mtcs函数文件名

static int runThread( const std::vector<std::string>& obj_files,char **objfs,char **objvalues)
{
   LightThreadPool pool;
   std::vector<std::future<AetResult>> futures;
   futures.reserve(obj_files.size());

   // 1. 将 200 个文件作为任务一次性投入线程池
   // 线程池内部会自动根据 CPU 核心数（如 8 核或 16 核）分批并发消化
   for (const auto& file : obj_files) {
      futures.push_back(pool.enqueue(file));
   }

   // 2. 收集并汇总结果
   std::vector<AetResult> final_results;
   for (auto& fut : futures) {
      AetResult res = fut.get(); // 阻塞等待单个任务完成
      if (res.found) {
         final_results.push_back(res);
      }
   }
   if(final_results.size() ==0)
      return 0;
   int count=0;
   for (const auto& res : final_results) {
      objfs[count] = createRealPath(res.file_path.c_str());
      objvalues[count]=xstrdup(res.content.c_str());
      count++;
   }
   return count;
}

/**
 * 单线程读.o文件的section .aetprog
 */
static int singleThread( const std::vector<std::string>& obj_files,char **objfs,char **objvalues)
{
   int count=0;
   for (const auto& file : obj_files) {
      AetResult res = extract_aet_content_safe (file);
      if(res.found){
         objfs[count] = createRealPath(res.file_path.c_str());
         objvalues[count]=xstrdup(res.content.c_str());
         count++;
      }
   }
   return count;
}

//中间过程的核心功能
static char **batch_process_aet_with_pool(const char *prog,char **ld_argv, const char *atsuffix,
      const std::vector<std::string>& obj_files)
{
   int i;
   char *objfs[2000];
   char *objvalues[2000];
   int count = 0;
   if(obj_files.size()>30)
     count = runThread(obj_files,objfs,objvalues);
   else
     count = singleThread(obj_files,objfs,objvalues);
   //fprintf(stderr,"batch_process_aet_with_pool 00 %d\n",count);
   if(count==0)
      return ld_argv;

   //取第一个.o的路径作用对象路径。
   char *objectRootPath=NULL;
   for (i=0;i<count;++i) {
      objectRootPath =  getObjRootPath(objfs[i]);
      if(objectRootPath)
         break;
   }
   if(objectRootPath==NULL)
      objectRootPath=xstrdup("/temp/");
   char *gcc=c_file_name;


   //第一个业务 是否编译middlefile
   int compileType = 0;
   int enterAet= 0;
   for (i=0;i<count;++i) {
      getCompileType(objvalues[i],&compileType,&enterAet);
   }
   int usemtcs=0;
   for (i=0;i<count;++i) {
      char *value = getContentValue(objvalues[i],"usemtcs=",0);
      if(value!=NULL){
         usemtcs = 1;
         break;
      }
   }
   //fprintf(stderr,"batch_process_aet_with_pool 22 compileType:%d enterAet:%d usemtcs:%d\n",compileType,enterAet,usemtcs);
   if(compileType==0 && enterAet==0)
      return ld_argv;
   if(compileType==0 && enterAet==-1){
      //只需要加入libaet库
      //编译5.note
      char *noteAet=compileNote(gcc,objectRootPath);
      char **aetargv=createNewArgv_new(ld_argv,NULL,NULL,
              0,NULL,0,NULL,noteAet,usemtcs);
      return aetargv;
   }

   //1.生成所需要的库文件
   collectUseLibFile_new(prog,ld_argv, atsuffix,objectRootPath,usemtcs);

   //printf("cfile ---objectRootPath :%s %s compileType:%d\n",objectRootPath,c_file_name,compileType);
   //2.编译middlefile泛型信息对象都存在middlefile中
   //COMPILE_IFACE_IMPL_CHECK需要接口检查的文件名 .ifacecheck_new.o (middle_file_iface_impl_check)
   char ifacecheck[10*1024];
   memset(ifacecheck,0,10*1024);

   //COMPILE_IFACE 接口实现的字符串保存的文件名xxx.ifaceimpl_new.o(iface_impl_save)
   char ifaceimpl[10*1024];
   memset(ifaceimpl,0,10*1024);

   //泛型块保存的文件 接口实现的字符串保存的文件名xxx.block_new.o(block_mgr_save)
   char blockfiles[10*1024];
   memset(blockfiles,0,10*1024);

   //新建泛型对象的文件 接口实现的字符串保存的文件名xxx.genobj_new.o (generic_graph_save)
   char newgenfiles[10*1024];
   memset(newgenfiles,0,10*1024);

   char mtcslinkfiles[10*1024];
   memset(mtcslinkfiles,0,10*1024);

   for (i=0;i<count;++i) {
      const char *content = objvalues[i];
      char* ifaceofile=getContentValue(content,"ifaceofile=",false);
      //printf("取接口检查文件名 %s\n",ifaceofile);
      if(ifaceofile!=NULL){
         strcat(ifacecheck,ifaceofile);
         strcat(ifacecheck,"\n");
         free(ifaceofile);
      }
      char* ifaceimplfile=getContentValue(content,"ifaceimplfile=",false);
      //printf("取接口实现文件名 %s\n",ifaceimplfile);
      if(ifaceimplfile!=NULL){
         strcat(ifaceimpl,ifaceimplfile);
         strcat(ifaceimpl,"\n");
         free(ifaceimplfile);
      }

      char* blockf=getContentValue(content,"blockfile=",false);
      //printf("取泛型块实现文件名 %s\n",blockf);
      if(blockf!=NULL){
         strcat(blockfiles,blockf);
         strcat(blockfiles,"\n");
         free(blockf);
      }

      char* newgenericfile=getContentValue(content,"newgenfile=",false);
      //printf("取新建泛型对象文件名 %s\n",newgenericfile);
      if(newgenericfile!=NULL){
         strcat(newgenfiles,newgenericfile);
         strcat(newgenfiles,"\n");
         free(newgenericfile);
      }

      char* mtcslinkfile=getContentValue(content,"mtcslinkfile=",false);
      //printf("取mtcslink对象文件名 %s\n",mtcslinkfile);
      if(mtcslinkfile!=NULL){
         strcat(mtcslinkfiles,mtcslinkfile);
         strcat(mtcslinkfiles,"\n");
         free(mtcslinkfile);
      }

   }

//   printf("batch_process_aet_with_pool 接口检查 data:%s\n",ifacecheck);
//   printf("batch_process_aet_with_pool 接口实现 data:%s\n",ifaceimpl);
//   printf("batch_process_aet_with_pool 泛型块实现  data:%s\n",blockfiles);
//   printf("batch_process_aet_with_pool 新建泛型块实现  data:%s\n",newgenfiles);
//   printf("batch_process_aet_with_pool MTCSLINK实现  data:%s\n",mtcslinkfiles);

   char checkListFileName[512];
   sprintf(checkListFileName,"%s/%s",objectRootPath,CHECK_FILE_LIST_NAME_NEW);
   if(strlen(ifacecheck)>0){
     FILE *f=fopen(checkListFileName,"w");
     fwrite(ifacecheck,1,strlen(ifacecheck),f);
     fclose(f);
   }else{
      remove(checkListFileName);
   }

   char ifaceImplListFileName[512];
   sprintf(ifaceImplListFileName,"%s/%s",objectRootPath,IFACE_IMPL_FILE_LIST_NAME_NEW);
   if(strlen(ifaceimpl)>0){
     FILE *f=fopen(ifaceImplListFileName,"w");
     fwrite(ifaceimpl,1,strlen(ifaceimpl),f);
     fclose(f);
   }else{
      remove(ifaceImplListFileName);
   }

   char blockListFileName[512];
   sprintf(blockListFileName,"%s/%s",objectRootPath,GENERIC_BLOCK_FILE_LIST_NAME_NEW);
   if(strlen(blockfiles)>0){
      FILE *f=fopen(blockListFileName,"w");
      fwrite(blockfiles,1,strlen(blockfiles),f);
      fclose(f);
   }else{
      remove(blockListFileName);
   }

   char newgenListFileName[512];
   sprintf(newgenListFileName,"%s/%s",objectRootPath,GENERIC_MODEL_INDEX_FILE_LIST_NAME_NEW);
   if(strlen(newgenfiles)>0){
      FILE *f=fopen(newgenListFileName,"w");
      fwrite(newgenfiles,1,strlen(newgenfiles),f);
      fclose(f);
   }else{
      remove(newgenListFileName);
   }

   char mtcslinkListFileName[512];
   sprintf(mtcslinkListFileName,"%s/%s",objectRootPath,AET_MTCS_LINK_FILE_LIST_NAME_NEW);
   if(strlen(mtcslinkfiles)>0){
      FILE *f=fopen(mtcslinkListFileName,"w");
      fwrite(mtcslinkfiles,1,strlen(mtcslinkfiles),f);
      fclose(f);
   }else{
      remove(mtcslinkListFileName);
   }

   //-Daetlib -Daetchecklist -Daetifaceimpllist -Daetblocklist -Daetnewgenlist。-Daetmtcslinklis 只有gcc.cc使用
   //-Daetlib = GCC_AET_LIB_PATH (aetlib.c)
   //-Daetchecklist = GCC_AET_CHECK_LIST_PATH
   char libparams[255];
   sprintf(libparams,"-Daetlib%s/%s",objectRootPath,SAVE_LIB_PARM_FILE_NEW);
   char checkListFileParam[255];
   sprintf(checkListFileParam,"-Daetchecklist%s",strlen(ifacecheck)>0?checkListFileName:"");
   char ifaceImplListFileParam[255];
   sprintf(ifaceImplListFileParam,"-Daetifaceimpllist%s",strlen(ifaceimpl)>0?ifaceImplListFileName:"");
   char blockListFileParam[255];
   sprintf(blockListFileParam,"-Daetblocklist%s",strlen(blockfiles)>0?blockListFileName:"");
   char newgenListFileParam[255];
   sprintf(newgenListFileParam,"-Daetnewgenlist%s",strlen(newgenfiles)>0?newgenListFileName:"");
   char mtcslinkListFileParam[255];
   sprintf(mtcslinkListFileParam,"-Daetmtcslinklist%s",strlen(mtcslinkfiles)>0?mtcslinkListFileName:"");

   char *argv[6];
   argv[0]=libparams;
   argv[1]=checkListFileParam;
   argv[2]=ifaceImplListFileParam;
   argv[3]=blockListFileParam;
   argv[4]=newgenListFileParam;
   argv[5]=mtcslinkListFileParam;
   //编译1.temp_func_track_45_NEW.c
   char *middleFileObj=compileMiddleFile_new(gcc,objectRootPath,compileType,argv,6);
   int genericOutputCount=0;
   //在编译temp_func_track_45时，处理泛型类时，会保存块信息到文件blockListFileName+.o中
   //在这里传给temp_func_track_45.c的有泛型信息的文件名列表是通过
   //-Daetblocklistxxx
   //  -->setAetArgv(gcc.cc)
   //     -->block_mgr_ready(blockmgr.c)
   //        -->generic_code_create_block_codes(genericcode.h)
   //         重点在generic_code_create_block_codes会把blockListFileName变成blockListFileName+.0用
   //         来保存块信息
   //编译2.temp_func_track_45_NEW.c
   char **genericOutputFiles=compileGeneric_new(gcc,objectRootPath,blockListFileName,&genericOutputCount,
         objfs,objvalues,count);

   //编译3.接口文件，在编译temp_func_track_45_NEW.c时，调用
   //class_parser_goto(classparser.c)
   //  -->iface_impl_compile_ready(ifaceimpl.c)
   //      -->createCFileSource_new(ifaceimpl.c) 生成接口实现的.c代码并保存来自.o文件
   // 例如/home/sns/workspace/ai/pc-build/debug/_RandomGenerator_2962277235__impl_iface.c\
   //$#@/home/sns/workspace/ai/pc-build/debug/ai0.o
   //$#@后是依赖的对象文件 该内容保存在文件ifaceImplListFileName+.o中
   int ifaceImplCount = 0;
   char **ifaceimplsObjs= compileIface_new(gcc,objectRootPath,&ifaceImplCount,ifaceImplListFileName,
         objfs,objvalues,count);
   //编译4.
   char *mtcsLinkObj=compileLinkLibDevice_new(gcc,objectRootPath,mtcslinkListFileName);
   //编译5.note
   char *noteAet=compileNote(gcc,objectRootPath);
   printf("链接准备工作完成 -- 44\n");
   //最后一步是生成新的链接参数列表。
   char **aetargv=createNewArgv_new(ld_argv,middleFileObj,mtcsLinkObj,
         ifaceImplCount,ifaceimplsObjs,genericOutputCount,genericOutputFiles,noteAet,usemtcs);
   printf("链接准备工作完成 -- 55\n");
   return aetargv;
}

/**
 *
 * 下面代码模拟链接，获取所有库并保存在文件 SAVE_LIB_PARM_FILE (aet_collect2_ld_lib_name.o)
 * 在编译middlefile.c前，打开库文件 SAVE_LIB_PARM_FILE 读取它的内容
 */
static void collectUseLibFile_new(const char *prog,char **ld_argv,
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
   int appendCount=createAetLib_new(appends,ld_argv,argc,usemtcs);
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
         outFileName, NULL,PEX_LAST | PEX_SEARCH,  HAVE_GNU_LD && at_file_supplied, atsuffix);
   collect_wait(prog, pex);
   // 6. 清理
   for (int i = 0; i<argc+2; i++) {
      if(trace_argv[i]!=NULL){
         free(trace_argv[i]);
      }
   }
   free(trace_argv);
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
static int createAetLib_new(char **appendArgs,char **ld_argv,int argc,int usemtcs)
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

#define ADDITIONAL_MIDDLE_AET_FILE_NEW  "temp_func_track_45_NEW.c"

/**
 * 编译中间文件 temp_func_track_45.c
 * 在编译源件期间，调用middle_file_modify方法，会改变文件 temp_func_track_45.c
 * 改变原因目前有3个原因:源文件中(1)引用接口,(2)创建泛型类对象或调用泛型函数 (3)泛型类或泛型函数中有泛型块。
 * 编译该文件的过程中 1.生成全局变量 LIB_GLOBAL_VAR_NAME_PREFIX的内容。2.生成接口实现文件.c 3.生成函数块文件.c
 */
static char *compileMiddleFile_new(char *gcc,char *objectRootPath,int compileType,char **argv,int argc)
{
   char src[255];
   sprintf(src,"%s/%s",objectRootPath,ADDITIONAL_MIDDLE_AET_FILE_NEW);
   char dest[255];
   sprintf(dest,"%s/%s",objectRootPath,ADDITIONAL_MIDDLE_AET_FILE_NEW);
   int destLen=strlen(dest);
   dest[destLen-1]='o';
   char writeContent[1024];
   sprintf(writeContent,"%s %d %d\n",RID_AET_GOTO_STR,GOTO_CHECK_FUNC_DEFINE,compileType);
   fprintf(stderr,"编译 middle file:%s %s %d arg1:%s arg2:%s\n",src,dest,compileType,argv[0],argv[1]);
   //检查ADDITIONAL_MIDDLE_AET_FILE_NEW中的内容与要写入的内容是否相同，相同不写入。
   FILE *f=fopen(src,"r");
   if(f){
      char buffer[4096];
      int len=fread(buffer,1,4096,f);
      buffer[len]='\0';
      fclose(f);
      if(strcmp(buffer,writeContent)){
         FILE *fw=fopen(src,"w");
         fwrite(writeContent,1,strlen(writeContent),fw);
         fclose(fw);
      }
   }else{
      FILE *fw=fopen(src,"w");
      fwrite(writeContent,1,strlen(writeContent),fw);
      fclose(fw);
   }
   char *objectFile= compileSingleFile_new(gcc,objectRootPath,src,dest,NULL,argv,argc);
   return objectFile;
}

static char * compileSingleFile_new(char *gcc,char *objectRootPath,char *src,
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
    //printf("compileSingleFile_new action %d src:%s obj:%s\n",action,src,addObject);
    return addObject;
}

/**
 * blockFileName 泛型块.c文件
 * srcFile 源.c文件
 * oFile 源.c文件的输出文件
 * srcFile对应的编译参数存在文件 oFile+parm.o文件中。在genericinfo.c generic_info_save中写入参数
 */
static void secondCompileGeneric_new(char *blockFileName,char *srcFile,char *oFile,
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
   printf("items[argc-1] %d %s %s items[0]:%s\n",argc,items[argc-1],srcFile,items[0]);
   gcc_assert(strcmp(items[argc-1],srcFile)==0);
   printf("items[argc-1] vvvv%d %s %s\n",argc,items[argc-1],srcFile);

   char dcl[512];
   sprintf(dcl,"-Dnclcompilefile%s",blockFileName);
   argv[argc] = xstrdup(dcl);
   argv[argc+1] = (char *) 0;
   cmds[index].prog=items[0];
   cmds[index].argv=argv;
   //for(i=0;i<argc;i++)
     // printf("secondCompileGeneric data i:%d %s\n",i,argv[i]);
}

static char *getParams(char **objfiles,char **objcontents,int ofileCount,char *objfile)
{
   int i;
   for(i=0;i<ofileCount;i++){
      if(strcmp(objfiles[i],objfile)==0){
         //printf("getParams --- i:%d %s\n",i,objfile);
         char*params= getContentValue(objcontents[i],"params=",false);
         return params;
      }
   }
   return NULL;
}
/**
 * 编译泛型有关的文件
 */
static char **compileGeneric_new(char *gcc,char *objectRootPath,char *blockListFileName,int *objCount,
      char **objfiles,char **objcontents,int ofileCount)
{
   //与genericcode.c中的generic_code_create_block_codes
   //创建保存块信息的文件名方法一个，在blockListFileName追加.o
   char fileName[512];
   sprintf(fileName,"%s.o",blockListFileName);
   //printf("compileGeneric_new 00 %s exists:%d\n",fileName,file_exists(fileName));
   if(!file_exists(fileName))
      return NULL;

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
         printf("compileGeneric_new 00 -- items[2]:%s\n",items[2]);
         char *params = getParams(objfiles,objcontents,ofileCount,items[2]);
         printf("compileGeneric_new 00 编译泛型文件 源文件是--- %s %s 删除文件:%s ok:%d params:%s\n",
               items[0],items[1],items[2],ret,params);
         secondCompileGeneric_new(items[0],items[1],items[2],params,commands,n_commands++);
         oFiles[i]=items[2];
         free(params);
      }else{
         char *cFile=cFiles[i];
         char oFile[512];
         getOFileName(cFile,oFile);
         printf("compileGeneric_new 11 -- items[2]:%s\n",oFile);

         char *params = getParams(objfiles,objcontents,ofileCount,oFile);
         if(params==NULL){
            //对应的oFile对应的参数没有，找第一个
            int j;
            for(j=0;j<ofileCount;j++){
               params= getContentValue(objcontents[j],"params=",false);
               if(params!=NULL)
                  break;
            }
         }
         printf("compileGeneric_new 11 -取参数- cfile:%s ofile:%s params:%s\n",cFile,oFile,params);
         compileBlockFunc(cFile,oFile,params,commands,n_commands++);
         oFiles[i]=xstrdup(oFile);
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
      errmsg = pex_run (pexes[i], PEX_LAST | PEX_SEARCH,string, CONST_CAST (char **, commands[i].argv),NULL, NULL, &err);
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
   char suffix[256];
   sprintf(suffix,"%s",GENERIC_BLOCK_FILE_NAME);
   char **objects=(char **)xmalloc(sizeof(char *)*100);
   *objCount=getOuputFile(objectRootPath,objects,suffix);
   return objects;
}

/**
 * 编译实现接口的.c文件
 * IFACE_IMPL_LIST_FILE_NAME 记录所有需要编译的接口.c文件
 */
static char **compileIface_new(char *gcc,char *objectRootPath,int *objCount,char *implFileList,
      char **objfiles,char **objcontents,int ofileCount)
{
   char indexFileName[512];
   sprintf(indexFileName,"%s.o",implFileList);//与iface_impl_compile_ready方法中创建的文件相同
   char buffer[1024*10];
   buffer[0]='\0';//必须加否则buffer内存不可知
   readFile(indexFileName,buffer,1024*10);

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
     // printf("编译接口文件 -----: %s %s\n",temp,dependOFile);
      char *cFile=temp;
      char oFile[512];
      char *compileParm = getParams(objfiles,objcontents,ofileCount,dependOFile);
      //printf("编译接口文件 ---param--: %s %s %s\n",temp,dependOFile,compileParm);
      getOFileName(cFile,oFile);
      createCmdForIfaceCompile(cFile,oFile,compileParm,commands,n_commands++);
   }

   struct pex_obj *pexes[n_commands];
   for(i=0;i<n_commands;i++){
      pexes[i]=pex_init (0,gcc,NULL);
      if (pexes[i] == NULL){
         remove(indexFileName);
         fatal_error (input_location, "%<pex_init%> failed: %m");
      }
   }

   for (i = 0; i < n_commands; i++){
      const char *errmsg;
      int err;
      const char *string = commands[i].argv[0];
      errmsg = pex_run (pexes[i], PEX_LAST | PEX_SEARCH,string, CONST_CAST (char **, commands[i].argv),NULL, NULL, &err);
      if (errmsg != NULL){
         remove(indexFileName);
         errno = err;
         fatal_error (input_location,err ? "cannot execute %qs: %s: %m": "cannot execute %qs: %s",string, errmsg);
      }
   }
   int ok=-1;
   for (i = 0; i < n_commands; i++){
      int status=0;
      if (!pex_get_status (pexes[i], 1, &status)){
         remove(indexFileName);
         fatal_error (input_location, "failed to get exit status: %m");
      }
      pex_free (pexes[i]);
      if(status==0){
         printf("编译接口文件成功---%s\n",headImplCFiles[i]);
      }else{
         printf("编译接口文件失败---%s status:%d\n",headImplCFiles[i],status);
         ok=i;
      }
      //remove(headImplCFiles[i]);//移走.c文件
   }
   // remove(indexFileName);
   // remove(paramFileName);
   if(ok>=0)
      fatal_error (input_location, "编译接口文件失败---:%qs",headImplCFiles[ok]);

   char suffix[256];
   sprintf(suffix,"%s.o",IFACE_FILE_SUFFIX);
   char **objects=(char **)xmalloc(sizeof(char *)*100);
   *objCount=getOuputFile(objectRootPath,objects,suffix);
   return objects;
}

/**
 * 编译链接函数所在的文件,返回.o对象文件
 */
static char *compileLinkLibDevice_new(char *gcc,char *objectRootPath,char *mtcsLinkFile)
{
   //与mtcs_link_link生成的相同
   char compileFileName[255];
   sprintf(compileFileName,"%s.o",mtcsLinkFile);

   char src[512];
   sprintf(src,"%s",compileFileName);
   char dest[255];
   sprintf(dest,"%s.o",compileFileName);
   char cfile[255];
   sprintf(cfile,"%s",compileFileName);
   int len=strlen(cfile);
   cfile[len-1]='c';
   char *objectFile= compileSingleFile_new(gcc,objectRootPath,src,dest,cfile,NULL,0);
   return objectFile;
}

/**
 * 加新的.o到最终目标
 * 两类文件的.o要加入到链结器中。
 * 1.temp_func_track_45.c 生成的.o文件
 * 2.接口的实现文件。
 */
static char **createNewArgv_new(char **ld_argv,char *middleFileObj,char *mtcsLinkObj,
      int ifaceObjectCount,char **ifaceObjects,int genCount,char **genObjs,char *noteAet,int usemtcs)
{
    int argc= getArgc(ld_argv);
    int total=argc;
    int i;
    if(middleFileObj!=NULL)
        total+=1;
    if(mtcsLinkObj!=NULL)
        total+=1;
    total+=ifaceObjectCount;
    total+=genCount;
    char *appAetLibs[10];
    int appAetLibCount=createAetLib_new(appAetLibs,ld_argv,argc,usemtcs);
    total+=appAetLibCount;
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
    for(i=0;i<genCount;i++)
        argv[count++]=genObjs[i];
    //加入缺省的库libaet.so和libaet_cudao.so
    for(i=0;i<appAetLibCount;i++)
       argv[count++]=appAetLibs[i];
    argv[count++]=noteAet;
    argv[count++]=(char*)0;
    return argv;
}

static void printLdArgv(char **ld_argv,char *explain)
{
   int i=0;
   while(ld_argv[i]!=(char*)0){
      printf("输出链接参数 %s:%d %s\n",explain,i,ld_argv[i]);
      i++;
   }
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
   char *objectFiles[2000];
   memset(objectFiles,0,sizeof(objectFiles));
   int count=0;
   int i=0;
   while(ld_argv[i]!=(char*)0){
      char *arg=ld_argv[i];
      if(endswith(arg,".o"))
         objectFiles[count++]=arg;
      i++;
   }

   if (count > 0){
      std::vector<std::string> obj_files;
      obj_files.reserve (count); // 提前预留空间，效率更高
      for (i = 0; i < count; ++i)
         obj_files.emplace_back (objectFiles[i]);
      // 传入线程池函数
      ld_argv = batch_process_aet_with_pool (prog,ld_argv,atsuffix,obj_files);
   }
   //printLdArgv(ld_argv,"之后");
   return ld_argv;
}
