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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "zlib.h"
#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"
#include <utime.h>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <iostream>

#include "aetutils.h"
#include "aetprinttree.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "classmgr.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "makefileparm.h"
#include "middlefile.h"
#include "classutil.h"
#include "ifaceimpl.h"
#include "genericgraph.h"
#include "blockmgr.h"
#include "genericutil.h"
#include "aetlib.h"
#include "mtcslink.h"
#include "mtcsparser.h"
#include "genericcode.h"

static void middleFileInit(MiddleFile *self)
{
   self->compileParam = NULL;
   self->arrays = NULL;
}

/**
 * 三个数据要统一否则不能解压。
 * compressBuffer 装压缩后的数据
 * len =compressBuffer数据长度
 * uncompressBuffer解压后的数据
 * uncompressLen uncompressBuffer的长度
 * 以上四个都是一样的数据 calaLen
 */
static char *compressData(char *data,int *returnLen)
{
    int dataLen=strlen(data);
    int calaLen=dataLen+10;
    char *compressBuffer =(char *)n_malloc(calaLen);
    nulong compressLen=calaLen;
    int result=compress((unsigned char*)compressBuffer, &compressLen, (unsigned char*)data, dataLen + 1);
    if (Z_OK == result){
        nulong uncompressLen =calaLen;
        char *uncompressBuffer =(char *)n_malloc(calaLen);
        result= uncompress((unsigned char*)uncompressBuffer, &uncompressLen, (unsigned char*)compressBuffer, compressLen);
        if (Z_OK == result)
        {
            if(strcmp(data,uncompressBuffer)){
                n_error("解压缩后的数据与原数据不相等。%s\n",uncompressBuffer);
            }
        }else{
            n_error("解压缩数据时出错。error:%d\n%s\n",result,data);
        }
        n_free(uncompressBuffer);
    }else{
        n_error("压缩数据时出错。error:%d\n%s\n",result,data);
    }
    int total=sizeof(int)+sizeof(int)+compressLen;
    char *newData=(char *)n_malloc(total+1);
    int cl=(int)compressLen;
    memcpy(newData,&calaLen,sizeof(int));
    memcpy(newData+sizeof(int),&cl,sizeof(int));
    memcpy(newData+2*sizeof(int),compressBuffer,compressLen);
    newData[total]='\0';
    n_free(compressBuffer);
    *returnLen=total;
    return newData;
}


static void warn_string_init (location_t loc, tree type, tree value,enum tree_code original_code)
{
  if (pedantic  && TREE_CODE (type) == ARRAY_TYPE  && TREE_CODE (value) == STRING_CST  && original_code != STRING_CST)
     warning_at(loc, OPT_Wpedantic,"array initialized from parenthesized string constant");
}

static void createGlobalGenericVar(char *varName,char *data,size_t length)
{
    location_t  loc = input_location;
    tree id=aet_utils_create_ident(varName);
    tree decl, type, init;
    type = build_array_type (char_type_node,build_index_type (size_int (length)));
    type = c_build_qualified_type (type, TYPE_QUAL_CONST);
    decl = build_decl (loc, VAR_DECL, id, type);
    DECL_EXTERNAL(decl)=0;
    TREE_PUBLIC(decl)=1;
    TREE_STATIC(decl)=1;
    decl=class_util_define_var_decl(decl,TRUE);
    init = build_string (length + 1, data);
    TREE_TYPE (init) = type;

    location_t init_loc=loc;
    warn_string_init (init_loc, TREE_TYPE (decl), init,STRING_CST);
    finish_decl (decl, init_loc, init,type, NULL_TREE);
}

/**
 * 解压数据
 */
char *middle_file_decode(char *value,int size)
{
    int calcLen=0;
    int compressLen=0;
    memcpy(&calcLen,value,sizeof(int));
    memcpy(&compressLen,value+sizeof(int),sizeof(int));
    char *compressBuffer=value+2*sizeof(int);
    nulong uncompressLen =calcLen;
    char *uncompressBuffer =(char *)n_malloc(calcLen);
    int result= uncompress(uncompressBuffer, &uncompressLen, compressBuffer, compressLen);
    if (Z_OK == result)
    {
        uncompressBuffer[uncompressLen]='\0';
    }else{
        n_error("解压缩数据时出错。error:%d\n",result);
    }
    return uncompressBuffer;
}

