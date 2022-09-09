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

#include "collect2.h"
#include "collect2-aix.h"
#include "collect-utils.h"
#include "diagnostic.h"
#include "demangle.h"
#include "obstack.h"
#include "intl.h"
#include "version.h"

#include "aetmicro.h"


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
    }else if(fsrc && !dest){
    	//需要编译 加.o到ld中
    	action=2;
    }else if(fsrc && dest){
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
 */
static char **createNewArgv(char **ld_argv,char *additional)
{
	 int count= getArgc(ld_argv);
	 int i;
	 char **real_argv = XCNEWVEC (char *, count+2);
	 const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
	 for(i=0;i<count;i++){
		argv[i]=ld_argv[i];
	 }
	 argv[count]=additional;
	 argv[count+1]=(char*)0;
	 return argv;
}

#define FILTER_ARGV_0 "-plugin-opt=-fresolution="

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



///////////////////////-----------------------------------------------

struct command
{
  const char *prog;		/* program name.  */
  const char **argv;		/* vector of args.  */
};

static void createCmdForSencodCompile(char *fileName,struct command *cmds,int index)
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
    argv[argc] = "-Dnclcompileyes";
    argv[argc+1] = (char *) 0;
    cmds[index].prog=items[0];
    cmds[index].argv=argv;
}

static int fillFileCount(char *objectRootPath,char **items,int count)
{
    char buffer[10*1024];
    char fileName[256];
    sprintf(fileName,"%s/%s",objectRootPath,AET_NEED_SECOND_FILE_NAME);
    printf("compileGenericFile----11 %s\n",fileName);
    FILE *fp=fopen(fileName,"r");
    if(!fp){
	   return 0;
    }
    int rev = fread(buffer, sizeof(char), 10*1024, fp);
    fclose(fp);
    if(rev<=0)
	   return 0;
    buffer[rev]='\0';
    int length= gsplit (buffer,"\n",items,count);
	remove((const char *)fileName);
    return length;

}

static char *aetgcc="gcc";



static int compileGenericOrIfaceFile (char *gcc,char *objectRootPath)
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
 	   printf("删除需要第二次编译的.o文件。%s\n",oFiles[i]);
 	   char *fileName=oFiles[i];
 	   int fileLen=strlen(fileName);
 	   if(fileLen==0)
 		   continue;
 	   if((fileLen!=0 && fileLen<2) || fileName[fileLen-1]!='o' || fileName[fileLen-2]!='.'){
 		   error ("无效的文件名:%s", fileName);
 	   }
 	   remove((const char *)fileName);
 	   createCmdForSencodCompile(fileName,commands,n_commands++);
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
	   	    printf("编译第二次成功%s\n",oFiles[i]);
	     }else{
		   	printf("编译第二次失败%s status:%d\n",oFiles[i],status);
		   	ok=i;
	     }
    }
    if(ok>=0){
        fatal_error (input_location, "编译第二次失败:%qs",oFiles[ok]);
    }
    return 0;
}

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
		  compileGenericOrIfaceFile(gcc,objectRootPath);
		  printf("aet_compile  aet的跟踪文件 %s  %s %s\n",COMPILE_TRACK_FILE_NAME,gcc,addObject);
		  if(addObject!=NULL){
			  char **aetargv=createNewArgv(ld_argv,addObject);
			  return aetargv;
		  }
	 }
     return ld_argv;
}


