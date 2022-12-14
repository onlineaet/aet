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
 * ??????GCC???????????????????????????
 * aet_object_path.tmp????????????????????????????????????????????????
 * ????????????????????????aet?????????????????????aet_object_path.tmp?????????
 */
static int getGccAndObjectPath(char *gcc,char *objectPath)
{
	   int exists=file_exists(COMPILE_TRACK_FILE_NAME);
	   char *aetGcc=getenv ("COLLECT_GCC");
	   if(!exists){
	       printf("aet_compile getGccAndObjectPath ??????%s ?????????!\n",COMPILE_TRACK_FILE_NAME);
		   return -1;
	   }
	   FILE *fp=fopen(COMPILE_TRACK_FILE_NAME,"r");
	   if (fp == (FILE *) 0){
         printf("aet_compile getGccAndObjectPath ??????????????????:%s errno:%d\n",COMPILE_TRACK_FILE_NAME,errno);
   	     return -1;
	   }
	   char buffer[1024];
	   int rev = fread(buffer, sizeof(char), 1024, fp);
	   fclose(fp);
	   if(rev<=0){
	       printf("aet_compile getGccAndObjectPath ??????%s??????????????????:%d\n",COMPILE_TRACK_FILE_NAME,rev);
		   return -1;
	   }
	   buffer[rev]='\0';
	   char *items[20];
	   int length= gsplit (buffer,"\n",items,20);
	   int i;
	   if(length!=2){
	       printf("aet_compile getGccAndObjectPath ??????%s??????????????????2,?????????:%d\n",COMPILE_TRACK_FILE_NAME,length);
		   return -1;
	   }
	   if(!strstr(items[0],"gcc")){
	        printf("aet_compile getGccAndObjectPath ??????%s???????????????gcc,?????????:%s\n",COMPILE_TRACK_FILE_NAME,items[0]);
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
    int action=0;//0 .?????? ?????? 1.???????????????lib??????2 ??????
    if(!fsrc && fdest){
    	//??????????????????.o???ld???
    	action=0;
    }else if(fsrc && !fdest){
    	//???????????? ???.o???ld???
    	action=2;
    }else if(fsrc && fdest){
    	//?????????,????????????
		unsigned long long st= getLastModified(src);
		unsigned long long dt= getLastModified(dest);
		if(st>dt){
			//????????? ???.o???ld???
	    	action=2;
		}else{
			//????????? ???.o???ld???
	    	action=1;
		}
    }else{
    	//????????????,??????????????????.o???ld???
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
		   printf("???????????????temp_func_track_45 ?????????\n");
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
 * ?????????.o???????????????
 * ???????????????.o???????????????????????????
 * 1.temp_func_track_45.c ?????????.o??????
 * 2.????????????????????????
 */
static char **createNewArgv(char **ld_argv,char *additional,int ifaceObjectCount,char **ifaceObjects)
{
	 int count= getArgc(ld_argv);
	 int total=count;
	 int i;
	 if(additional!=NULL)
	     total+=1;
	 total+=ifaceObjectCount;
	 total+=1;//???NULL
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
 * ?????????????????????????????????aet_collect2_argv.tmp
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
                	  printf("writeLibArgv,???%s??????????????????????????????????????? :%s\n",items[i],ld_argv[i]);
        		  }else{
        		     find=1;
        		     break;
        		  }
        	  }
          }
      }
      printf("?????????????????????????????????aet_collect2_argv.tmp find:%d i:%d count:%d\n",find,i,count);
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



///////////////////////--------??????????????????---------------------------------------


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
    static char * SEPARATION ="#$%"; //???gcc.c????????????

   	buffer[rev]='\0';
   	char *items[1024];
   	int argc=  gsplit (buffer,SEPARATION,items,1024);
   	if(items[argc-1]==NULL || !strcmp(items[argc-1],"")){
   		printf("?????????%s????????????????????????????????????????????????0 %s ????????????:%d\n",temp,items[argc-1],argc);
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
    printf("?????????????????????????????????:fileName:%s content:%s\n",fileName,buffer);
    int length= gsplit (buffer,"\n",items,count);
	remove((const char *)fileName);//????????????aet_need_second_compile.tmp
    return length;

}

static char *aetgcc="gcc";


/**
 * ?????????????????????????????????????????????
 */
