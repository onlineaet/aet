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
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "opts.h"
#include "toplev.h"
#include "mkdeps.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "c-aet.h"
#include "makefileparm.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "classparser.h"
#include "makefileparm.h"
#include "blockmgr.h"
#include "classimpl.h"
#include "genericfile.h"
#include "genericexpand.h"


static void makefileParmInit(MakefileParm *self)
{
	self->bufferFiles=n_ptr_array_new();
	self->collect2LinkFile=NULL;
	self->objectPath=NULL;
	self->objectFile=NULL;
}

static char *getRootObjectPathOrObjectFile(char *src,char *dest,nboolean needRootPath)
{
	if(dest==NULL)
		return NULL;
	int len=strlen(dest);
	if(len<=2){
		printf("长度小于2 %s\n",dest);
		return NULL;
	}
	if(dest[len-1]!='o' || dest[len-2]!='.'){
		return NULL;
	}
	if(dest[0]=='-'){
		printf("第一个字符是- %s\n",dest);
		return NULL;
    }
	NFile *sfile=n_file_new(src);
	NFile *dfile=n_file_new(dest);
	if(sfile==NULL){
		printf("严重错误:没有源文件:%s\n",src);
		return NULL;
	}
	if(dfile==NULL){
		printf("目标不能生成文件:%s\n",src);
		return NULL;
	}
	char *sname=n_file_get_name(sfile);
	char *dname=n_file_get_name(dfile);
	nboolean re=strncmp(sname,dname,strlen(dname)-1);
	if(re){
		printf("文件名不同:%s %s\n",sname,dname);
		return NULL;
	}
	NFile *sroot=sfile;
	NFile *droot=dfile;
	char *p1=n_file_get_absolute_path(n_file_get_parent_file(sroot));
	char *p2=n_file_get_absolute_path(n_file_get_parent_file(dfile));
	if(!strcmp(p1,p2)){
	  n_debug("源文件与.o中同一个目录下:p1:%s sname:%s\n",p1,sname);
	  if(needRootPath){
		 return n_strdup(p1);
	  }else{
		 return n_strdup(n_file_get_absolute_path(dfile));
	  }
	}
	char *objectFile=n_strdup(n_file_get_absolute_path(dfile));
    while(TRUE){
	  NFile *sp=n_file_get_parent_file(sroot);
	  NFile *dp=n_file_get_parent_file(droot);
	  if(sp==NULL || dp==NULL){
		 // printf("parent is null:%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sroot),n_file_get_absolute_path(droot));
		  if(sp!=NULL){
			  //printf("parent sp:%s\n",n_file_get_absolute_path(sp));
			  n_file_unref(sp);
		  }
		  if(dp!=NULL){
		 	// printf("parent dp:%s\n",n_file_get_absolute_path(dp));
			  n_file_unref(dp);
		  }
		  break;
	  }


	  char *sn=n_file_get_name(sp);
	  char *dn=n_file_get_name(dp);
	  if(strcmp(sn,dn)){
		 // printf("文件不相同了:%s %s\n",sn,dn);
		  //printf("文件不相同了 is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sroot),n_file_get_absolute_path(droot));
		 // printf("文件不相同了 xx is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sp),n_file_get_absolute_path(dp));
		  char *objectRootPath=n_strdup(n_file_get_absolute_path(dp));
		  n_file_unref(sp);
		  n_file_unref(dp);
		  if(needRootPath){
			 n_free(objectFile);
		     return objectRootPath;
		  }else{
			 n_free(objectRootPath);
			 return objectFile;
		  }

	  }else{
		 // printf("文件相同了:%s %s\n",sn,dn);
		  //printf("文件相同了 is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sroot),n_file_get_absolute_path(droot));
		 // printf("文件相同了 xx is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sp),n_file_get_absolute_path(dp));

	  }
	  if(sroot){
		  n_file_unref(sroot);
	  }
	  if(sroot){
	 	  n_file_unref(droot);
	  }
	  sroot=sp;
	  droot=dp;
    }
    n_free(objectFile);
	return NULL;
}