static int file_exists (const char *name)
{
  return access (name, R_OK) == 0;
}



MiddleFile *middle_file_get()
{
   static MiddleFile *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(MiddleFile));
      middleFileInit(singleton);
   }
   return singleton;
}

//---------------------------------------新的处理方式---------------------

void middle_file_delete_collect_file(MiddleFile *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("middle_file_delete_collect_file 是第二次编译 %s\n",in_fnames[0]);
      return;
   }
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   char newName[255];
   sprintf(newName,"%s.collect.o",objfile);
   if(file_exists(newName))
      remove(newName);
   sprintf(newName,"%s.mtcs_collect.o",objfile);
   if(file_exists(newName))
      remove(newName);
}

#define HEAD_START "HEAD_START:"
#define HEAD_END "HEAD_END:"

static void saveFile(NString *codes,nboolean useMtcs)
{
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   char newName[255];
   if(!useMtcs)
      sprintf(newName,"%s.collect.o",objfile);
   else
      sprintf(newName,"%s.mtcs_collect.o",objfile);
   FILE *fp=fopen(newName,"w");
   fwrite(codes->str,1,codes->len,fp);
   fclose(fp);
}

void middle_file_save_note(MiddleFile *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("middle_file_save_note 是第二次编译 %s,写入原aetprog。time:%llu\n",in_fnames[0]);
      return;
   }
   NString *content=n_string_new("");
   int action = 0;
   //接口检查 接口信息是在一起的，来自classmgr中的ifaceCheckCodes
   char *save=iface_impl_check(iface_impl_get());
   if(save){
      action+=COMPILE_IFACE_IMPL_CHECK;
      n_string_append(content,save);
      n_free(save);
   }
   //接口实现
   save=iface_impl_save(iface_impl_get());
   if(save){
      action+=COMPILE_IFACE;
      n_string_append(content,save);
      n_free(save);
   }

   save=block_mgr_save(block_mgr_get());
   if(save){
      action+=COMPILE_BLOCK;
      n_string_append(content,save);
      n_free(save);
   }

   save=generic_graph_save(generic_graph_get());
   if(save){
      action+=COMPILE_NEW;
      n_string_append(content,save);
      n_free(save);
   }

   MtcsParser *mtcsParser = mtcs_parser_get();
   save=mtcs_link_save(mtcsParser->mtcsLink);
   if(save){
      action+=COMPILE_MTCS_LINK;
      n_string_append(content,save);
      n_free(save);
   }

   GenericParser *genericParser = generic_parser_get();
   if(genericParser->funcWithGBBuffer){
      n_string_append(content,genericParser->funcWithGBBuffer->str);
   }
   nboolean haveMtcs = mtcs_parser_have_mtcs(mtcs_parser_get());
   if(!enter_aet && action==0 && !haveMtcs){
      n_string_free(content,TRUE);
      return;
   }
   NString *codes=n_string_new("");
   n_string_append(codes,HEAD_START);
   n_string_append(codes,"\n");
   n_string_append(codes,"type=1\n");
   n_string_append_printf(codes,"action=%d\n",action==0?-1:action);
   if(self->compileParam==NULL){
      char *aetEnv=getenv ("GCC_AET_ARGV");
      self->compileParam=n_strdup(aetEnv);
   }
   n_string_append_printf(codes,"params=%s\n",self->compileParam);
   if(haveMtcs){
      n_string_append(codes,"usemtcs=1\n");
   }
   n_string_append(codes,HEAD_END);
   n_string_append(codes,"\n");

   n_string_append(codes,content->str);
   n_string_append(codes,"\n");
   saveFile(codes,haveMtcs);
}


