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
#include "opts.h"

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"

#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "classutil.h"
#include "genericinfo.h"
#include "genericutil.h"
#include "funcmgr.h"
#include "makefileparm.h"

#define FIELD_SPERATOR  "!zclei@#_&"

static void genericInfoInit(GenericInfo *self)
{
	self->className=NULL;
	self->genStructDecl=NULL;
	self->blocksCount=0;
}

static tree  getField(GenericInfo *self,char *name)
{
    tree chain;
	tree record=TREE_TYPE(self->genStructDecl);
    tree fieldList=TYPE_FIELDS(record);
	for (chain = fieldList; chain; chain = DECL_CHAIN (chain)){
        tree id=DECL_NAME(chain);
        char *fn=IDENTIFIER_POINTER(id);
        if(strcmp(fn,name)==0)
        	return chain;
	}
	return NULL_TREE;
}

/**
 * 加泛型块到GenericInfo
 * inGenericFunc 块是否在泛型函数内
 */
GenericBlock *generic_info_add_block(GenericInfo *self,tree lhs,vec<tree, va_gc> *exprlist,char *body,
      char *belongFunc,nboolean inGenericFunc)
{
    GenericBlock  *gblock=generic_block_new(self->className,belongFunc,inGenericFunc,self->blocksCount);
    generic_block_set_return_type(gblock,lhs);
    generic_block_set_parm(gblock,exprlist);
    generic_block_set_body(gblock,body);
    generic_block_create_type_decl(gblock,lhs,exprlist);
    generic_block_create_call(gblock,lhs,exprlist);
    self->blocks[self->blocksCount++]=gblock;
   // printf("generic_info_add_block --- %s %d %d\n",self->className->sysName,self->blocksCount,gblock->index);
    if(self->blocksCount>=AET_MAX_GENERIC_BLOCKS){
    	n_error("在一个类中泛型块不能超过:%d,类名:%s 泛型块数:%d",MAX_GEN_BLOCKS,self->className->sysName,self->blocksCount);
    	return NULL;
    }
    return gblock;
}

int  generic_info_get_block_count(GenericInfo *self)
{
	return self->blocksCount;
}


nboolean  generic_info_same(GenericInfo *self,ClassName *className)
{
	return strcmp(self->className->sysName,className->sysName)==0;
}


GenericBlock *generic_info_get_block(GenericInfo *self,char *name)
{
	int i;
	for(i=0;i<self->blocksCount;i++){
		GenericBlock *block=self->blocks[i];
		if(strcmp(block->name,name)==0)
			return block;
	}
	return NULL;
}

GenericBlock *generic_info_get_block_by_index(GenericInfo *self,int index)
{
	return self->blocks[index];
}

tree  generic_info_get_field(GenericInfo *self,char *name)
{
	tree field=getField(self,name);
	return field;
}


/**
 * 获取的字符串格式：
 * 类名+FIELD_SPERATOR+块数量+FIELD_SPERATOR+块1+FIELD_SPERATOR+...
 */
char *generic_info_save00(GenericInfo *self)
{
    int i;
    printf("generic_info_save ---- %d\n",self->blocksCount);
    if(self->blocksCount==0)
    	return NULL;
    NString *codes=n_string_new("");
	n_string_append_printf(codes,"%s%s%d%s",self->className->sysName,FIELD_SPERATOR,self->blocksCount,FIELD_SPERATOR);
    for(i=0;i<self->blocksCount;i++){
    	GenericBlock *item=self->blocks[i];
    	char *bc=generic_block_create_save_codes(item);
    	n_string_append(codes,bc);
    	if(i<self->blocksCount-1)
        	n_string_append(codes,FIELD_SPERATOR);
    	n_free(bc);
    }
    char *result=n_strdup(codes->str);
    n_string_free(codes,TRUE);
    return result;
}

static char *toAbsolutePath(char *str)
{
   char **items=n_strsplit(str,"\n",-1);
   int len=n_strv_length(items);
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<len;i++){
      char *f=items[i];
      if(!f || strlen(f)==0)
         continue;
      if(f[0]=='0'){
         char *ff=f+1;
         n_string_append_printf(codes,"#include <%s>\n", ff);
      }else{
         char *ff=f+1;
         NFile *compileFile=n_file_new(in_fnames[0]);
         NFile *nfile=n_file_new_by_parent(n_file_get_parent_file(compileFile),ff);
         NFile *canonicalFile=n_file_get_canonical_file(nfile);
         char *canonicalPath=n_file_get_absolute_path(canonicalFile);
       //  printf("toAbsolutePath -- %s %s %s %s\n",ff,n_file_get_path(nfile),n_file_get_path(canonicalFile),canonicalPath);
        // printf("toAbsolutePath --111 %s %s %s %s\n",
              // in_fnames[0],n_file_get_path(compileFile),n_file_get_absolute_path(compileFile),n_file_get_parent(compileFile));
         n_string_append_printf(codes,"#include \"%s\"\n", canonicalPath);
         n_file_unref(compileFile);
         n_file_unref(nfile);
         n_file_unref(canonicalFile);
      }
   }
   n_strfreev(items);
   return n_string_free(codes,FALSE);
}

#define CLASS_BLOCK_START "class_block start:"
#define CLASS_BLOCK_END   "class_block end:"

#define CLASS_INCLUDE_START "class_include start:"
#define CLASS_INCLUDE_END   "class_include end:"

