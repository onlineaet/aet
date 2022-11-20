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

#include "collect2.h"
#include "collect2-aix.h"
#include "collect-utils.h"
#include "diagnostic.h"
#include "demangle.h"
#include "obstack.h"
#include "intl.h"
#include "version.h"

#include "aetmicro.h"

static int compileHeaderFile(char *objectRootPath);
static int readFileList(char *basePath,char **objs);

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
      while ( s)
        {
          int len;
          len = s - remainder;
          char *item = XNEWVEC (char, len + 1);
          memset(item,0,len+1);
          strncpy(item,remainder,len);
          buffers[count++]=item;
          remainder = s + delimiter_len;
          s = strstr (remainder, delimiter);
        }
   }
  if (*string){
	  int len=strlen(remainder);
      char *item = XNEWVEC (char, len + 1);
      memset(item,0,len+1);
      strncpy(item,remainder,len);
      buffers[count++]=item;
  }
  return count;
}

/**
 * 获得GCC可执行文件和的路径
 * aet_object_path.tmp不存在。说明不需要有第二次编译。
 * 因为如果编译的是aet相关的。会生成aet_object_path.tmp文件。
 */
static int getGccAndObjectPath(char *gcc,char *objectPath)
{
	   int exists=file_exists(COMPILE_TRACK_FILE_NAME);
	   char *aetGcc=getenv ("COLLECT_GCC");
	   if(!exists){
	       printf("aet_compile getGccAndObjectPath 文件%s 不存在!\n",COMPILE_TRACK_FILE_NAME);
		   return -1;
	   }
	   FILE *fp=fopen(COMPILE_TRACK_FILE_NAME,"r");
	   if (fp == (FILE *) 0){
         printf("aet_compile getGccAndObjectPath 不能打开文件:%s errno:%d\n",COMPILE_TRACK_FILE_NAME,errno);
   	     return -1;
	   }
	   char buffer[1024];
	   int rev = fread(buffer, sizeof(char), 1024, fp);
	   fclose(fp);
	   if(rev<=0){
	       printf("aet_compile getGccAndObjectPath 文件%s，大小不正确:%d\n",COMPILE_TRACK_FILE_NAME,rev);
		   return -1;
	   }
	   buffer[rev]='\0';
	   char *items[20];
	   int length= gsplit (buffer,"\n",items,20);
	   int i;
	   if(length!=2){
	       printf("aet_compile getGccAndObjectPath 文件%s，行数应该是2,实际是:%d\n",COMPILE_TRACK_FILE_NAME,length);
		   return -1;
	   }
	   if(!strstr(items[0],"gcc")){
	        printf("aet_compile getGccAndObjectPath 文件%s，第一行是gcc,实际是:%s\n",COMPILE_TRACK_FILE_NAME,items[0]);
		   return -1;
	   }
	   sprintf(gcc,"%s",items[0]);
	   sprintf(objectPath,"%s",items[1]);
	   for(i=0;i<length;i++)
		   free(items[i]);
	   return 0;
}

static unsigned long long  getLastModified(char *file)
{
    struct stat64 sb;
    unsigned long long rv=0;
    if (stat64(file, &sb) == 0)
    {
	    rv = 1000 * sb.st_mtime;
	}
	return rv;
}

static char * compileFuncCheckFile(char *gcc,char *objectRootPath)
{
	char src[255];
    sprintf(src,"%s/%s",objectRootPath,ADDITIONAL_MIDDLE_AET_FILE);
    char dest[255];
    sprintf(dest,"%s/%s",objectRootPath,ADDITIONAL_MIDDLE_AET_FILE);
    int destLen=strlen(dest);
    dest[destLen-1]='o';
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
    if(action==2){
       struct pex_obj *pex;
	   char **real_argv = XCNEWVEC (char *, 6);
	   const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
	   argv[0] = gcc;
	   argv[1] = "-o";
	   argv[2] = dest;
	   argv[3] = "-c";
	   argv[4] = src;
	   argv[5] = (char *) 0;
	   pex = collect_execute (gcc, real_argv, NULL, NULL,PEX_LAST | PEX_SEARCH, false);
	   int ret = collect_wait (gcc, pex);
	   if (ret){
			error ("gcc returned %d exit status", ret);
			exit (ret);
	   }else{
		   printf("编译完成了temp_func_track_45 成功了\n");
	   }
    }
    char *addObject=NULL;
    if(action!=0){
    	addObject=xstrdup(dest);
    }
    printf("action is --- %d %s\n",action,addObject);
    return addObject;
}