static NPtrArray  *readData(const char *content,char *startTag,char *endTag)
{
   char *c=content;
   NPtrArray *array=n_ptr_array_new();
     while(strstr(c,startTag)){
        char *start=strstr(c,startTag);
        //printf("r0 is :%s\n",start);
        char *n=start+strlen(startTag)+1;//加1跳过 CLASS_BLOCK_START 后的\n号
        char *end=strstr(n,endTag);
        int len=strlen(n);
        int remain=strlen(end);
        char *ret=xmalloc(len-remain+1);
        memcpy(ret,n,len-remain);
        ret[len-remain]='\0';
        n_ptr_array_add(array,ret);
        c = end+strlen(endTag);
     }
     return array;
}

typedef enum
{
   HEAD_DATA,
   IFACE_CHECK,
   IFACE_INFO,
   IFACE_IMPL,
   GENERIC_GRAPH,
   GENERIC_BLOCK,
   GENERIC_FWGB,
   MTCS_LINK,
}DataIndx;

/**
 * 一个文件一个
 */
static NPtrArray **extractData(char *content)
{
   NPtrArray *head = readData(content,HEAD_START,HEAD_END);
   if(head->len>1)
      error("头只能有一个");
   //对应 middle_file_func_check
   NPtrArray *ifaceCheck = readData(content,CLASS_IFACE_NEED_CHECK_START,CLASS_IFACE_NEED_CHECK_END);
   NPtrArray *locaIfaceInfo=readData(content,CLASS_IFACE_INFO_START,CLASS_IFACE_INFO_END);
   //对应 iface_impl_compile_ready
   NPtrArray *ifaceImpl=iface_impl_create_impl_codes(content);
   //generic_graph_ready
   NPtrArray *graphArray  = generic_graph_read(content);
   //对应 block_mgr_ready
   NPtrArray *block = generic_info_create_text(content);
   //对应 generic_parser_ready
   NPtrArray *fwgb = generic_parser_create_fwgb_text(content);
   //对应 mtcs_parser_link_func
   NPtrArray *mtcslinkArray =mtcs_link_create_array(content);

   NPtrArray **ms=xmalloc(sizeof(void*)*8);
   ms[HEAD_DATA]=head;
   ms[IFACE_CHECK]=ifaceCheck;
   ms[IFACE_INFO]=locaIfaceInfo;
   ms[IFACE_IMPL]=ifaceImpl;
   ms[GENERIC_GRAPH]=graphArray;
   ms[GENERIC_BLOCK]=block;
   ms[GENERIC_FWGB]=fwgb;
   ms[MTCS_LINK]=mtcslinkArray;
   return ms;
}

typedef struct _ThreadData
{
   int start;
   int end;
   char **fileList;
   MiddleFile *self;
}ThreadData;

static void *readFile_cb(ThreadData *data)
{
   MiddleFile *self= data->self;
   int i;
   for(i=data->start;i<data->end;++i){
      char *file=data->fileList[i];
      char buffer[1024*150];
      FILE *fp=fopen(file,"r");
      int rev=fread(buffer,1,1024*150,fp);
      buffer[rev]='\0';
      fclose(fp);
      NPtrArray **ret=(NPtrArray **)extractData(buffer);
      self->arrays[i] =(NPtrArray *)ret;
   }
   return NULL;
}

/**
 * 创建全局变量 LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX 内容是泛型类的genericinfo,泛型类的genericobj
 * 当前所在的.c文件是 temp_func_track_45.c
 */
