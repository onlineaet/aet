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

static char *getIfaceImplInfoSavedInLib(MiddleFile *self);

static void middleFileInit(MiddleFile *self)
{
   self->action = 0;
   self->compileParam = NULL;
   self->ifaceOFile = NULL;
}

/**
 * 第一次编译时，编译每个单元，当有如下情況时，会调用该方法，写入编译类型
 * 编译有接口的类
 * 编译有泛型块的类
 * 编译new 泛型类或调用泛型函数的文件
 * 写入标记到temp_func_track_45.c
 * 这是触发第二次编译的条件和编译第二次的内容导航
 * 保存在每个.o文件的最后 .aetprog section
 */
void middle_file_modify(MiddleFile *self,CompileType type)
{
   int compileType=self->action;
   n_debug("middle_file_modify --- %s type:%d\n",in_fnames[0],type);
   if(!(compileType&type)){
      compileType+=type;
   }
   if(self->compileParam==NULL){
      char *aetEnv=getenv ("GCC_AET_ARGV");
      self->compileParam=n_strdup(aetEnv);
   }
   self->action=compileType;
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
 * 创建全局变量 LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX 内容是泛型类的genericinfo,泛型类的genericobj
 * 当前所在的.c文件是 temp_func_track_45.c
 */
void middle_file_create_global_var(MiddleFile *self)
{
    //LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX
    char *genericObj=generic_graph_get_output_string(generic_graph_get());
    char *block=block_mgr_get_save(block_mgr_get());
    NString *codes=n_string_new("");
    if(genericObj && strlen(genericObj)>0){
       n_debug("middle_file_create_global_var 11 泛型对象。\n%s\n",genericObj);
       n_string_append(codes,genericObj);
       n_string_append(codes,"\n");
    }

    if(block && strlen(block)>0){
       n_debug("middle_file_create_global_var 22 块函数。\n%s\n",block);
       n_string_append(codes,block);
       n_string_append(codes,"\n");
    }
    //块所在函数
    char *funcWithGb=generic_parser_get_fwg_source(generic_parser_get());
    if(funcWithGb && strlen(funcWithGb)>0){
       n_debug("middle_file_create_global_var 33 带泛型块的函数。\n%s\n",funcWithGb);
       n_string_append(codes,funcWithGb);
       n_string_append(codes,"\n");
       n_free(funcWithGb);
    }

    char *ifaceImpl= getIfaceImplInfoSavedInLib(self);
    if(ifaceImpl && strlen(ifaceImpl)>0){
       n_debug("middle_file_create_global_var 44 接口实现。\n%s\n",ifaceImpl);
       n_string_append(codes,ifaceImpl);
       n_string_append(codes,"\n");
    }

    if(codes->len<=0){
        n_string_free(codes,TRUE);
        return;
    }
    n_debug("middlefile.c middle_file_create_global_var 55 全部保存的内容。\n%s\n",codes->str);
    int newDataLen=0;
    char *newData=compressData(codes->str,&newDataLen);
    char varName[255];
    nint number=class_util_get_random_number();
    sprintf(varName,"%s_%d",LIB_GLOBAL_GENERIC_VAR_NAME_PREFIX,number>0?number:number*-1);
    createGlobalGenericVar(varName,newData,newDataLen);
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

void middle_file_iface_impl_check(MiddleFile *self)
{
   ClassMgr *classMgr=class_mgr_get();
   NString *codes=classMgr->ifaceCheckCodes;
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   char newName[255];
   sprintf(newName,"%s.ifacecheck_new.o",objfile);
   if(codes->len==0){
      if(file_exists(newName)){
         remove(newName);//删除文件 xxx.ifacecheck.o
      }
      return;
   }
   FILE *fp=fopen(newName,"w");
   fwrite(codes->str,1,codes->len,fp);
   fclose(fp);
   self->ifaceOFile=n_strdup(newName);
   middle_file_modify(self,COMPILE_IFACE_IMPL_CHECK);
}

/**
 * 读每个文件列表:
 * 文件名格式:
 * xxx.o.funccheck.o
 * 每个文件保存的内容是编译单元中类实现的函数信息，格式如下;
 * CLASS_FUNC_INFO_START:
 * TFirst
 * TFirst:Abc:_Z07setdataEv
 * CLASS_FUNC_INFO_END:
 * CLASS_FUNC_NEED_CHECK_START:
 * Txs
 * TFirst debug_AObject
 * Abc:_Z07setdataEv
 * CLASS_FUNC_NEED_CHECK_END:
 */
static char * readLocalFile(char *localFileList)
{
   nchar **items=n_strsplit(localFileList,"\n",-1);
   int length= n_strv_length(items);
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<length;i++){
      char *fn=items[i];
      FILE *fp=fopen(fn,"r");
      if(fp){
         char buffer[1024*150];
         int rev=fread(buffer,1,1024*150,fp);
         if(rev>0){
            buffer[rev]='\0';
            n_string_append(codes,buffer);
            n_string_append(codes,"\n");
         }
         fclose(fp);
      }
   }
   n_strfreev(items);
   return n_string_free(codes,FALSE);
}

/**
 * 从字符串读出类中方法信息
 * 格式如下:
 * CLASS_FUNC_INFO_START:
 * TFirst                    类名
 * TFirst:Abc:_Z07setdataEv  实现的接口所在类+接口+接口方法 实现的接口所在类不一定就是类
 * CLASS_FUNC_INFO_END:
 * 或者
 * CLASS_FUNC_NEED_CHECK_START:
 * Txs
 * TFirst debug_AObject
 * Abc:_Z07setdataEv
 * CLASS_FUNC_NEED_CHECK_END:
 */
static NPtrArray *readClassFuncInfoOrNeedCheckInfo(char *buffer,char *startTag,char *endTag)
{
   NPtrArray *array=n_ptr_array_new_with_free_func(n_free);
   char *c=buffer;
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

/**
 * 在本项目中检查接口是否实现
 * compare参数格式:
 * TFirst:Abc:_Z07setdataEv  实现的接口所在类+接口+接口方法
 */
static nboolean checkIfaceImpl(char *compare,NPtrArray *local,char *lib)
{
   int i;
   if(local){
      int len=local->len;
      for(i=0;i<len;i++){
         char *funcInfo=n_ptr_array_index(local,i);
         if(strstr(funcInfo,compare))
            return TRUE;
      }
   }
   //库中是否有字匹配字符串 TFirst:Abc:_Z07setdataEv
   return strstr(lib,compare)?TRUE:FALSE;
}

/**
 * 获取当前项目的接口实现信息
 * 格式如下:
 * CLASS_FUNC_INFO_START:
 * TFirst                    类名
 * TFirst:Abc:_Z07setdataEv  实现的接口所在类+接口+接口方法 实现的接口所在类不一定就是类
 * CLASS_FUNC_INFO_END:
 */
static char *getIfaceImplInfoSavedInLib(MiddleFile *self)
{
   char *fileName = getenv("GCC_AET_CHECK_LIST_PATH");
   if(fileName==NULL || strlen(fileName)==0){
      n_warning("middlefile.c getIfaceImplInfoSavedInLib 不存在的文件 GCC_AET_CHECK_LIST_PATH\n");
      return NULL;
   }
   FILE *fp=fopen(fileName,"r");
   if(!fp)
      return NULL;
   char buffer[5*1024];
   int rev = fread(buffer,1,5*1024,fp);
   fclose(fp);
   if(rev<=0)
      return NULL;
   buffer[rev]='\0';
   char *content = readLocalFile(buffer);
   NString *codes=n_string_new("");
   char *c=content;
   while(strstr(c,CLASS_IFACE_INFO_START)){
      char *start=strstr(c,CLASS_IFACE_INFO_START);
      //printf("r0 is :%s\n",start);
      char *n=start+strlen(CLASS_IFACE_INFO_START)+1;//加1跳过 CLASS_BLOCK_START 后的\n号
      char *end=strstr(n,CLASS_IFACE_INFO_END);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      n_string_append(codes,ret);
      n_string_append(codes,"\n");
      free(ret);
      c = end+strlen(CLASS_IFACE_INFO_END);
   }
   return n_string_free(codes,FALSE);
}

/**
 * 映射如下字符串
 * Txs
 * TFirst debug_AObject
 * Abc:_Z07setdataEv
 */
typedef struct _NeedCheckInfo
{
   char *sysName;
   char **extendsClass;
   int extendsClassCount;
   char **checkIface;
   int checkCount;
}NeedCheckInfo;

static void freeNeedCheckInfo(NeedCheckInfo *info)
{
   if(!info)
      return;
   n_free(info->sysName);
   n_strfreev(info->extendsClass);
   int i;
   for(i=0;i<info->checkCount;i++)
      free(info->checkIface[i]);
   n_free(info->checkIface);
   n_slice_free(NeedCheckInfo,info);

}

static NeedCheckInfo *createNeedInfo(char *str)
{
    char **items=n_strsplit(str,"\n",-1);
    int checkIfaceCount=n_strv_length(items)-2;
    NeedCheckInfo *info=n_slice_new0(NeedCheckInfo);
    info->sysName=n_strdup(items[0]);
    info->extendsClass=n_strsplit(items[1]," ",-1);
    info->extendsClassCount=n_strv_length(info->extendsClass);
    int checkCount=n_strv_length(items)-2;
    info->checkIface=xmalloc(sizeof(char*)*checkCount);
    int i;
    for(i=0;i<checkCount;i++)
       info->checkIface[i]=n_strdup(items[i+2]);
    info->checkCount=checkCount;
    n_strfreev(items);
    return info;
}

static void findImpl( NeedCheckInfo *info,NPtrArray *locaIfaceInfo,char *libIfaceInfo,NString *errorCodes)
{
   int i,j;
   for(i=0;i<info->checkCount;i++){
      char *ifaceMethod=info->checkIface[i];//检查类item[0]是否实现接口方法 items[j+2]
      if(!ifaceMethod || strlen(ifaceMethod)==0)
         continue;
      nboolean find=FALSE;
      for(j=0;j<info->extendsClassCount;j++){
         char compare[512];
         sprintf(compare,"%s:%s",info->extendsClass[j],ifaceMethod);
         if(checkIfaceImpl(compare,locaIfaceInfo,libIfaceInfo)){
            find=TRUE;
            break;
         }
      }
      if(!find){
         n_string_append_printf(errorCodes, "类 %s 必须实现继承的接口方法 %s 。\n",info->sysName,ifaceMethod);
      }
   }
}

extern FILE *asm_out_file;

/*
 * 把文本按 byte 输出到汇编
 */
static void write_bytes_text (FILE *fp,const char *content)
{
   const unsigned char *p;
   if (fp == NULL)
      return;
   if (content == NULL)
      return;
   p = (const unsigned char *) content;
   while (*p){
      fprintf (fp,"\t.byte 0x%02x\n",(unsigned int)*p);
      p++;
   }
   /*
   * 字符串结束符
   */
   fprintf (fp,"\t.byte 0x00\n");
}

/*
 * 向 .s 文件追加一个文本 section
 */
static void writeNote (const char *content)
{
    FILE *fp;
    /*
     * GCC 当前汇编文件名
     */
    if (asm_file_name == NULL)
        return;
    if (content == NULL)
        return;
    if (asm_out_file == NULL)
        return;
    fp=asm_out_file;
    /*
     * 防止和上一行粘连
     */
    fprintf (fp, "\n");
    /*
     * 普通 ELF section
     *
     * 不是 NOTE
     */
    fprintf (fp,".section .aetprog,\"a\"\n");
    fprintf (fp,".align 1\n");
    /*
     * 写入文本
     */
    write_bytes_text (fp, content);
    /*
     * 回到 text section
     * 防止影响后续汇编
     */
    fprintf (fp,".text\n");
}

/**
 * 重要功能：存储每个编译文件的信息到名字叫.aetarg的section中。这些数据在aetcollect中处理。
 * 调用到该函数是从
 * toplev.cc -->mtcscompile.cc-->aetmediator.cc
 */
void middle_file_save_note(MiddleFile *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("middle_file_save_note 是第二次编译 %s,写入原aetprog。time:%llu\n",in_fnames[0]);
      char *aetprog = makefile_parm_get_aetprog(makefile_parm_get());
      if(aetprog!=NULL){
         writeNote(aetprog);
         return;
      }
   }
   if(self->action==0 && !enter_aet)
      return;
   //没有 COMPILE_IFACE COMPILE_BLOCK、...、COMPILE_IFACE_IMPL_CHECK,但引用AET对象系统。
   if(self->action==0 && enter_aet){
      NString *content=n_string_new("");
      n_string_append(content,"type=1\n");
      n_string_append(content,"action=-1\n");
      if(mtcs_parser_have_mtcs(mtcs_parser_get())){
         n_string_append(content,"usemtcs=1\n");
      }
      n_string_append(content,"end;\n");
      writeNote(content->str);
      n_string_free(content,TRUE);
      return;
   }
   n_debug("middlefile.c middle_file_save_note 11 %d %s\n",self->action,self->compileParam,self->ifaceOFile);
   NString *content=n_string_new("");
   n_string_append(content,"type=1\n");
   n_string_append_printf(content,"action=%d\n",self->action);
   n_string_append_printf(content,"params=%s\n",self->compileParam);
   if(self->ifaceOFile!=NULL){
      n_string_append_printf(content,"ifaceofile=%s\n",self->ifaceOFile);
   }
   IfaceImpl  *ifaceImpl=iface_impl_get();
   if(ifaceImpl->saveIfaceFileName)
      n_string_append_printf(content,"ifaceimplfile=%s\n",ifaceImpl->saveIfaceFileName);

   BlockMgr *blockMgr=block_mgr_get();
   if(blockMgr->blockFileName)
       n_string_append_printf(content,"blockfile=%s\n",blockMgr->blockFileName);

   GenericParser *genericParser = generic_parser_get();
   if(genericParser->funcWithGBFileName!=NULL)
      n_string_append_printf(content,"funcwithgbfile=%s\n",genericParser->funcWithGBFileName);

   GenericGraph *genericGraph=generic_graph_get();
   if(genericGraph->collectFileName)
       n_string_append_printf(content,"newgenfile=%s\n",genericGraph->collectFileName);

   MtcsLink *mtcsLink=mtcs_parser_get()->mtcsLink;
   if(mtcsLink->collectMtcsLinkFile)
       n_string_append_printf(content,"mtcslinkfile=%s\n",mtcsLink->collectMtcsLinkFile);

   if(mtcs_parser_have_mtcs(mtcs_parser_get())){
      n_string_append(content,"usemtcs=1\n");
   }
   n_string_append(content,"end;\n");
   writeNote(content->str);
   n_string_free(content,TRUE);
}

/**
 *进入这里属于编译temp_func_track_45.c 主要靠gcc.cc中传递的参数获取在aetcollect中收集的数据
 */
void middle_file_func_check(MiddleFile *self)
{
   char *fileName = getenv("GCC_AET_CHECK_LIST_PATH");
   if(fileName==NULL || strlen(fileName)==0){
      printf("middle_file_func_check path 出错了:path:%s\n",fileName);
      return;
   }
   FILE *fp=fopen(fileName,"r");
   if(!fp)
      return;
   char buffer[5*1024];
   int rev = fread(buffer,1,5*1024,fp);
   fclose(fp);
   if(rev<=0)
      return;
   buffer[rev]='\0';
   char *content = readLocalFile(buffer);
   NPtrArray *needCheckInfo=readClassFuncInfoOrNeedCheckInfo(content,
         CLASS_IFACE_NEED_CHECK_START,CLASS_IFACE_NEED_CHECK_END);
   if(needCheckInfo==NULL || needCheckInfo->len==0){
      //printf("本次编译不需要检查类接口实现 middle_file_func_check。\n");
      if(needCheckInfo)
         n_ptr_array_unref(needCheckInfo);
      n_free(content);
      return;
   }
   NPtrArray *locaIfaceInfo=readClassFuncInfoOrNeedCheckInfo(content,CLASS_IFACE_INFO_START,CLASS_IFACE_INFO_END);
   char  *libIfaceInfo = aet_lib_get_class_iface_impl_info(aet_lib_get());
   NString *errorCodes=n_string_new("");
   int errorCount=1;
   int i;
   for(i=0;i<needCheckInfo->len;i++){
      char *check=n_ptr_array_index(needCheckInfo,i);
      NeedCheckInfo *info=createNeedInfo(check);
      findImpl(info,locaIfaceInfo,libIfaceInfo,errorCodes);
      freeNeedCheckInfo(info);
   }
   if(errorCodes->len>0){
      fatal_error(0,errorCodes->str);
   }
   n_ptr_array_unref(locaIfaceInfo);
   n_ptr_array_unref(needCheckInfo);
   n_free(content);
}

void middle_file_test(MiddleFile *self,char *codes)
{
//   char *ca="i love you\n";
//   printf("middle_file_test ---\n");
//   aet_utils_add_token(parse_in,ca,strlen(ca));

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

