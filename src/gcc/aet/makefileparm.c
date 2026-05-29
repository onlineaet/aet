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
#define INCLUDE_MEMORY
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
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "c-aet.h"
#include "makefileparm.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "classutil.h"
#include "classparser.h"
#include "makefileparm.h"
#include "blockmgr.h"
#include "classimpl.h"


static void makefileParmInit(MakefileParm *self)
{
	self->bufferFiles=n_ptr_array_new();
	self->objectFile=NULL;
	self->gccRootPath=NULL;
}

static char *getRootObjectPathOrObjectFile(char *src,char *dest,nboolean needRootPath)
{
	if(dest==NULL)
		return NULL;
	int len=strlen(dest);
	if(len<=2){
		//printf("长度小于2 %s\n",dest);
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
		  printf("parent is null:%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sroot),n_file_get_absolute_path(droot));
		  if(sp!=NULL){
			  printf("parent sp:%s\n",n_file_get_absolute_path(sp));
			  n_file_unref(sp);
		  }
		  if(dp!=NULL){
		 	 printf("parent dp:%s\n",n_file_get_absolute_path(dp));
			  n_file_unref(dp);
		  }
		  break;
	  }


	  char *sn=n_file_get_name(sp);
	  char *dn=n_file_get_name(dp);
	  if(strcmp(sn,dn)){
//		  printf("文件不相同了:%s %s\n",sn,dn);
//		  printf("文件不相同了 is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sroot),n_file_get_absolute_path(droot));
//		  printf("文件不相同了 xx is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sp),n_file_get_absolute_path(dp));
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
//		  printf("文件相同了:%s %s\n",sn,dn);
//		  printf("文件相同了 is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sroot),n_file_get_absolute_path(droot));
//		  printf("文件相同了 xx is :%p %p %s %s\n",sp,dp,n_file_get_absolute_path(sp),n_file_get_absolute_path(dp));
	      ;

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

#define SEPARATION "#$%"


/**
 * 获得编译文件的cpp_buffer
 */
static cpp_buffer* getCompileFileBuffer(cpp_buffer *buffer)
{
       if(buffer==NULL){
           printf("getCompileFileBuffer 00 buffer:%p %s\n",buffer,in_fnames[0]);
           return NULL;
       }
       struct _cpp_file *file=buffer->file;
       if(file==NULL){
           cpp_buffer *prev=buffer->prev;
           return getCompileFileBuffer(prev);
       }
       const char *fileName=_cpp_get_file_name (file);
       if(!strcmp(fileName,in_fnames[0])){
           return buffer;
       }
       cpp_buffer *prev=buffer->prev;
       return getCompileFileBuffer(prev);
}

/**
 * 在files.c把read_file_guts函数中的16改成64,留出空间，追加字符串
 * RID_AET_GOTO_STR,GOTO_READY_COMPILE_GENERIC_BLOCK_FUNC
 * 不需再存在prev里。
 */
static nboolean addToCppBuffer(c_parser *parser,char *tag)
{
   cpp_buffer* buffer=getCompileFileBuffer(parse_in->buffer);
   if(buffer!=NULL){
      nboolean ok=(endswith(in_fnames[0],".c") || endswith(in_fnames[0],".cpp"));
      n_debug("addToCppBuffer --xxx-- 00 %s ok:%d tag:%s len:%d\n",in_fnames[0],ok,tag,strlen(tag));
      if(ok){
         strncat(buffer->buf,tag,strlen(tag));
         buffer->rlimit=buffer->rlimit+strlen(tag);
         return TRUE;
      }
   }else{
      n_error("当前编译的内容来自缓存，不是来自文件%s。",in_fnames[0]);
   }
   return FALSE;
}

/**
 * 当编译到.c的最后，继续编块
 */
static void add_eof_tag(MakefileParm *self)
{
   c_parser *parser=aet_parser_get()->parser;
   char tag[256];
   sprintf(tag,"%s %d \n",RID_AET_GOTO_STR,GOTO_READY_COMPILE_GENERIC_BLOCK_FUNC);
   n_debug("add_eof_tag %s pid:%d %s\n",tag,getpid(),in_fnames[0]);
   addToCppBuffer(parser,tag);
}

/**
 * 如果有参数-Dnclcompile,说明是第二次编译
 */
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
		    	if(strstr(argv,"nclcompilefile")){ //aetcollect.c中加入的块函数文件。
	            char *file=strstr(argv,"nclcompilefile")+strlen("nclcompilefile");
               self->compileFileName=n_strdup(file);
		    	}
		    	self->isSecondCompile=TRUE;
			   break;
			}
		}
	}
}