static void createGlobalVar(MiddleFile *self,NPtrArray **arrays,int aLen,int pos)
{
   //LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX
   char *block=block_mgr_get_save(block_mgr_get());
   NString *codes=n_string_new("");
   if(block && strlen(block)>0){
      n_debug("middlefile.c createGlobalVar 00 块函数。\n%s\n",block);
      n_string_append(codes,block);
      n_string_append(codes,"\n");
   }
   //块所在函数
   char *funcWithGb=generic_parser_get_fwg_source(generic_parser_get());
   if(funcWithGb && strlen(funcWithGb)>0){
      n_debug("middlefile.c createGlobalVar 11 带泛型块的函数。\n%s\n",funcWithGb);
      n_string_append(codes,funcWithGb);
      n_string_append(codes,"\n");
      n_free(funcWithGb);
   }

   //保存接口信息
   int i,j;
   for(i=0;i<aLen;i++){
      NPtrArray **as=(NPtrArray**)arrays[i];
      NPtrArray *content=as[pos];
      if(content && content->len>0){
         for(j=0;j<content->len;j++){
            char *item=n_ptr_array_index(content,j);
            n_string_append(codes,CLASS_IFACE_INFO_START);
            n_string_append(codes,"\n");
            n_string_append(codes,item);
            n_string_append(codes,"\n");
            n_string_append(codes,CLASS_IFACE_INFO_END);
            n_string_append(codes,"\n");
         }
      }
   }

   if(codes->len<=0){
      n_string_free(codes,TRUE);
      return;
   }
   printf("middlefile.c createGlobalVar 33 全部保存的内容。\n%s\n",codes->str);
   int newDataLen=0;
   char *newData=compressData(codes->str,&newDataLen);
   char varName[255];
   nint number=class_util_get_random_number();
   sprintf(varName,"%s_%d",LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX,number>0?number:number*-1);
   createGlobalGenericVar(varName,newData,newDataLen);
}


/*
 * 创建 __aet_generic_zero
 * is_shared == true  → 生成 weak 版本（给 .so 用）
 * is_shared == false → 生成普通强符号（给可执行文件用）
 */
static void createGenericZero (bool is_shared,int maxsize)
{
  location_t loc = input_location;

  /* 1. 类型 */
  tree index_type = build_index_type (size_int (maxsize));
  tree array_type = build_array_type (char_type_node, index_type);
  array_type = build_qualified_type (array_type, TYPE_QUAL_CONST);

  /* 2. 名字（必须和报错里的完全一致） */
  tree name = get_identifier (GENERIC_ZERO_STORAGE);

  tree decl = build_decl (loc, VAR_DECL, name, array_type);

  /* 3. 属性 */
  TREE_PUBLIC (decl)     = 1;
  TREE_STATIC (decl)     = 1;
  TREE_READONLY (decl)   = 1;
  DECL_EXTERNAL (decl)   = 0;
  DECL_CONTEXT (decl)    = NULL_TREE;          // 文件作用域
  DECL_ARTIFICIAL (decl) = 1;                  // 建议加上

  if (is_shared)
    {
      DECL_WEAK (decl) = 1;
      tree attr = tree_cons (get_identifier ("weak"), NULL_TREE, NULL_TREE);
      decl_attributes (&decl, attr, 0);
    }

  /* 4. 初始化器 */
  tree init = build_constructor (array_type, NULL);
  TREE_CONSTANT (init) = 1;
  TREE_STATIC (init)   = 1;

  /* ========== 关键步骤开始 ========== */

  /* 5. 先 push 到全局作用域（非常重要！） */
  tree pushed = pushdecl (decl);     // C 前端
  // 或者：
  // tree pushed = lang_hooks.decls.pushdecl (decl);

  if (pushed != decl && pushed != NULL_TREE)
    decl = pushed;                   // 发生了合并

  /* 6. 完成声明 */
  finish_decl (decl, loc, init, array_type, NULL_TREE);

  /* 7. 强制保留 */
  TREE_USED (decl)       = 1;
  TREE_ADDRESSABLE (decl)= 1;
  DECL_PRESERVE_P (decl) = 1;
  DECL_IGNORED_P (decl)  = 0;

  /* 8. 强制交给后端 */
  rest_of_decl_compilation (decl, /*top_level=*/1, /*at_end=*/0);
}