#define MAKEFILE_PREFIX "-D nclcompile"

nboolean makefile_parm_is_second_compile(MakefileParm *self)
{
    return self->isSecondCompile;
}


char *makefile_parm_get_full_file(MakefileParm *self,char *fileName)
{
	char *objectPath=makefile_parm_get_object_root_path(self);
	char *path=n_strdup_printf("%s/%s",objectPath,fileName);
	int i;
	for(i=0;i<self->bufferFiles->len;i++){
		char *item=(char *)n_ptr_array_index(self->bufferFiles,i);
		if(strcmp(item,path)==0){
			n_free(path);
			return item;
		}
	}
	n_ptr_array_add(self->bufferFiles,path);
	return path;
}

char *makefile_parm_get_object_root_path(MakefileParm *self)
{
	return self->objectPath;
}

#define SEPARATION "#$%"

/**
 * 写入编译参数到与正在编译的同名的.tmp文件中。
 * 为第二次编译做准备。
 */
void makefile_parm_write_compile_argv(MakefileParm *self)
{
	 char *aetEnv=getenv ("GCC_AET_PARM");
	 if(aetEnv!=NULL){
		 char  *objfile=makefile_parm_get_object_file(self);
		 char *temp=n_strndup(objfile,strlen(objfile)-2);//去除.o
		 char *fileName=n_strdup_printf("%s.tmp",temp);
		 n_free(temp);
		 FILE *fp = fopen(fileName, "w");
		 fwrite(aetEnv,1,strlen(aetEnv),fp);
		 fclose(fp);
	 }else{
		 n_error("不是aet-gcc编译器,因为没有GCC_AET_PARM");
	 }
}


static void initSecondCompileParm(MakefileParm *self)
{
	int i;
	for(i=0;i<save_decoded_options_count;i++){
		struct cl_decoded_option item=save_decoded_options[i];
		if(item.arg){
			char *argv=item.arg;
		    if(strlen(argv)>strlen(MAKEFILE_PREFIX) && argv[0]=='-' && argv[1]=='D' && strstr(argv,MAKEFILE_PREFIX)){
		    	//printf("initMakefileParm00--ddd----%s\n",argv);
		    	self->isSecondCompile=TRUE;
			   break;
		   }
		}
		if(item.orig_option_with_args_text){
			char *argv=item.orig_option_with_args_text;
			if(strlen(argv)>strlen(MAKEFILE_PREFIX) && argv[0]=='-' && argv[1]=='D' && strstr(argv,MAKEFILE_PREFIX)){
		    	//printf("initMakefileParm00--dddvvvv---%s\n",argv);
		    	self->isSecondCompile=TRUE;
			    break;
			}
		}
	}
}

static void initRootObjectPath(MakefileParm *self)
{
	if(self->objectPath==NULL){
		char *fileName=in_fnames[0];
		char *dotOFile=NULL;
		int i;
		for(i=0;i<save_decoded_options_count;i++){
			struct cl_decoded_option item=save_decoded_options[i];
			dotOFile=getRootObjectPathOrObjectFile(fileName,item.arg,TRUE);
			if(dotOFile!=NULL)
			   break;
			dotOFile=getRootObjectPathOrObjectFile(fileName,item.orig_option_with_args_text,TRUE);
			if(dotOFile)
			   break;
		}
		if(dotOFile!=NULL){
			self->objectPath= dotOFile;
		}
	}
}

/**
 * 初始化两个参数参数
 * 1.编译器可执行文件
 * 2..o文件存放的根目录。
 * 例如:
 * /home/sns/gcc-10.4.0/bin/gcc
 * /home/sns/workspace/ai/pc-build
 * 并把这两个参数存入文件aet_object_path.tmp
 */