/**
 * 在文件尾插入块函数代码。
 */
void    makefile_parm_insert_block_func_codes(MakefileParm *self)
{
   if(self->isSecondCompile && self->compileFileName && !self->insertBlockFunc)  {
      add_eof_tag(self);
      self->insertBlockFunc = TRUE;
   }
}

static void printOptions(struct cl_decoded_option *item,int i)
{
   printf("save_decoded_options i:%d opt:%d %d %d %d\n",i,item->opt_index,OPT_o,OPT_dumpdir,OPT_dumpbase);
   printf("\twarn_message:%s\n",item->warn_message);
   printf("\targ:%s\n",item->arg);
   printf("\torig_option_with_args_text:%s\n",item->orig_option_with_args_text);
   int j;
   for(j=0;j<item->canonical_option_num_elements;j++)
      printf("\tcanonical_option:j:%d %s\n",j,item->canonical_option[j]);
}

const char *getDotFileByDump(struct cl_decoded_option *opts,unsigned int argc) {
    const char *dumpdir = NULL;
    const char *dumpbase = NULL;

    // 遍历所有选项
    for (unsigned int i = 0; i < argc; i++) {
      // printOptions(&opts[i],i);
        switch (opts[i].opt_index) {
            case OPT_dumpdir:
                dumpdir = opts[i].arg;      // 转储目录
                break;

            case OPT_dumpbase:
                dumpbase = opts[i].arg;      // 转储基础名
                break;
        }
        if(dumpdir && dumpbase)
           break;
    }
    // 使用转储选项构建输出文件名
    if (dumpdir && dumpbase) {
        char *ret=concat(dumpdir, dumpbase, NULL);
        ret[strlen(ret)-1]='o';
        return ret;
    }
    return NULL;
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
   initSecondCompileParm(self);
}

/**
 * 获取编译文件的输出.o文件。
 */
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
      if(dotOFile!=NULL)
         self->objectFile= dotOFile;
      else{
         char *base=NULL;
         if (dump_base_name)
            base = dump_base_name;
         else if (aux_base_name)
            base = aux_base_name;
         if(base!=NULL){
            dotOFile=xmalloc(512);
            const char *dot = strrchr(base, '.');
            if (dot) {
               size_t len = dot - base;
               snprintf(dotOFile, 512, "%.*s.o", (int)len, base);
            } else {
               snprintf(dotOFile, 512, "%s.o", base);
            }
            self->objectFile= dotOFile;
         }else
            n_error("没有找到编译文件的输出.o文件,报告此错误。 %s\n",fileName);
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
   char *dfile=getDFile(self);
   if(dfile==NULL)
	   return;

   NPtrArray *sysNameArray=NULL;//generic_expand_get_ref_block_class_name(generic_expand_get());
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
	   char *fileName=NULL;//generic_file_get_file_class_located(generic_file_get(),sysName);
	   if(fileName!=NULL){
		      deps_add_dep(pfile->deps,fileName);
	   }
   }
   n_ptr_array_unref(sysNameArray);
}


static void createGccRoot(MakefileParm *self)
{
   if(self->gccRootPath==NULL){
      char exe[PATH_MAX];
      int len=readlink("/proc/self/exe", exe, PATH_MAX);
      exe[len] = '\0';
      struct cl_decoded_option item=save_decoded_options[0];
      char *first=item.arg;
      char *r=lrealpath(first);
      char *rr=lrealpath(exe);
      NFile *file=n_file_new(exe);
      while(1){
         NFile *parent = n_file_get_parent_file(file);
         char path[512];
         sprintf(path, "%s/libexec/gcc", n_file_get_absolute_path(parent));
         NFile *f=n_file_new(path);
         if(n_file_exists(f)){
            self->gccRootPath=n_strdup(n_file_get_absolute_path(parent));
            n_file_unref(f);
            n_file_unref(parent);
            break;
         }
         n_file_unref(f);
         n_file_unref(file);
         file=parent;
      }
      n_file_unref(file);
   }
}

//获取aet include路径
const char *makefile_parm_get_aet_include_path(MakefileParm *self)
{
   static char aetinclude[256];
   if(self->gccRootPath==NULL){
      createGccRoot(self);
   }
   sprintf(aetinclude,"%s/include/libaet",self->gccRootPath);
   //检查 aet.h文件是不否存在
   char head[512];
   sprintf(head,"%s/aet.h",aetinclude);
   NFile *file=n_file_new(head);
   if(!n_file_exists(file)){
      n_error("aet头文件不存在:%s\n",head);
      return NULL;
   }
   n_file_unref(file);
   return aetinclude;
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