/**
 * 加新的.o到最终目标
 * 两类文件的.o要加入到链结器中。
 * 1.temp_func_track_45.c 生成的.o文件
 * 2.接口的实现文件。
 */
static char **createNewArgv(char **ld_argv,char *additional,int ifaceObjectCount,char **ifaceObjects)
{
	 int count= getArgc(ld_argv);
	 int total=count;
	 int i;
	 if(additional!=NULL)
	     total+=1;
	 total+=ifaceObjectCount;
	 total+=1;//放NULL
	 char **real_argv = XCNEWVEC (char *, total);
	 const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
	 for(i=0;i<count;i++){
		argv[i]=ld_argv[i];
	 }
	 if(additional)
	   argv[count++]=additional;
	 for(i=0;i<ifaceObjectCount;i++){
	     argv[count++]=ifaceObjects[i];
	 }
	 argv[count++]=(char*)0;
	 return argv;
}

#define FILTER_ARGV_0 "-plugin-opt=-fresolution="

/**
 * 如果从保存库路径的文件aet_collect2_argv.tmp
 */
static void writeLibArgv(char **ld_argv,char *objectRootPath)
{
   char fileName[255];
   sprintf(fileName,"%s/%s",objectRootPath,SAVE_LIB_PARM_FILE);
   FILE *fd=fopen(fileName,"r");
   char buffer[4096];
   buffer[0]='\0';
   int rev=0;
   if(fd){
	   rev=fread(buffer,1,4096,fd);
	   fclose(fd);
	   if(rev>0)
		   buffer[rev]='\0';
   }
   if(rev>0){
      char *items[1000];
      int count= gsplit (buffer,"\n", items,1000);
      int i;
      for(i=0;i<3000;i++){
     	  if(ld_argv[i]==(char*)0)
     	    break;
      }
      if(items[count-1]==((char*)0) || strlen(items[count-1])==0)
    	  count--;
      int find=0;
      if(i==count){
          for(i=0;i<count;i++){
        	  if(strcmp(items[i],ld_argv[i])){
        		  if(!strncmp(items[i],FILTER_ARGV_0,strlen(FILTER_ARGV_0)) && !strncmp(ld_argv[i],FILTER_ARGV_0,strlen(FILTER_ARGV_0))){
                	  printf("writeLibArgv,“%s“不是库路径，过滤不需要的 :%s\n",items[i],ld_argv[i]);
        		  }else{
        		     find=1;
        		     break;
        		  }
        	  }
          }
      }
      printf("是否要写入库路径到文件aet_collect2_argv.tmp find:%d i:%d count:%d\n",find,i,count);
      if(find==0){
    	  return;
      }
   }
   fd=fopen(fileName,"w");
   int i;
   for(i=0;i<3000;i++){
	  if(ld_argv[i]==(char*)0)
		  break;
	  fputs(ld_argv[i],fd);
	  fputs("\n",fd);
   }
   fclose(fd);
}



///////////////////////--------编译泛型文件---------------------------------------


static void createCmdForGenericCompile(char *fileName,struct command *cmds,int index)
{
    int len=strlen(fileName);
    char temp[255];
    memcpy(temp,fileName,len-1);
    memcpy(temp+len-1,"tmp",3);
    temp[len+2]='\0';
    FILE *fp=fopen(temp,"r");
   	if(!fp){
   	   error ("gcc returned %s exit status", temp);
  	}
   	char buffer[4096];
   	int rev = fread(buffer, sizeof(char), 4096, fp);
   	fclose(fp);
 	if(rev<=0){
   	   error ("gcc returned %s exit status", temp);
  	}
    static char * SEPARATION ="#$%"; //与gcc.c中的一样

   	buffer[rev]='\0';
   	char *items[1024];
   	int argc=  gsplit (buffer,SEPARATION,items,1024);
   	if(items[argc-1]==NULL || !strcmp(items[argc-1],"")){
   		printf("从文件%s取出的最后一个参数是空的或长度是0 %s 参数个数:%d\n",temp,items[argc-1],argc);
   		argc--;
   	}
    char **real_argv = XCNEWVEC (char *, argc+2);
    const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
    int i;
    for(i=0;i<argc;i++){
        argv[i] = items[i];
    }
    argv[argc] = xstrdup("-Dnclcompileyes");
    argv[argc+1] = (char *) 0;
    cmds[index].prog=items[0];
    cmds[index].argv=argv;
}