static void makefile_parm_init_argv (MakefileParm *self)
{
	if(self->collect2LinkFile==NULL){
		char lockFile[255];
		sprintf(lockFile,"%s_lock",COMPILE_TRACK_FILE_NAME);
		self->collect2LinkFile=link_file_new_with(COMPILE_TRACK_FILE_NAME,lockFile);
	}
	initSecondCompileParm(self);
	initRootObjectPath(self);
	char  *objectPath=self->objectPath;
	NString *codes=n_string_new("");
	char *aetGcc=getenv ("COLLECT_GCC");
	if(aetGcc==NULL){
		n_error("通过环境变量COLLECT_GCC找不到 gcc\n");
		return;
	}
	n_debug("makefile_parm_init_argv --- %s %s isSecondCompile:%d",aetGcc,objectPath,self->isSecondCompile);
	n_string_append_printf(codes,"%s\n",aetGcc);
	n_string_append_printf(codes,"%s",objectPath);
    link_file_lock_write(self->collect2LinkFile,codes->str,codes->len);
	n_string_free(codes,TRUE);
}

char  *makefile_parm_get_object_file(MakefileParm *self)
{
	if(self->objectFile==NULL){
		char *fileName=in_fnames[0];
		char *dotOFile=NULL;
		int i;
		for(i=0;i<save_decoded_options_count;i++){
			struct cl_decoded_option item=save_decoded_options[i];
			dotOFile=getRootObjectPathOrObjectFile(fileName,item.arg,FALSE);
			if(dotOFile!=NULL)
			   break;
			dotOFile=getRootObjectPathOrObjectFile(fileName,item.orig_option_with_args_text,FALSE);
			if(dotOFile)
			   break;
		}
		if(dotOFile!=NULL){
			self->objectFile= dotOFile;

		}
	}
	return self->objectFile;
}

/**
 * 获取依赖文件
 */
static char  *getDFile(MakefileParm *self)
{
        int i;
		for(i=0;i<save_decoded_options_count;i++){
			struct cl_decoded_option item=save_decoded_options[i];
			char *rr=item.arg;
			if(rr!=NULL){
			   int len=strlen(rr);
			   if(rr[len-1]=='d' && rr[len-2]=='.' && rr[0]!='-'){
				   return rr;
			   }
			}

		}
		return NULL;

}

/**
 * 依赖文件
 */
void makefile_parm_append_d_file(MakefileParm *self)
{
   char  *objectFile=makefile_parm_get_object_file(self);
   char fileName[255];
   sprintf(fileName,"%s",objectFile);
   int len=strlen(fileName);
   fileName[len-1]='d';
   FILE *fp = fopen(fileName, "r");
   int rev=0;
   char buffer[4096];
   if(fp){
	  rev= fread(buffer, sizeof(char), 4096, fp);
	  fclose(fp);
	  buffer[rev]='\0';
   }
   char *dfile=getDFile(self);
   if(dfile==NULL)
	   return;

   NPtrArray *sysNameArray=generic_expand_get_ref_block_class_name(generic_expand_get());
   if(sysNameArray==NULL)
	   return;
   if(sysNameArray->len==0){
	   n_ptr_array_unref(sysNameArray);
	   return;
   }
   cpp_reader *pfile=parse_in;
   int i;
   int count=0;
   char *files[sysNameArray->len];
   for(i=0;i<sysNameArray->len;i++){
	   char *sysName=n_ptr_array_index(sysNameArray,i);
	   char *fileName=generic_file_get_file_class_located(generic_file_get(),sysName);
	   if(fileName!=NULL){
		      deps_add_dep(pfile->deps,fileName);
	   }
   }
   n_ptr_array_unref(sysNameArray);
}


/**
 * 如果调用了makefileparm说明编译的是aet相关的源文件
 */
MakefileParm *makefile_parm_get()
{
	static MakefileParm *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(MakefileParm));
		 makefileParmInit(singleton);
		 makefile_parm_init_argv(singleton);
	}
	return singleton;
}