char *generic_info_save(GenericInfo *self)
{
   int i;
   if(self->blocksCount==0)
      return NULL;
   NString *codes=n_string_new("");
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),self->className);
   n_string_append(codes,CLASS_BLOCK_START"\n");
   n_string_append_printf(codes,"%s\n",self->className->sysName);
   n_string_append_printf(codes,"%s\n",class_info_get_file(info));//类声明所在的文件 .h或.c
  // n_string_append_printf(codes,"%s\n",in_fnames[0]);//类声明所在的文件 .h或.c
   n_string_append_printf(codes,"%s\n",makefile_parm_get_object_file(makefile_parm_get()));//正在编译的文件的输出文件名
   n_string_append_printf(codes,"%d\n",class_info_is_generic_class(info));//类是不是泛型类
   //类声明在.c文件中，又有泛型块，该文件可以需要编译第二次。所以保存该文件的编译参数到文件中。
   if(endswith(class_info_get_file(info),".c")){
      if(strcmp(in_fnames[0],class_info_get_file(info))){
         n_error("类声明在文件:%s,正在编译的文件是:%s\n",class_info_get_file(info),in_fnames[0]);
         return NULL;
      }
      char *aetEnv=getenv ("GCC_AET_ARGV");
      if(aetEnv!=NULL){
         char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
         char  compileParmFileName[512];
         sprintf(compileParmFileName,"%s.parm.o",objfile);//aetcollect.c用到该文件名。泛型块的实现只能在该.c文件中。
         FILE *fp = fopen(compileParmFileName, "w");
         fwrite(aetEnv,1,strlen(aetEnv),fp);
         fclose(fp);
      }else{
         n_error("不是aet-gcc编译器,因为没有GCC_AET_ARGV");
      }
   }

   for(i=0;i<self->blocksCount;i++){
      GenericBlock *item=self->blocks[i];
      char *bc=generic_block_create_save_codes(item);
      n_string_append(codes,bc);
      n_free(bc);
   }
   //加入头文件
    NString *includeStr=aet_parser_get()->includeCodes;
    if(includeStr && includeStr->len>0){
       n_string_append(codes,CLASS_INCLUDE_START"\n");
       char *re=toAbsolutePath(includeStr->str);
       n_string_append(codes,re);
       n_string_append(codes,"\n");
       free(re);
       n_string_append(codes,CLASS_INCLUDE_END"\n");
    }
   //结束加头文件
   n_string_append(codes,CLASS_BLOCK_END"\n");
   return n_string_free(codes,FALSE);
}



/**
 * 从字符串读出GenericInfo;
 */
static NPtrArray *readInfo(char *buffer)
{
   NPtrArray *array=n_ptr_array_new();
   char *c=buffer;
   while(strstr(c,CLASS_BLOCK_START)){
      char *start=strstr(c,CLASS_BLOCK_START);
      //printf("r0 is :%s\n",start);
      char *n=start+strlen(CLASS_BLOCK_START)+1;//加1跳过 CLASS_BLOCK_START 后的\n号
      char *end=strstr(n,CLASS_BLOCK_END);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      n_ptr_array_add(array,ret);
      c = end+strlen(CLASS_BLOCK_END);
   }
   return array;
}

/**
 * 读
 * class_include start:
 * class_include end:
 */
static char *readInclude(char *buffer)
{
   char *c=buffer;
   if(strstr(c,CLASS_INCLUDE_START)){
      char *start=strstr(c,CLASS_INCLUDE_START);
      //printf("r0 is :%s\n",start);
      char *n=start+strlen(CLASS_INCLUDE_START)+1;//加1跳过 CLASS_BLOCK_START 后的\n号
      char *end=strstr(n,CLASS_INCLUDE_END);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      return ret;
   }
   return NULL;
}

/**
 * 从如下字符串创建出GenericInfo和GenericBlock对象。
 * class_block start:
   TFirst
   block start:
   _Z6TFirst7setdataEPN6TFirstE
   0
   void _TFirst__inner_generic_func_0(TFirst * self,int xe)
   {
      int f ;
   }
   block end:
   class_block end:
 *
 */
NPtrArray *generic_info_create_info(char *content)
{
   NPtrArray *infos=n_ptr_array_new();
   NPtrArray *array=readInfo(content);
   int i;
   for(i=0;i<array->len;i++){
      char *str=n_ptr_array_index(array,i);
      char **items=n_strsplit(str,"\n",4);//有4行 0.类 1.类声明所在文件 2.编译文件的输出文件 3.是不是泛型类
      GenericInfo *info=n_slice_new0(GenericInfo);
      ClassName *className=n_slice_new0(ClassName);
      className->sysName=n_strdup(items[0]);
      info->className=className;
      info->classDeclBelongFile=n_strdup(items[1]);
      info->oFile=n_strdup(items[2]);
      info->isGenericClass = atoi(items[3]);
     // printf("generic_info_create_info --- %s %s %s\n",items[0],items[1],items[2]);
      n_strfreev(items);
      NPtrArray *blockArray= generic_block_create_block(str);
      int j;
      for(j=0;j<blockArray->len;j++){
         info->blocks[j]=n_ptr_array_index(blockArray,j);
      }
      info->blocksCount=blockArray->len;
      info->includeStr=readInclude(str);
      n_ptr_array_unref(blockArray);
      n_ptr_array_add(infos,info);
   }
   n_ptr_array_unref(array);
   return infos;
}

GenericInfo  *generic_info_new(ClassName *className)
{
	 GenericInfo *self = n_slice_alloc0 (sizeof(GenericInfo));
	 genericInfoInit(self);
	 self->className=class_name_clone(className);
	 return self;
}