static int fillFileCount(char *objectRootPath,char **items,int count)
{
    char buffer[10*1024];
    char fileName[256];
    sprintf(fileName,"%s/%s",objectRootPath,AET_NEED_SECOND_FILE_NAME);
    FILE *fp=fopen(fileName,"r");
    if(!fp){
	   return 0;
    }
    int rev = fread(buffer, sizeof(char), 10*1024, fp);
    fclose(fp);
    if(rev<=0)
	   return 0;
    buffer[rev]='\0';
    printf("第二次需要编译的文件有:fileName:%s content:%s\n",fileName,buffer);
    int length= gsplit (buffer,"\n",items,count);
	remove((const char *)fileName);//删除文件aet_need_second_compile.tmp
    return length;

}

static char *aetgcc="gcc";


/**
 * 第二次编译泛型和接口有关的文件
 */
static int compileGenericFile (char *gcc,char *objectRootPath)
{
  int i;
  int n_commands;		/* # of command.  */
  static char * SEPARATION ="#$%"; //与gcc.c中的一样
  char *oFiles[1000];
  int count=fillFileCount(objectRootPath,oFiles,1000);
  if(count==0)
	  return 0;
  struct command *commands;	/* each command buffer with above info.  */
  commands = (struct command *) alloca (count * sizeof (struct command));
  n_commands=0;
  for(i=0;i<count;i++){
 	   printf("删除需要第二次编译的泛型的.o文件。%s\n",oFiles[i]);
 	   char *fileName=oFiles[i];
 	   int fileLen=strlen(fileName);
 	   if(fileLen==0)
 		   continue;
 	   if((fileLen!=0 && fileLen<2) || fileName[fileLen-1]!='o' || fileName[fileLen-2]!='.'){
 		   error ("无效的文件名:%s", fileName);
 	   }
 	   remove((const char *)fileName);
 	   createCmdForGenericCompile(fileName,commands,n_commands++);
  }

  struct pex_obj *pexes[n_commands];
  for(i=0;i<n_commands;i++){
	  pexes[i]=pex_init (0,aetgcc,NULL);
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
	   	    printf("编译泛型文件第二次成功%s\n",oFiles[i]);
	     }else{
		   	printf("编译泛型文件第二次失败%s status:%d\n",oFiles[i],status);
		   	ok=i;
	     }
    }
    if(ok>=0){
        fatal_error (input_location, "编译泛型文件第二次失败:%qs",oFiles[ok]);
    }
    return 0;
}



//////////////////////////编译头文件-------------------------------

static void getOFileName(char *cFile,char *oFile)
{
    char *re=strstr(cFile,".c");
    int remainLen=strlen(cFile)-strlen(re);
    char remain[remainLen+1];
    memcpy(remain,cFile,remainLen);
    remain[remainLen]='\0';
    sprintf(oFile,"%s.o",remain);
}

static void createCmdForIfaceCompile(char *cFile,char *oFile,char *compileParm,struct command *cmds,int index)
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
//    for(i=argc-5;i<argc+2;i++){
//        printf("createCmdForIfaceCompile---eee-- %d %s\n",i,argv[i]);
//    }

}

/**
 * 读取两个文件的内容
 * iface_impl_index.tmp 由ifacecompile.c写入的要编译的头文件换行符分隔。
 * iface_impl_parm.tmp 编译参数，每个c文件有一个编译参数。是往文件覆盖写的。
 */
static int readIfaceFile(char *fileName,char *buffer,int size)
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

static int readFileList(char *basePath,char **objs)
{
    DIR *dir;
    struct dirent *ptr;
    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        exit(1);
    }
    int count=0;
    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 8) {   ///file
            if(strstr(ptr->d_name,IFACE_OBJECT_FILE_SUFFIX)){
                printf("aetcompile.c readFileList name:%s/%s\n",basePath,ptr->d_name);
                char *ret=xmalloc(strlen(basePath)+strlen(ptr->d_name)+2);
                sprintf(ret,"%s/%s",basePath,ptr->d_name);
                objs[count++]=ret;
            }
        }else if(ptr->d_type == 10)    ///link file
            printf("d_name:%s/%s\n",basePath,ptr->d_name);
        else if(ptr->d_type == 4) //dir
        {
         continue;
        }
    }
    closedir(dir);
    return count;
}

/**
 * IFACE_IMPL_INDEX_FILE的内容应该形式如下:
 * /home/sns/workspace/ai/pc-build/_RandomGenerator_impl_iface.c
 * /home/sns/workspace/ai/pc-build/_Abx_impl_iface.c
 * 如果是:
 * debug_RandomGenerator$#@debug$#@/home/sns/workspace/ai/src/debug/RandomGenerator.h$#@/home/sns/workspace/ai/src/debug/debug-random.c
 * 说明用引用到接口的.c文件被编译了。但不需要编译接口的三元素实现.c文件。
 */