void middle_file_collect(MiddleFile *self)
{
   char *fileName = getenv("GCC_AET_COLLECT_PATH");
   if(fileName==NULL){
      fatal_error (input_location, "GCC_AET_COLLECT_PATH 是空的");
      return ;
   }
   char buffer[1024*150];
   FILE *fp=fopen(fileName,"r");
   int rev=fread(buffer,1,1024*150,fp);
   if(rev<=0)
      fatal_error (input_location, "GCC_AET_COLLECT_PATH %s 是空的",fileName);
   buffer[rev]='\0';
   fclose(fp);

   nchar **items=n_strsplit(buffer,"\n",-1);
   int length= n_strv_length(items);
   if(items[length-1] == NULL || strlen(items[length-1])==0)
      length--;
   self->arrays=xmalloc(sizeof(void*)*length);
   //如果链接文件小于5,单线程处理，否则多线程处理
   if(length<5){
      ThreadData threadData={0,length,items,self};
      readFile_cb(&threadData);
   }else{
      size_t threads = std::thread::hardware_concurrency();
      if(threads==0)
         threads = 4;
      int avg = length/threads;
      while(avg<5 && threads>4){
         threads--;
         avg = length/threads;
      }
      int i;
      pthread_t ps[threads];
      ThreadData *td[threads];
      for(i=0;i<threads;i++){
         pthread_attr_t attr;
         int start = i*avg;
         int end = start+avg;
         if(i==threads-1)
            end = length;
         ThreadData *threadData=xmalloc(sizeof(ThreadData));
         threadData->start = start;
         threadData->end = end;
         threadData->fileList = items;
         threadData->self = self;
         td[i] = threadData;
         int ret = pthread_create (&ps[i], &attr, readFile_cb, (void *)threadData);
         if (ret == EAGAIN){
            error("创建线程出错。");
            return ;
         }
      }
      for(i=0;i<threads;i++){
         pthread_join(ps[i],NULL);
      }
      for(i=0;i<threads;i++){
         free(td[i]);
         td[i]=NULL;
      }
   }
   char  *ofile = makefile_parm_get_object_file(makefile_parm_get());
   NFile *f=n_file_new(ofile);
   NFile *parent=n_file_get_parent_file(f);
   NFile  *canonical=n_file_get_canonical_file(parent);
   const  char *objectRootPath = n_file_get_absolute_path(canonical);
   //对应 middle_file_func_check
   //printf("检查接口数据 objpath:%s\n",objectRootPath);
   iface_impl_valid(iface_impl_get(),self->arrays,length,IFACE_CHECK,IFACE_INFO);
  // printf("实现接口\n");
   //实现接口的文件列表
   iface_impl_compile_ready(iface_impl_get(),objectRootPath,self->arrays,length,IFACE_IMPL);
   //printf("泛型实现 generic_graph_ready_new %d\n",length);
   generic_graph_ready(generic_graph_get(),self->arrays,length,GENERIC_GRAPH);
  // printf("泛型实现 block_mgr_ready\n");
   block_mgr_ready(block_mgr_get(),self->arrays,length,GENERIC_BLOCK);
   //printf("泛型实现 generic_parser_ready\n");
   generic_parser_ready(generic_parser_get(),self->arrays,length,GENERIC_FWGB);
   generic_code_create_block_codes(generic_code_get(),objectRootPath);
  // printf("实现 mtcs_parser_link_func_new\n");
   mtcs_parser_link_func(mtcs_parser_get(),objectRootPath,self->arrays,length,MTCS_LINK);
   //保存接口，泛型，mtcs
   createGlobalVar(self,self->arrays,length,IFACE_INFO);
   int  max = generic_graph_get_max_generic_unit(generic_graph_get());
   char *targetTag = getenv("GCC_AET_TARGET");
   int storageSizeAtLib = aet_lib_get_generic_zero_storage_size(aet_lib_get());
   n_debug("targetTag is :%s max:%d lib:%d\n",targetTag,max,storageSizeAtLib);
   max = storageSizeAtLib>max?storageSizeAtLib:max;
   max = max>2048?max:2047;
   if(!strcmp(targetTag,"0")){
      //可执行文件
      createGenericZero(false,max);
   }else  if(!strcmp(targetTag,"1")){
      //so
      createGenericZero(true,max);
   }else{
      printf(".a文件不需要生成全局变量:%s\n",GENERIC_ZERO_STORAGE);
   }


}


