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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "classmgr.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "makefileparm.h"
#include "linkfile.h"
#include <utime.h>




static void linkFileInit(LinkFile *self)
{
   self->fileName=NULL;
   self->lockFile=NULL;
}

/**
 * 加了object path
 */
static char *getSaveFile(LinkFile *self,char *name)
{
	return name;
}


int link_file_lock(LinkFile *self)
{
	  char *lockFile=getSaveFile(self,self->lockFile);
	  int fd = open(lockFile, O_RDWR | O_TRUNC|O_CREAT | O_EXCL, 0666);
	  if(fd<=0){
		  if(errno==EEXIST){
			  fd = open(lockFile, O_RDWR |O_TRUNC| O_EXCL, 0666);
		  }
		  if(fd<=0){
			n_error("打不开文件:%s error:%d %s\n",lockFile,errno,xstrerror(errno));
			return -1;
		  }
	  }
	  return fd;
}

void link_file_unlock(int fd)
{
    close(fd);
}

int link_file_read(LinkFile *self,char *buffer,int size)
{
   char *fn=getSaveFile(self,self->fileName);
   FILE *fp = fopen(fn, "r");
   int rev=0;
   if(fp){
		rev = fread(buffer, sizeof(char), size, fp);
		fclose(fp);
		if(rev>0){
			buffer[rev]='\0';
		}
	}
	return rev;
}

int link_file_write(LinkFile *self,char *content,int size)
{
	  char *saveFileName=getSaveFile(self,self->fileName);
	  FILE *fp = fopen(saveFileName, "w");
	  if(!fp){
		 if(errno==17){
		   fp = fopen(saveFileName, "r+x");
		 }
		 if(!fp){
		   n_error("打不开文件----:%s error:%d %s\n",saveFileName,errno,xstrerror(errno));
		   return errno;
		 }
	  }
	  int rev=fwrite(content,1,size,fp);
	  fclose(fp);
	  return rev;
}

/**
 * 修改文件的日期为当前时间
 */
void link_file_touch(LinkFile *self)
{
	  char *saveFileName=getSaveFile(self,self->fileName);
	  utime(saveFileName,NULL);
}

int  link_file_lock_read(LinkFile *self,char *buffer,int size)
{
	int fd=link_file_lock(self);
	int  rev=link_file_read(self,buffer,size);
	link_file_unlock(fd);
	return rev;
}

int  link_file_lock_write(LinkFile *self,char *buffer,int size)
{
	int fd=link_file_lock(self);
	int  rev=link_file_write(self,buffer,size);
	link_file_unlock(fd);
	return rev;
}

int  link_file_apppend(LinkFile *self,char *content,int size)
{
	if(size==0)
	    return 0;
	char buffer[512*1024];
	int  rev=link_file_read(self,buffer,512*1024);
	if(rev>0){
		buffer[rev]='\0';
		NString *re=n_string_new(buffer);
		n_string_append(re,content);
		link_file_write(self,re->str,re->len);
		n_string_free(re,TRUE);
	}else{
		link_file_write(self,content,size);
	}
	return size;
}

nboolean   link_file_lock_delete(LinkFile *self)
{
	  int fd=link_file_lock(self);
	  char *saveFileName=getSaveFile(self,self->fileName);
	  NFile *file=n_file_new(saveFileName);
	  nboolean re=FALSE;
	  if(n_file_exists(file)){
		 re=n_file_delete(file);
	  }
	  n_file_unref(file);
	  link_file_unlock(fd);
	  return re;
}

nint64     link_file_lock_get_create_time(LinkFile *self)
{
	      int fd=link_file_lock(self);
		  char *saveFileName=getSaveFile(self,self->fileName);
		  NFile *file=n_file_new(saveFileName);
		  if(!n_file_exists(file)){
			  return 0;
		  }
		  nint64 time=n_file_get_create_time(file);
		  n_file_unref(file);
		  link_file_unlock(fd);
		  return time;
}

char *link_file_read_text(LinkFile *self)
{
    char *fn=getSaveFile(self,self->fileName);
    FILE *fp = fopen(fn, "r");
    int rev=0;
	if(fp){
  	   NFile *f=n_file_new(fn);
  	   nuint64  size=   n_file_get_length(f);
  	   n_file_unref(f);
  	   char *buffer=(char *)n_malloc(size);
	   rev= fread(buffer, sizeof(char), 4096, fp);
  	   fclose(fp);
  	   if(rev==0){
  		   n_free(buffer);
  		   return NULL;
  	   }
  	   if(rev==size){
  			buffer[rev]='\0';
  	   }else{
  	    	n_error("读取文件出错:%s size:%llu rev:%d\n",fn,size,rev);
  	   }
  	   return buffer;
  	}
	return NULL;
}

static NPtrArray *readsecond(LinkFile *self)
{
	  NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
	  char buffer[4096];
	  int rev=link_file_read(self,buffer,4096);
	  if(rev>0){
		buffer[rev]='\0';
		nchar **items=n_strsplit(buffer,"\n",-1);
		int length= n_strv_length(items);
		int i;
		for(i=0;i<length;i++){
		   char *fn=items[i];
		   if(fn && strlen(fn)>0){
			 n_ptr_array_add(array,n_strdup(fn));
		   }
	    }
	    n_strfreev(items);
	 }
	 return array;
}

static nboolean repeatSecondCompileFile(NPtrArray *array,char *oFile)
{
	int i;
	for(i=0;i<array->len;i++){
		char  *item=(char *)n_ptr_array_index(array,i);
		if(!strcmp(item,oFile))
			return TRUE;
	}
	return FALSE;
}

void link_file_add_second_compile_file(LinkFile *self,char **oFiles,int count)
{
	 int fd=link_file_lock(self);
	 NPtrArray *array=readsecond(self);
	 int i;
	 nboolean have=FALSE;
	 for(i=0;i<count;i++){
	    if(!repeatSecondCompileFile(array,oFiles[i])){
	      n_ptr_array_add(array,n_strdup(oFiles[i]));
	      have=TRUE;
	    }
	 }
	 if(have){
		 NString *codes=n_string_new("");
		 for(i=0;i<array->len;i++){
		   n_string_append(codes,n_ptr_array_index(array,i));
		   n_string_append(codes,"\n");
		 }
		 link_file_write(self,codes->str,codes->len);
		 n_string_free(codes,TRUE);
	 }
	 n_ptr_array_unref(array);
	 link_file_unlock(fd);
}

LinkFile  *link_file_new_with(char *fileName,char *lockFile)
{
	     LinkFile *self =n_slice_alloc0 (sizeof(LinkFile));
		 linkFileInit(self);
		 self->fileName=n_strdup(fileName);
		 self->lockFile=n_strdup(lockFile);
		 return self;
}

LinkFile  *link_file_new(char *fileName)
{
	     LinkFile *self =n_slice_alloc0 (sizeof(LinkFile));
		 linkFileInit(self);
		 self->fileName=n_strdup(fileName);
		 self->lockFile=n_strdup_printf("%s_lock",fileName);
		 return self;
}