#define SPERATOR "$#@" //与ifacefile.c中的一样。
static int compileHeaderFile(char *objectRootPath)
{
       char indexFileName[512];
       sprintf(indexFileName,"%s/%s",objectRootPath,IFACE_IMPL_INDEX_FILE);
       char buffer[1024*10];
       readIfaceFile(indexFileName,buffer,1024*10);
       if(strstr(buffer,SPERATOR)){
           printf("aetcompile.c 根据iface_impl_index.tmp内容判断，并不需要编译接口实现文件。%s\n",buffer);
           return 0;
       }
       char paramFileName[512];
       sprintf(paramFileName,"%s/%s",objectRootPath,IFACE_IMPL_PARM_FILE);
       char compileParm[4096];
       readIfaceFile(paramFileName,compileParm,4096);

       char *headImplCFiles[2048];
       int count=  gsplit (buffer,"\n",headImplCFiles,2048);
       struct command *commands;  /* each command buffer with above info.  */
       commands = (struct command *) alloca (count * sizeof (struct command));
       int   n_commands=0;
       int i;
       for(i=0;i<count;i++){
           if(strlen(headImplCFiles[i])==0)
               continue;
           char *cFile=headImplCFiles[i];
           char oFile[512];
           getOFileName(cFile,oFile);
           createCmdForIfaceCompile(cFile,oFile,compileParm,commands,n_commands++);
       }

       if(n_commands==0)
           return 0;

       struct pex_obj *pexes[n_commands];
       for(i=0;i<n_commands;i++){
           pexes[i]=pex_init (0,aetgcc,NULL);
           if (pexes[i] == NULL){
             remove(indexFileName);
             remove(paramFileName);
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
               remove(paramFileName);
               errno = err;
               fatal_error (input_location,err ? "cannot execute %qs: %s: %m": "cannot execute %qs: %s",string, errmsg);
           }
        }
        int ok=-1;
        for (i = 0; i < n_commands; i++){
              int status=0;
              if (!pex_get_status (pexes[i], 1, &status)){
                  remove(indexFileName);
                  remove(paramFileName);
                  fatal_error (input_location, "failed to get exit status: %m");
              }
              pex_free (pexes[i]);
              if(status==0){
                 printf("编译接口文件成功%s\n",headImplCFiles[i]);
              }else{
                 printf("编译接口文件失败%s status:%d\n",headImplCFiles[i],status);
                 ok=i;
              }
              remove(headImplCFiles[i]);//移走.c文件
         }
         remove(indexFileName);
         remove(paramFileName);
         if(ok>=0){
             fatal_error (input_location, "编译接口文件失败:%qs",headImplCFiles[ok]);
         }
         return 0;
}

///////////////////////结束编译头文件---------------------------------


/**
 * 被collect2.c的do_link调用
 * 判断是不是编译aet
 * 写入库信息到文件
 * 编译类方法检查
 * 第二次编译泛型相关的文件
 */
char **aet_compile(char **ld_argv)
{
     char gcc[256];
     char objectRootPath[256];
     int re=getGccAndObjectPath(gcc,objectRootPath);
     if(re==0){
          //printf("aet_compile 把编译参数中有关库路径写入到文件 gcc:%s objectRootPath:%s :file:%s\n",gcc,objectRootPath,SAVE_LIB_PARM_FILE);
          writeLibArgv(ld_argv,objectRootPath);//把库信息写入文件
          //printf("aet_compilexxx 把编译参数中有关库路径写入到文件 gcc:%s objectRootPath:%s :file:%s\n",gcc,objectRootPath,SAVE_LIB_PARM_FILE);
          char *addObject=compileFuncCheckFile(gcc,objectRootPath);
         // printf("aet_compilexxxyyyyy 把编译参数中有关库路径写入到文件 gcc:%s objectRootPath:%s :file:%s\n",gcc,objectRootPath,SAVE_LIB_PARM_FILE);
          compileGenericFile(gcc,objectRootPath);
          compileHeaderFile(objectRootPath);
          char *ifaceObjecs[1000];
          int ifaceObjectCount=readFileList(objectRootPath,ifaceObjecs);
          printf("aet_compile  aet的跟踪文件 %s  %s %s\n",COMPILE_TRACK_FILE_NAME,gcc,addObject);
          char **aetargv=createNewArgv(ld_argv,addObject,ifaceObjectCount,ifaceObjecs);
          return aetargv;
     }
     return ld_argv;
}