static int compileGenericFile (char *gcc,char *objectRootPath)
{
  int i;
  int n_commands;		/* # of command.  */
  static char * SEPARATION ="#$%"; //???gcc.c????????????
  char *oFiles[1000];
  int count=fillFileCount(objectRootPath,oFiles,1000);
  if(count==0)
	  return 0;
  struct command *commands;	/* each command buffer with above info.  */
  commands = (struct command *) alloca (count * sizeof (struct command));
  n_commands=0;
  for(i=0;i<count;i++){
 	   printf("???????????????????????????????????????.o?????????%s\n",oFiles[i]);
 	   char *fileName=oFiles[i];
 	   int fileLen=strlen(fileName);
 	   if(fileLen==0)
 		   continue;
 	   if((fileLen!=0 && fileLen<2) || fileName[fileLen-1]!='o' || fileName[fileLen-2]!='.'){
 		   error ("??????????????????:%s", fileName);
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
	   	    printf("?????????????????????????????????%s\n",oFiles[i]);
	     }else{
		   	printf("?????????????????????????????????%s status:%d\n",oFiles[i],status);
		   	ok=i;
	     }
    }
    if(ok>=0){
        fatal_error (input_location, "?????????????????????????????????:%qs",oFiles[ok]);
    }
    return 0;
}



//////////////////////////???????????????-------------------------------

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
    static char * SEPARATION ="#$%"; //???gcc.c????????????
    char *items[1024];
    int argc=  gsplit (compileParm,SEPARATION,items,1024);
    if(items[argc-1]==NULL || !strcmp(items[argc-1],"")){
        //printf("???compileParm????????????????????????????????????????????????0 %s ????????????:%d\n",items[argc-1],argc);
        argc--;
    }
    char **real_argv = XCNEWVEC (char *, argc+2);
    const char ** argv = CONST_CAST2 (const char **, char **,real_argv);
    int i;
    for(i=0;i<argc;i++){
        argv[i] = items[i];
    }
    argv[argc-3] =xstrdup(oFile);//????????????????????? ??????-c????????? ????????????-c,??????????????????
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
 * ???????????????????????????
 * iface_impl_index.tmp ???ifacecompile.c????????????????????????????????????????????????
 * iface_impl_parm.tmp ?????????????????????c?????????????????????????????????????????????????????????
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
 * IFACE_IMPL_INDEX_FILE???????????????????????????:
 * /home/sns/workspace/ai/pc-build/_RandomGenerator_impl_iface.c
 * /home/sns/workspace/ai/pc-build/_Abx_impl_iface.c
 * ?????????:
 * debug_RandomGenerator$#@debug$#@/home/sns/workspace/ai/src/debug/RandomGenerator.h$#@/home/sns/workspace/ai/src/debug/debug-random.c
 * ???????????????????????????.c???????????????????????????????????????????????????????????????.c?????????
 */
#define SPERATOR "$#@" //???ifacefile.c???????????????
static int compileHeaderFile(char *objectRootPath)
{
       char indexFileName[512];
       sprintf(indexFileName,"%s/%s",objectRootPath,IFACE_IMPL_INDEX_FILE);
       char buffer[1024*10];
       readIfaceFile(indexFileName,buffer,1024*10);
       if(strstr(buffer,SPERATOR)){
           printf("aetcompile.c ??????iface_impl_index.tmp??????????????????????????????????????????????????????%s\n",buffer);
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
                 printf("????????????????????????%s\n",headImplCFiles[i]);
              }else{
                 printf("????????????????????????%s status:%d\n",headImplCFiles[i],status);
                 ok=i;
              }
              remove(headImplCFiles[i]);//??????.c??????
         }
         remove(indexFileName);
         remove(paramFileName);
         if(ok>=0){
             fatal_error (input_location, "????????????????????????:%qs",headImplCFiles[ok]);
         }
         return 0;
}

///////////////////////?????????????????????---------------------------------


/**
 * ???collect2.c???do_link??????
 * ?????????????????????aet
 * ????????????????????????
 * ?????????????????????
 * ????????????????????????????????????
 */
char **aet_compile(char **ld_argv)
{
     char gcc[256];
     char objectRootPath[256];
     int re=getGccAndObjectPath(gcc,objectRootPath);
     if(re==0){
          //printf("aet_compile ???????????????????????????????????????????????? gcc:%s objectRootPath:%s :file:%s\n",gcc,objectRootPath,SAVE_LIB_PARM_FILE);
          writeLibArgv(ld_argv,objectRootPath);//????????????????????????
          //printf("aet_compilexxx ???????????????????????????????????????????????? gcc:%s objectRootPath:%s :file:%s\n",gcc,objectRootPath,SAVE_LIB_PARM_FILE);
          char *addObject=compileFuncCheckFile(gcc,objectRootPath);
         // printf("aet_compilexxxyyyyy ???????????????????????????????????????????????? gcc:%s objectRootPath:%s :file:%s\n",gcc,objectRootPath,SAVE_LIB_PARM_FILE);
          compileGenericFile(gcc,objectRootPath);
          compileHeaderFile(objectRootPath);
          char *ifaceObjecs[1000];
          int ifaceObjectCount=readFileList(objectRootPath,ifaceObjecs);
          printf("aet_compile  aet??????????????? %s  %s %s\n",COMPILE_TRACK_FILE_NAME,gcc,addObject);
          char **aetargv=createNewArgv(ld_argv,addObject,ifaceObjectCount,ifaceObjecs);
          return aetargv;
     }
     return ld_argv;
}
